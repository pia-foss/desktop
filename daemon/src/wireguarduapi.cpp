// Copyright (c) 2023 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include <common/src/common.h>
#line SOURCE_FILE("wireguarduapi.cpp")

#include "wireguarduapi.h"
#include "wireguardbackend.h"
#include <unordered_map>
#include <cstdlib>
#include <type_traits>

#if defined(Q_OS_WIN)
    #include <WinSock2.h>   // htonl(), etc.
    #include <WS2tcpip.h>   // inet_pton()
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>  // htonl(), etc.; inet_pton()
#endif

Q_DECLARE_METATYPE(QLocalSocket::LocalSocketError)

namespace Uapi
{
    Q_NAMESPACE

    const QLatin1String
        // Set result keys
        errNo{"errno"}, // Note spelling to avoid "errno" macro
        // Device keys
        privateKey{"private_key"},
        listenPort{"listen_port"},
        fwmark{"fwmark"},
        replacePeers{"replace_peers"},
        // Peer keys
        publicKey{"public_key"},
        remove{"remove"},
        updateOnly{"update_only"},
        presharedKey{"preshared_key"},
        endpoint{"endpoint"},
        persistentKeepaliveInterval{"persistent_keepalive_interval"},
        replaceAllowedIps{"replace_allowed_ips"},
        // Allowed IP keys
        allowedIp{"allowed_ip"},
        // Peer keys (more)
        rxBytes{"rx_bytes"},
        txBytes{"tx_bytes"},
        lastHandshakeTimeSec{"last_handshake_time_sec"},
        lastHandshakeTimeNsec{"last_handshake_time_nsec"},
        protocolVersion{"protocol_version"};

    enum class Key
    {
        // Set result keys
        ErrNo,
        // Device keys
        PrivateKey,
        ListenPort,
        Fwmark,
        ReplacePeers,   // Set only
        // Peer keys
        PublicKey,
        Remove,         // Set only
        UpdateOnly,     // Set only
        PresharedKey,
        Endpoint,
        PersistentKeepaliveInterval,
        ReplaceAllowedIps,  // Set only
        // Allowed IP keys
        AllowedIp,
        // Peer keys (more)
        RxBytes,        // Get only
        TxBytes,        // Get only
        LastHandshakeTimeSec,   // Get only
        LastHandshakeTimeNsec,  // Get only
        ProtocolVersion,
    };
    Q_ENUM_NS(Key);

    std::unordered_map<QLatin1String, Key> lookupKey{
        // Set result keys
        {errNo, Key::ErrNo},
        // Device keys
        {privateKey, Key::PrivateKey},
        {listenPort, Key::ListenPort},
        {fwmark, Key::Fwmark},
        {replacePeers, Key::ReplacePeers},
        // Peer keys
        {publicKey, Key::PublicKey},
        {remove, Key::Remove},
        {updateOnly, Key::UpdateOnly},
        {presharedKey, Key::PresharedKey},
        {endpoint, Key::Endpoint},
        {persistentKeepaliveInterval, Key::PersistentKeepaliveInterval},
        {replaceAllowedIps, Key::ReplaceAllowedIps},
        // Allowed IP keys
        {allowedIp, Key::AllowedIp},
        // Peer keys (more)
        {rxBytes, Key::RxBytes},
        {txBytes, Key::TxBytes},
        {lastHandshakeTimeSec, Key::LastHandshakeTimeSec},
        {lastHandshakeTimeNsec, Key::LastHandshakeTimeNsec},
        {protocolVersion, Key::ProtocolVersion},
    };

    template<>
    long long strToLonglong(const char *str, char **end, int base)
    {
        return std::strtoll(str, end, base);
    }
    template<>
    unsigned long long strToLonglong(const char *str, char **end, int base)
    {
        return std::strtoull(str, end, base);
    }

