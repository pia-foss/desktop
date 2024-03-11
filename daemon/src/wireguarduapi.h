// Copyright (c) 2024 Private Internet Access, Inc.
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
#line HEADER_FILE("wireguarduapi.h")

#ifndef WIREGUARDUAPI_H
#define WIREGUARDUAPI_H

#include <common/src/async.h>
#include <common/src/linebuffer.h>
#include "wireguardbackend.h"
#include <QLocalSocket>
#include <memory>
#include <deque>

namespace Uapi
{
    enum class Key;

    // Parsing and request formatting support - mainly only useful to
    // Wireguard UAPI implementation, but also covered by unit tests.

    // Use either std::strtoll() or std::strtoull() based on result type
    template<class LonglongT>
    LonglongT strToLonglong(const char *, char **, int);
    template<>
    long long strToLonglong(const char *str, char **end, int base);
    template<>
    unsigned long long strToLonglong(const char *str, char **end, int base);

    // Make a target type signed or unsigned based on a compile-time flag
    template<class IntT, bool sign>
    class WithSigned;
    template<class IntT>
    class WithSigned<IntT, true>
    {
    public:
        using type = std::make_signed_t<IntT>;
    };
    template<class IntT>
    class WithSigned<IntT, false>
    {
    public:
        using type = std::make_unsigned_t<IntT>;
    };

    // Copy signed-ness of one integer type to another.  'type' is signed TargetT if
    // SourceT is signed, unsigned TargetT otherwise.
    template<class TargetT, class SourceT>
    class SignedAs : public WithSigned<TargetT, std::is_signed<SourceT>::value> {};

    // Parse to a signed or unsigned long long; throws if the value cannot be
    // parsed (format is invalid).
    // Currently, if the value is out of range, the minimum/max for the
    // long long type is returned.  '-' is allowed even for parsing unsigned,
    // the result is still negated.  (strtoll()/strtoull() provide these
    // behaviors.)
    template<class LonglongT>
    LonglongT parseLonglong(const QLatin1String &value)
    {
        char *end{nullptr};
        LonglongT result = strToLonglong<LonglongT>(value.data(), &end, 10);
        // Must consume the entire string
        if(end != value.data() + value.size())
            throw Error{HERE, Error::Code::Unknown};
        return result;
    }

    // Parse to any integral type (signed or unsigned).  Throws if the value cannot
    // be parsed.
    template<class IntT>
    IntT parseInt(const QLatin1String &value)
    {
        auto result = parseLonglong<typename SignedAs<long long, IntT>::type>(value);
        if(result < std::numeric_limits<IntT>::min() || result > std::numeric_limits<IntT>::max())
            throw Error{HERE, Error::Code::Unknown};    // Out of range
        return static_cast<IntT>(result);
    }
    // Parse a Wireguard key.  Does not alter the key if the value can't be
    // parsed.
    void parseWireguardKey(const QLatin1String &value, wg_key &key);
    // Parse a peer endpoint - parses to either addr4 or addr6.
    void parsePeerEndpoint(const QLatin1String &value, decltype(wg_peer{}.endpoint) &endpoint);
    void parseAllowedIp(const QLatin1String &value, wg_allowedip &ip);

    // Append an IP address to a request
    void appendIp(QByteArray &message, const in_addr &value);
    void appendIp(QByteArray &message, const in6_addr &value);

    // Append key-value pairs to a request
    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const wg_key &value);
    void appendRequest(QByteArray &message, const QLatin1String &key, int value);
    void appendRequest(QByteArray &message, const QLatin1String &key, unsigned value);
    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const sockaddr_in &value);
    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const sockaddr_in6 &value);
    void appendRequest(QByteArray &message, const QLatin1String &key,
                       const wg_allowedip &value);
}

// Interface to the different tasks used to implement IPC results.
// WireguardUapi passes incoming IPC lines to the next queued result, then
// completes it when a blank line is received.
class WireguardIpc : public QObject
{
    Q_OBJECT

public:
    WireguardIpc(std::shared_ptr<QLocalSocket> pIpcSocket);
    ~WireguardIpc();

private:
    void processLine(const QByteArray &line);

public:
    // Send the IPC request in 'message'.  Returns true if it's sent
    // successfully.  Returns false if the data can't be written.
    bool writeIpcRequest(const QByteArray &message);

signals:
    // A key=value line was received
    void receivedValue(Uapi::Key key, const QLatin1String &value);
    // Finish the task (received a blank line)
    void finish();
    // Abort the task due to loss of the IPC connection.  Not emitted after
    // finish().
    void abort();

private:
    std::shared_ptr<QLocalSocket> _pIpcSocket;
    LineBuffer _ipcLineBuffer;
    // This flag is set once we receive a blank line.
    // - The finish() signal is queued (see processLine()), so this ensures we
    //   don't process anything after the blank (it would not be ordered
    //   correctly with the finish event).
    // - On Windows, we get a disconnect error immediately after the blank is
    //   received, we ignore errors once finished since we expect the server to
    //   disconnect.
    bool _finished;
};

// Send an IPC request to configure a Wireguard device and receive the errno
// response.
class WireguardConfigDeviceTask : public Task<int>
{
    Q_OBJECT

public:
    WireguardConfigDeviceTask(std::shared_ptr<QLocalSocket> pIpcSocket,
                              const wg_device &wgDev);

private:
    void receiveValue(Uapi::Key key, const QLatin1String &value);

private:
    WireguardIpc _ipc;
    int _errno;
};

// Request device stats from UAPI.  Populates a wg_device.  If the returned
// errno is nonzero, the task rejects.
class WireguardDeviceStatusTask : public Task<std::shared_ptr<WgDevStatus>>
{
    Q_OBJECT

private:
    // embeddable-wg-library flags aren't typed correctly - the bitwise
    // combination of wg_device_flags values is not itself a wg_device_flags
    // vaule.
    template<class FlagsT>
    void addFlag(FlagsT &flags, FlagsT newFlag);

public:
    WireguardDeviceStatusTask(std::shared_ptr<QLocalSocket> pIpcSocket);

private:
    void ensureHasPeer();

public:
    void applyValue(Uapi::Key key, const QLatin1String &value);
    void receiveValue(Uapi::Key key, const QLatin1String &value);

private:
    WireguardIpc _ipc;
    std::shared_ptr<WgDevStatus> _pDev;
    int _errno;
};

#endif