    // Parse a Wireguard key.  Does not alter the key if the value can't be
    // parsed.
    void parseWireguardKey(const QLatin1String &value, wg_key &key)
    {
        static_assert(CHAR_BIT == 8, "Assumed 8-bit chars");

        // Parse into scratch space; don't alter key if the value was invalid
        wg_key parsed;
        static_assert(sizeof(parsed[0]) == 1, "Assumed wg_key is array of bytes");
        if(value.size() != sizeof(parsed)*2)
        {
            // Value not traced - could be a private key
            qWarning() << "WireGuard key invalid";
            throw Error{HERE, Error::Code::Unknown};
        }

        auto parseNibble = [](QLatin1Char lc) -> quint8
        {
            char c = lc.toLatin1();
            if(c >= '0' && c <= '9')
                return static_cast<quint8>(c - '0');
            if(c >= 'a' && c <= 'f')
                return static_cast<quint8>(c - 'a' + 10);
            qWarning() << "WireGuard key invalid";
            throw Error{HERE, Error::Code::Unknown};
        };
        for(unsigned i=0; i<sizeof(parsed); ++i)
        {
            parsed[i] = (parseNibble(value[i*2])<<4) | parseNibble(value[i*2+1]);
        }
        std::copy(std::begin(parsed), std::end(parsed), std::begin(key));
    }

    // Copy a non-null-terminated IP address and parse it with inet_pton()
    void copyParseIp(int af, const char *begin, const char *end, void *dst)
    {
        static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN, "Unexpected address str lengths");
        char address[INET6_ADDRSTRLEN]{};
        if(end - begin >= static_cast<int>(sizeof(address)))
            throw Error{HERE, Error::Code::Unknown};    // Address too long
        std::copy(begin, end, std::begin(address));
        if(::inet_pton(af, address, dst) != 1)
            throw Error{HERE, Error::Code::Unknown};    // Address invalid
    }

    // Parse a peer endpoint - parses to either addr4 or addr6.
    void parsePeerEndpoint(const QLatin1String &value, decltype(wg_peer{}.endpoint) &endpoint)
    {
        // The permitted formats are:
        //  IPv4: IP:port
        //  IPv6: [IP]:port
        if(!value.isEmpty() && value[0] == '[')
        {
            auto itIpv6End = std::find(value.begin(), value.end(), ']');
            // Must be present, and must be followed by ':'
            if(itIpv6End == value.end())
                throw Error{HERE, Error::Code::Unknown};
            auto itColon = itIpv6End + 1;
            if(itColon == value.end() || *itColon != ':')
                throw Error{HERE, Error::Code::Unknown};

            in6_addr addr6{};
            copyParseIp(AF_INET6, value.data()+1, itIpv6End, &addr6);
            uint16_t port = parseInt<uint16_t>(value.mid(static_cast<int>(itColon - value.begin() + 1)));
            // Parse succeeded, store back
            endpoint.addr6.sin6_family = AF_INET6;
            endpoint.addr6.sin6_port = htons(port);
            endpoint.addr6.sin6_addr = addr6;
        }
        else
        {
            auto itColon = std::find(value.begin(), value.end(), ':');
            if(itColon == value.end())
                throw Error{HERE, Error::Code::Unknown};

            in_addr addr4;
            copyParseIp(AF_INET, value.data(), itColon, &addr4);
            uint16_t port = parseInt<uint16_t>(value.mid(static_cast<int>(itColon - value.begin() + 1)));
            // Parse succeeded
            endpoint.addr4.sin_family = AF_INET;
            endpoint.addr4.sin_port = htons(port);
            endpoint.addr4.sin_addr = addr4;
        }
    }

    void parseAllowedIp(const QLatin1String &value, wg_allowedip &ip)
    {
        // The permitted formats is:
        //  IP/cidr
        // (for both IPv4 and IPv6)
        auto itSlash = std::find(value.begin(), value.end(), '/');
        if(itSlash == value.end())
            throw Error{HERE, Error::Code::Unknown};
        uint8_t cidr = parseInt<uint8_t>(value.mid(static_cast<int>(itSlash - value.begin() + 1)));
        if(std::find(value.begin(), itSlash, ':') != itSlash)
        {
            in6_addr addr6;
            copyParseIp(AF_INET6, value.begin(), itSlash, &addr6);
            ip.family = AF_INET6;
            ip.ip6 = addr6;
        }
        else
        {
            in_addr addr4;
            copyParseIp(AF_INET, value.begin(), itSlash, &addr4);
            ip.family = AF_INET;
            ip.ip4 = addr4;
        }
        ip.cidr = cidr;
    }

    // Append an IP address to a request
    void appendIp(QByteArray &message, const in_addr &value)
    {
        char address[INET_ADDRSTRLEN]{};
        if(!::inet_ntop(AF_INET, &value, address, sizeof(address)))
        {
            // Not possible, only failure modes are invalid address family or buffer
            // size.
            Q_ASSERT(false);
        }
        message += address;
    }

    void appendIp(QByteArray &message, const in6_addr &value)
    {
        char address[INET6_ADDRSTRLEN]{};
        if(!::inet_ntop(AF_INET6, &value, address, sizeof(address)))
        {
            // Not possible, only failure modes are invalid address family or buffer
            // size.
            Q_ASSERT(false);
        }
        message += address;
    }

    // Append key-value pairs to a request
    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const wg_key &value)
    {
        message += key;
        message += '=';
        // Note that keys in UAPI are passed as hexadecimal, not base64.  Lowercase
        // is required.
        static const char hexChars[] = "0123456789abcdef";
        char valueStr[sizeof(value)*2];
        static_assert(CHAR_BIT == 8, "Assumed 8-bit chars");
        static_assert(sizeof(value[0]) == 1, "Assumed wg_key is array of bytes");
        for(unsigned i=0; i<sizeof(value); ++i)
        {
            valueStr[2*i] = hexChars[(value[i] & 0xF0) >> 4];
            valueStr[2*i+1] = hexChars[value[i] & 0x0F];
        }
        message.append(valueStr, sizeof(valueStr));
        message += '\n';
    }

    void appendRequest(QByteArray &message, const QLatin1String &key, int value)
    {
        message += key;
        message += '=';
        message += QByteArray::number(value, 10);
        message += '\n';
    }

    void appendRequest(QByteArray &message, const QLatin1String &key, unsigned value)
    {
        message += key;
        message += '=';
        message += QByteArray::number(value, 10);
        message += '\n';
    }

    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const sockaddr_in &value)
    {
        message += key;
        message += '=';
        appendIp(message, value.sin_addr);
        message += ':';
        message += QByteArray::number(ntohs(value.sin_port), 10);
        message += '\n';
    }

    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const sockaddr_in6 &value)
    {
        message += key;
        message += "=[";
        appendIp(message, value.sin6_addr);
        message += "]:";
        message += QByteArray::number(ntohs(value.sin6_port), 10);
        message += '\n';
    }

    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const wg_allowedip &value)
    {
        if(value.family != AF_INET && value.family != AF_INET6)
        {
            qWarning() << "Invalid address family in peer allowed IP:" << value.family;
            return;
        }

        message += key;
        message += '=';
        if(value.family == AF_INET)
            appendIp(message, value.ip4);
        else
            appendIp(message, value.ip6);
        message += '/';
        message += QByteArray::number(value.cidr, 10);
        message += '\n';
    }

}

WireguardIpc::WireguardIpc(std::shared_ptr<QLocalSocket> pIpcSocket)
    : _pIpcSocket{std::move(pIpcSocket)}, _finished{false}
{
    Q_ASSERT(_pIpcSocket); // Ensured by caller
    connect(_pIpcSocket.get(), &QLocalSocket::readyRead, this,
            [this]()
            {
                _ipcLineBuffer.append(_pIpcSocket->readAll());
            });
    // The disconnected() and error() signals are handled with queued
    // connections since it's not safe to destroy the socket during its signals.
    connect(_pIpcSocket.get(), &QLocalSocket::disconnected, this,
        [this]()
        {
            if(!_finished)
            {
                qInfo() << "IPC socket disconnected";
                emit abort();
            }
        }, Qt::ConnectionType::QueuedConnection);
    connect(_pIpcSocket.get(),
        QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
        this,
        [this](QLocalSocket::LocalSocketError err)
        {
            if(!_finished)
            {
                qInfo() << "IPC socket error:" << err;
                emit abort();
            }
        }, Qt::ConnectionType::QueuedConnection);
    // We have to be careful not to use queued invocations for the received data
    // anywhere in the pipeline.  On Windows, the server closes the pipe
    // connection immediately after QLocalSocket is notified of the last read
    // completion, which will generate a disconnect error.  If we were to queue
    // data in any way, it would deserialize those events, and the connection
    // could be aborted before we process all the data.
    connect(&_ipcLineBuffer, &LineBuffer::lineComplete, this,
            &WireguardIpc::processLine);
}

WireguardIpc::~WireguardIpc()
{
    Q_ASSERT(_pIpcSocket);  // Class invariant
    // Ensure we don't receive signals from the QLocalSocket's destructor
    _pIpcSocket->disconnect(this);
}

void WireguardIpc::processLine(const QByteArray &line)
{
    if(_finished)
    {
        // Don't expect data after the blank line
        qWarning() << "Received unexpected line after finished:" << line.size()
            << "bytes";
        return;
    }

    // Empty lines indicate the end of a message
    if(line.isEmpty())
    {
        // Emit finish() asynchronously - QLocalSocket and LineBuffer do not
        // expect to be destroyed during their signals.  Set _finished to ignore
        // errors that occur from a susequent disconnect, or any excess data.
        _finished = true;
        QMetaObject::invokeMethod(this, &WireguardIpc::finish,
                                  Qt::ConnectionType::QueuedConnection);
    }
    else
    {
        // Split on '='
        auto keyEndIdx = line.indexOf('=');
        if(keyEndIdx < 0 || keyEndIdx >= line.size())
        {
            qWarning() << "Invalid IPC line:" << line.data();
            return;
        }

        QLatin1String key{line.data(), line.data()+keyEndIdx};
        QLatin1String value{line.data()+keyEndIdx+1, line.data()+line.size()};
        auto itKeyMatch = Uapi::lookupKey.find(key);
        if(itKeyMatch == Uapi::lookupKey.end())
        {
            qWarning() << "Unknown key in IPC line:" << line.data();
            return;
        }

        emit receivedValue(itKeyMatch->second, value);
    }
}

bool WireguardIpc::writeIpcRequest(const QByteArray &message)
{
    // Send the request
    Q_ASSERT(_pIpcSocket);  // Class invariant
    auto written = _pIpcSocket->write(message);
    if(written < 0)
    {
        qWarning() << "Failed to send request, result" << written;
        return false;
    }

    if(written != message.size())
    {
        // Not expected, QLocalSocket queues up data to be written
        qWarning() << "QLocalSocket wrote partial data (" << written << "/"
            << message.size() << ")";
    }

    return true;
}


WireguardConfigDeviceTask::WireguardConfigDeviceTask(std::shared_ptr<QLocalSocket> pIpcSocket,
                                                     const wg_device &wgDev)
    : _ipc{std::move(pIpcSocket)}, _errno{EBADMSG}
    // Defaults to EBADMSG if the errno= line isn't provided for some reason.
    // We shouldn't default to success, and this code is reasonable.
{
    QByteArray request{"set=1\n"};
    // Build the request from the wg_device
    if(wgDev.flags & WGDEVICE_HAS_PRIVATE_KEY)
        Uapi::appendRequest(request, Uapi::privateKey, wgDev.private_key);
    if(wgDev.flags & WGDEVICE_HAS_LISTEN_PORT)
        Uapi::appendRequest(request, Uapi::listenPort, wgDev.listen_port);
    if(wgDev.flags & WGDEVICE_HAS_FWMARK)
        Uapi::appendRequest(request, Uapi::fwmark, wgDev.fwmark);
    if(wgDev.flags & WGDEVICE_REPLACE_PEERS)
    {
        request += Uapi::replacePeers;
        request += "=true\n";
    }
    // WGDEVICE_HAS_PUBLIC_KEY is not implemented, not in UAPI
    Q_ASSERT(!(wgDev.flags & WGDEVICE_HAS_PUBLIC_KEY));
    // Peers
    for(const wg_peer *pPeer = wgDev.first_peer; pPeer; pPeer = pPeer->next_peer)
    {
        // Peers must have a public key; this key indicates the start of a peer
        Q_ASSERT(pPeer->flags & WGPEER_HAS_PUBLIC_KEY);
        Uapi::appendRequest(request, Uapi::publicKey, pPeer->public_key);
        if(pPeer->flags & WGPEER_REMOVE_ME)
        {
            request += Uapi::remove;
            request += "=true\n";
        }
        if(pPeer->flags & WGPEER_HAS_PRESHARED_KEY)
            Uapi::appendRequest(request, Uapi::presharedKey, pPeer->preshared_key);
        switch(pPeer->endpoint.addr.sa_family)
        {
            case AF_INET:
                Uapi::appendRequest(request, Uapi::endpoint, pPeer->endpoint.addr4);
                break;
            case AF_INET6:
                Uapi::appendRequest(request, Uapi::endpoint, pPeer->endpoint.addr6);
                break;
            case AF_UNSPEC:
                // Not set, skip
                break;
            default:
                Q_ASSERT(false);    // Should have a valid family
                break;
        }
        if(pPeer->flags & WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL)
        {
            Uapi::appendRequest(request, Uapi::persistentKeepaliveInterval,
                                pPeer->persistent_keepalive_interval);
        }
        if(pPeer->flags & WGPEER_REPLACE_ALLOWEDIPS)
        {
            request += Uapi::replaceAllowedIps;
            request += "=true\n";
        }
        for(const wg_allowedip *pAllowedIp = pPeer->first_allowedip;
            pAllowedIp;
            pAllowedIp = pAllowedIp->next_allowedip)
        {
            Uapi::appendRequest(request, Uapi::allowedIp, *pAllowedIp);
        }
    }
    request += '\n';    // Blank line to terminate request

    if(!_ipc.writeIpcRequest(request))
    {
        // Failed; traced by writeIpcRequest
        reject({HERE, Error::Code::WireguardNotResponding});
    }

    connect(&_ipc, &WireguardIpc::receivedValue, this,
            &WireguardConfigDeviceTask::receiveValue);
    connect(&_ipc, &WireguardIpc::finish, this,
        [this]()
        {
            auto keepAlive = sharedFromThis();
            if(isPending())
                resolve(_errno);
            else
                qWarning() << "Unexpected finish signal for finished IPC task";
        });
    connect(&_ipc, &WireguardIpc::abort, this,
        [this]()
        {
            auto keepAlive = sharedFromThis();
            if(isPending())
                reject({HERE, Error::Code::WireguardProcessFailed});
            else
                qWarning() << "Unexpected abort signal for finished IPC task";
        });
}

void WireguardConfigDeviceTask::receiveValue(Uapi::Key key, const QLatin1String &value)
{
    if(!isPending())
    {
        qWarning() << "Unexpected IPC value:" << traceEnum(key) << "-" << value
            << "for finished IPC task";
    }
    else if(key != Uapi::Key::ErrNo)
    {
        qWarning() << "Unexpected key type:" << traceEnum(key) << "-"
            << value;
        reject(Error{HERE, Error::Code::Unknown});
    }
    else
        _errno = Uapi::parseInt<int>(value);
}

WireguardDeviceStatusTask::WireguardDeviceStatusTask(std::shared_ptr<QLocalSocket> pIpcSocket)
    : _ipc{std::move(pIpcSocket)}, _pDev{new WgDevStatus{}}, _errno{EBADMSG}
{
    if(!_ipc.writeIpcRequest(QByteArrayLiteral("get=1\n\n")))
    {
        // Failed; traced by writeIpcRequest
        reject({HERE, Error::Code::WireguardNotResponding});
    }

    connect(&_ipc, &WireguardIpc::receivedValue, this,
            &WireguardDeviceStatusTask::receiveValue);
    connect(&_ipc, &WireguardIpc::finish, this,
        [this]()
        {
            auto keepAlive = sharedFromThis();
            if(isPending())
            {
                if(_errno)
                {
                    qWarning() << "UAPI get failed - error" << _errno;
                    reject({HERE, Error::Code::Unknown});
                }
                else
                    resolve(_pDev);
            }
            else
                qWarning() << "Unexpected finish signal for finished IPC task";
        });
    connect(&_ipc, &WireguardIpc::abort, this,
        [this]()
        {
            auto keepAlive = sharedFromThis();
            if(isPending())
                reject({HERE, Error::Code::WireguardProcessFailed});
            else
                qWarning() << "Unexpected abort signal for finished IPC task";
        });
}

template<class FlagsT>
void WireguardDeviceStatusTask::addFlag(FlagsT &flags, FlagsT newFlag)
{
    flags = static_cast<FlagsT>(flags | newFlag);
}

void WireguardDeviceStatusTask::ensureHasPeer()
{
    Q_ASSERT(_pDev);    // Class invariant
    if(!_pDev->device().last_peer)
    {
        qWarning() << "Invalid peer field; not preceded by public key";
        throw Error{HERE, Error::Code::Unknown};
    }
}

void WireguardDeviceStatusTask::applyValue(Uapi::Key key, const QLatin1String &value)
{
    Q_ASSERT(_pDev);    // Class invariant
    switch(key)
    {
        default:
            qWarning() << "Unexpected key type:" << traceEnum(key) << "-"
                << value;
            throw Error{HERE, Error::Code::Unknown};
        case Uapi::Key::ErrNo:
            _errno = Uapi::parseInt<int>(value);
            break;
        // Device keys
        case Uapi::Key::PrivateKey:
            Uapi::parseWireguardKey(value, _pDev->device().private_key);
            addFlag(_pDev->device().flags, WGDEVICE_HAS_PRIVATE_KEY);
            break;
        case Uapi::Key::ListenPort:
            _pDev->device().listen_port = Uapi::parseInt<uint16_t>(value);
            addFlag(_pDev->device().flags, WGDEVICE_HAS_LISTEN_PORT);
            break;
        case Uapi::Key::Fwmark:
            _pDev->device().fwmark = Uapi::parseInt<uint32_t>(value);
            addFlag(_pDev->device().flags, WGDEVICE_HAS_FWMARK);
            break;
        // Peer keys
        case Uapi::Key::PublicKey:
        {
            // Add a new peer
            wg_peer &peer = _pDev->addPeer({});
            // Parse the key after adding the peer; if the key is invalid we
            // don't want to apply all the later fields to a prior peer
            Uapi::parseWireguardKey(value, peer.public_key);
            addFlag(peer.flags, WGPEER_HAS_PUBLIC_KEY);
            break;
        }
        case Uapi::Key::PresharedKey:
            ensureHasPeer();
            Uapi::parseWireguardKey(value, _pDev->device().last_peer->preshared_key);
            addFlag(_pDev->device().last_peer->flags, WGPEER_HAS_PRESHARED_KEY);
            break;
        case Uapi::Key::Endpoint:
            ensureHasPeer();
            Uapi::parsePeerEndpoint(value, _pDev->device().last_peer->endpoint);
            break;
        case Uapi::Key::PersistentKeepaliveInterval:
            ensureHasPeer();
            _pDev->device().last_peer->persistent_keepalive_interval = Uapi::parseInt<uint16_t>(value);
            addFlag(_pDev->device().last_peer->flags, WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL);
            break;
        // Allowed IP keys
        case Uapi::Key::AllowedIp:
        {
            ensureHasPeer();
            // Add a new allowed IP
            wg_allowedip &allowedIp = _pDev->addAllowedIp({});
            Uapi::parseAllowedIp(value, allowedIp);
            break;
        }
        // Peer keys (more)
        case Uapi::Key::RxBytes:
            ensureHasPeer();
            _pDev->device().last_peer->rx_bytes = Uapi::parseInt<uint64_t>(value);
            break;
        case Uapi::Key::TxBytes:
            ensureHasPeer();
            _pDev->device().last_peer->tx_bytes = Uapi::parseInt<uint64_t>(value);
            break;
        case Uapi::Key::LastHandshakeTimeSec:
            ensureHasPeer();
            _pDev->device().last_peer->last_handshake_time.tv_sec = Uapi::parseInt<int64_t>(value);
            break;
        case Uapi::Key::LastHandshakeTimeNsec:
            ensureHasPeer();
            _pDev->device().last_peer->last_handshake_time.tv_nsec = Uapi::parseInt<int64_t>(value);
            break;
        case Uapi::Key::ProtocolVersion:
            ensureHasPeer();
            // Ignored; this is valid but is not in wg_peer
            break;
    }
}

void WireguardDeviceStatusTask::receiveValue(Uapi::Key key, const QLatin1String &value)
{
    try
    {
        applyValue(key, value);
    }
    catch(const Error &err)
    {
        qWarning() << "Invalid IPC value:" << traceEnum(key) << "-" << value
            << "error:" << err;
    }
}

#include "wireguarduapi.moc"
