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

#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <schannel.h>
#include <security.h>
#include <sspi.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <memory>
#include <optional>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")

class Winsock
{
public:
    Winsock(int reqMajor, int reqMinor)
    {
        // Major number is in low byte, minor number is in high byte
        int err = ::WSAStartup(MAKEWORD(reqMinor, reqMajor), &_wsaData);
        // If it failed, abort, do not call WSACleanup()
        if(err)
            throw std::runtime_error{"Winsock 2.2 is required"};
        // Note that if it returned a lower version than we requested (2.1, etc.),
        // then we do need to call WSACleanup() even if we reject that version.
    }

    ~Winsock()
    {
        ::WSACleanup();
    }

private:
    Winsock(const Winsock &) = delete;
    Winsock &operator=(const Winsock &) = delete;

public:
    bool supportsVersion(int supportMajor, int supportMinor)
    {
        if(LOBYTE(_wsaData.wVersion) < supportMajor)
            return false;
        if(LOBYTE(_wsaData.wVersion) == supportMajor)
            return HIBYTE(_wsaData.wVersion) >= supportMinor;
        return true;
    }

private:
    WSADATA _wsaData;
};

class SecurityInterface
{
public:
    SecurityInterface()
    {
        _pSecFuncTable = ::InitSecurityInterfaceW();
        if(!_pSecFuncTable)
            throw std::runtime_error{"Unable to load SSPI security interface"};
        // TODO - also check the needed entry points
    }

    const SecurityFunctionTableW &get() const {return *_pSecFuncTable;}
private:
    SecurityFunctionTableW *_pSecFuncTable;
};

// SSPI handle deleters
class SspiCredDeleter
{
public:
    void operator()(CredHandle &h){::FreeCredentialsHandle(&h);}
};

class SspiContextDeleter
{
public:
    SspiContextDeleter(const SecurityInterface &secItf) : _secItf{secItf} {}

public:
    void operator()(CtxtHandle &h){_secItf.get().DeleteSecurityContext(&h);}
private:
    const SecurityInterface &_secItf;
};

// SSPI object owner.  Note that SSPI handles are 128 bits on 64-bit platforms,
// not just a plain pointer (two ULONG_PTRs in general).  Most API functions take a
// pointer-to-handle as a result.
//
// The various SSPI handle types are actually all typedefs of the same underlying
// opaque handle, but they do require different deleters, so the deleter must be
// specfieid.  (The HandleT actually doesn't matter as a result, but it's helpful
// for exposition at least.)
template<class HandleT, class DeleterT>
class SspiHandle
{
public:
    SspiHandle(DeleterT del = {}) : _handle{}, _deleter{std::move(del)} {}
    ~SspiHandle() {clear();}

public:
    void clear() {_deleter(_handle);}
    // Not const-correct - most API functions require a non-const pointer
    // Don't mess with the handle through this :-/  Don't use this to
    // populate SspiHandle with a new handle; use receive() for that
    HandleT *get() {return &_handle;}
    // Receive a handle into the SspiHandle(), used to pass a pointer to an
    // empty handle to an API that creates and stores it
    HandleT *receive() {clear(); return get();}

private:
    HandleT _handle;
    DeleterT _deleter;
};

using SspiCredHandle = SspiHandle<CredHandle, SspiCredDeleter>;
// SspiCtxtHandle requires a SecurityInterface since the deleter API is
// part of the package interface
using SspiCtxtHandle = SspiHandle<CtxtHandle, SspiContextDeleter>;

class WinSocket
{
public:
    WinSocket(int af, int type, int protocol)
        : _socket{INVALID_SOCKET}
    {
        _socket = ::socket(af, type, protocol);
        if(_socket == INVALID_SOCKET)
            throw std::runtime_error{"Unable to allocate socket"};
    }

    ~WinSocket()
    {
        ::closesocket(_socket);
    }

private:
    WinSocket(const WinSocket &) = delete;
    WinSocket &operator=(const WinSocket &) = delete;

public:
    int connect(const sockaddr *name, int namelen)
    {
        return ::connect(_socket, name, namelen);
    }
    int send(const char *buf, int len, int flags)
    {
        return ::send(_socket, buf, len, flags);
    }
    int recv(char *buf, int len, int flags)
    {
        return ::recv(_socket, buf, len, flags);
    }
    int eventSelect(WSAEVENT eventHandle, long events)
    {
        return ::WSAEventSelect(_socket, eventHandle, events);
    }
    int enumNetworkEvents(WSAEVENT eventHandle, WSANETWORKEVENTS *pEvents)
    {
        return ::WSAEnumNetworkEvents(_socket, eventHandle, pEvents);
    }

private:
    SOCKET _socket;
};

class WinSocketEvent
{
public:
    WinSocketEvent()
        : _event{::WSACreateEvent()}
    {
        if(_event == WSA_INVALID_EVENT)
            throw std::runtime_error{"Failed to create Winsock event"};
    }
    ~WinSocketEvent()
    {
        // Somehow WSACloseEvent() can fail (returns zero), but what could we
        // even do if it did?
        ::WSACloseEvent(_event);
    }

private:
    WinSocketEvent(const WinSocketEvent &) = delete;
    WinSocketEvent &operator=(const WinSocketEvent &) = delete;

public:
    WSAEVENT get() {return _event;}
    operator WSAEVENT() {return get();}

private:
    WSAEVENT _event;
};

class WinError : public std::runtime_error
{
public:
    // GetLastError() and security APIs return DWORD, WSAGetLastError()
    // returns int.  Both work with FormatMessage()
    WinError(const char *pMsg, int err) : WinError{pMsg, static_cast<DWORD>(err)} {}
    WinError(const char *pMsg, DWORD err)
        : std::runtime_error{pMsg}, _err{err}
    {
    }

public:
    DWORD getWinErr() const {return _err;}
    std::string getWinMsg() const;

private:
    DWORD _err;
};

std::string WinError::getWinMsg() const
{
    LPSTR errMsg{nullptr};

    auto len = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                                nullptr, _err, 0,
                                reinterpret_cast<LPSTR>(&errMsg), 0, nullptr);
    std::string msg{errMsg, len};
    ::LocalFree(errMsg);

    return msg;
}

void createClientCredential(const SecurityInterface &secItf, SspiCredHandle &clientCred)
{
    SCHANNEL_CRED clientCredSpec{};
    clientCredSpec.dwVersion = SCHANNEL_CRED_VERSION;
    // Use the defaults for everything in SCHANNEL_CRED; all fields
    // support 0 indicating the system defaults.  No client cert is
    // specified.
    TimeStamp expire{};
    SECURITY_STATUS err = secItf.get().AcquireCredentialsHandleW(nullptr, UNISP_NAME_W,
        SECPKG_CRED_OUTBOUND, nullptr, &clientCredSpec, nullptr, nullptr,
        clientCred.receive(), &expire);
    if(err != SEC_E_OK)
        throw WinError{"Unable to obtain credentials handle", err};
}

void secureHandshake(const SecurityInterface &secItf, SspiCredHandle &clientCred, WinSocket &socket,
                     const char *pProxyHostname, SspiCtxtHandle &secureCtxt,
                     std::vector<char> &receivedData)
{
    SecBufferDesc sendBufferDesc{};
    SecBuffer sendBuffer{};
    sendBufferDesc.ulVersion = SECBUFFER_VERSION;
    sendBufferDesc.cBuffers = 1;
    sendBufferDesc.pBuffers = &sendBuffer;
    sendBuffer.cbBuffer = 0;
    sendBuffer.BufferType = SECBUFFER_TOKEN;
    sendBuffer.pvBuffer = nullptr;
    DWORD sspiContextFlags{};
    DWORD sspiRequestFlags{ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR |
        ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM};
    DWORD sspiRequiredRetFlags{ISC_RET_CONFIDENTIALITY | ISC_RET_REPLAY_DETECT | ISC_RET_SEQUENCE_DETECT |
        ISC_RET_STREAM};

    // Convert the host name to UTF-16 - since we've already assumed it's
    // ASCII by using getaddrinfo(), just widen each char to 16 bits.
    std::wstring hostnameW;
    std::size_t hostnameLen{std::strlen(pProxyHostname)};
    hostnameW.resize(hostnameLen);
    std::copy(pProxyHostname, pProxyHostname + hostnameLen,
                hostnameW.data());
    SECURITY_STATUS err = secItf.get().InitializeSecurityContextW(clientCred.get(), nullptr,
        hostnameW.data(), sspiRequestFlags, 0, 0, nullptr, 0, secureCtxt.receive(),
        &sendBufferDesc, &sspiContextFlags, nullptr);

    if(err != SEC_I_CONTINUE_NEEDED)
        throw WinError{"Unable to initialize security context", err};

    // Send the response to the server if present
    if(sendBuffer.cbBuffer > 0 && sendBuffer.pvBuffer)
    {
        int sent = socket.send(reinterpret_cast<const char*>(sendBuffer.pvBuffer),
            sendBuffer.cbBuffer, 0);
        secItf.get().FreeContextBuffer(sendBuffer.pvBuffer);
        if(sent != sendBuffer.cbBuffer)
            throw WinError{"Failed to send context response", ::WSAGetLastError()};
    }

    // Keep receiving until the handshake is complete
    receivedData.resize(65536);
    std::size_t queuedSize{0};

    while(err == SEC_I_CONTINUE_NEEDED || err == SEC_E_INCOMPLETE_MESSAGE)
    {
        // Read if we don't have a complete message yet (there's nothing queued,
        // or SSPI said we don't have a complete message).
        //
        // We can skip this if we just completed a step and we already have more
        // data ready.
        if(queuedSize == 0 || err == SEC_E_INCOMPLETE_MESSAGE)
        {
            int received = socket.recv(receivedData.data() + queuedSize,
                receivedData.size() - queuedSize, 0);
            if(received == SOCKET_ERROR || received == 0)
                throw WinError{"Server disconnected during handshake", ::WSAGetLastError()};
            queuedSize += received;
        }

        // Pass the input data to SSPI.  Provide it an extra input buffer in case it
        // does not consume all the data; it will copy the remainder to the extra
        // buffer.
        SecBufferDesc inputBufferDesc{};
        SecBuffer inputBuffers[2];
        SecBufferDesc outputBufferDesc{};
        SecBuffer outputBuffer;
        inputBufferDesc.ulVersion = SECBUFFER_VERSION;
        inputBufferDesc.cBuffers = 2;
        inputBufferDesc.pBuffers = inputBuffers;
        inputBuffers[0].cbBuffer = queuedSize;
        inputBuffers[0].BufferType = SECBUFFER_TOKEN;
        inputBuffers[0].pvBuffer = reinterpret_cast<void *>(receivedData.data());
        inputBuffers[1].cbBuffer = 0;
        inputBuffers[1].BufferType = SECBUFFER_EMPTY;
        inputBuffers[1].pvBuffer = nullptr;
        outputBufferDesc.ulVersion = SECBUFFER_VERSION;
        outputBufferDesc.cBuffers = 1;
        outputBufferDesc.pBuffers = &outputBuffer;
        outputBuffer.cbBuffer = 0;
        outputBuffer.BufferType = SECBUFFER_TOKEN;
        outputBuffer.pvBuffer = nullptr;

        err = secItf.get().InitializeSecurityContextW(clientCred.get(),
            secureCtxt.get(), nullptr, sspiRequestFlags, 0, 0, &inputBufferDesc,
            0, nullptr, &outputBufferDesc, &sspiContextFlags, nullptr);

        // If any response content was given, send it.  This can happen even if the
        // negotiation fails, such as for an error indication.
        if(outputBuffer.cbBuffer > 0 && outputBuffer.pvBuffer)
        {
            auto sent = socket.send(reinterpret_cast<char*>(outputBuffer.pvBuffer), outputBuffer.cbBuffer, 0);
            secItf.get().FreeContextBuffer(outputBuffer.pvBuffer);
            if(sent != outputBuffer.cbBuffer)
                throw WinError{"Failed to send handshake response data to server", ::WSAGetLastError()};
        }

        // If the message was incomplete, keep the data in receivedData and keep receiving.
        if(err == SEC_E_INCOMPLETE_MESSAGE)
            continue;

        // If we processed a message and are continuing or finished, we'll check for extra data.
        //
        // Any other codes are not expected; fail the handshake.
        // Schannel does not use the 'complete' statuses (SEC_I_COMPLETE_NEEDED,
        // SEC_I_COMPLETE_AND_CONTINUE).  We don't support any client credentials at
        // this layer (SEC_I_INCOMPLETE_CREDENTIALS).
        if(err != SEC_E_OK && err != SEC_I_CONTINUE_NEEDED)
            throw WinError{"Server handshake failed", err};

        // Put extra data back in receivedData, or reset it if there is no extra data.
        if(inputBuffers[1].BufferType == SECBUFFER_EXTRA)
        {
            std::copy(receivedData.begin() + queuedSize - inputBuffers[1].cbBuffer,
                      receivedData.begin() + queuedSize, receivedData.begin());
            queuedSize = inputBuffers[1].cbBuffer;
        }
        else
            queuedSize = 0;
    }

    // We're done, if the handshake didn't complete successfully, bail.
    if(err != SEC_E_OK)
        throw WinError{"Server handshake failed", err};

    // Make sure we actually got the security guarantees we asked for
    if((sspiContextFlags & sspiRequiredRetFlags) != sspiRequiredRetFlags)
        throw WinError{"Handshake failed security requirements", err};

    // Resize the vector to indicate how much leftover data there actually were
    receivedData.resize(queuedSize);
}

class MsgBuf
{
public:
    MsgBuf(const SecPkgContext_StreamSizes &sizes)
        : _headerSize{sizes.cbHeader}, _trailerSize{sizes.cbTrailer}
    {
        setMsgSize(sizes.cbMaximumMessage);
    }

public:
    std::size_t headerSize() const {return _headerSize;}
    std::size_t msgSize() const {return _data.size() - _headerSize - _trailerSize;}
    void setMsgSize(std::size_t msgSize) {_data.resize(_headerSize + msgSize + _trailerSize);}
    std::size_t trailerSize() const {return _trailerSize;}

    std::size_t totalSize() const {return _data.size();}

    char *headerBuf() {return _data.data();}
    char *msgBuf() {return _data.data() + _headerSize;}
    char *trailerBuf() {return _data.data() + (_data.size() - _trailerSize);}

    // data() is the same as headerBuf(), but this does make it clear that
    // this context is referring to the whole buffer, not the header
    char *data() {return _data.data();}
    const char *data() const {return _data.data();}

private:
    std::size_t _headerSize;
    std::size_t _trailerSize;
    std::vector<char> _data;
};

template<std::size_t N>
void appendMsg(MsgBuf &msg, std::size_t &writePos, const char (&data)[N])
{
    std::copy(&data[0], &data[N], msg.msgBuf() + writePos);
    writePos += N;
}
void appendMsg(MsgBuf &msg, std::size_t &writePos, const char *data)
{
    auto len = data ? std::strlen(data) : 0;
    std::copy(data, data + len, msg.msgBuf() + writePos);
    writePos += len;
}
void appendMsg(MsgBuf &msg, std::size_t &writePos, const std::string &data)
{
    std::copy(data.begin(), data.end(), msg.msgBuf() + writePos);
    writePos += data.size();
}

// Send the last 'remaining' bytes of 'msg'.  If the socket is async and would
// block, some data may not be sent, in which case the new remaining count is
// returned.  (If all data were sent, 0 is returned.)
unsigned long sendRemainingData(WinSocket &socket, const MsgBuf &msg,
    unsigned long remaining)
{
    // Send until we are out of data or the socket (if async) would block.
    // send() is allowed to accept part of the buffer, this doesn't necessarily
    // indicate that the socket is about to block (try again until it actually
    // says it would block)
    while(remaining > 0)
    {
        auto sent = socket.send(msg.data() + (msg.totalSize() - remaining),
            remaining, 0);
        if(sent == SOCKET_ERROR)
        {
            auto error = ::WSAGetLastError();
            // If the socket couldn't send because it would have to block, we're
            // done, send the remaining bytes later
            if(error == WSAEWOULDBLOCK)
                return remaining;
            // Throw for any other error
            throw WinError{"Failed to send data to server", error};
        }

        // Otherwise, subtract the bytes sent
        if(sent > remaining)
        {
            // Shouldn't be possible, would cause underflow
            WinError err{"Sent more bytes than requested", ::WSAGetLastError()};
            throw err;
        }

        remaining -= sent;
    }
    // Sent everything
    return 0;
}

// Encrypt a data payload (in msg) and attempt to send it.  If the socket
// is asynchronous and would block, the remaining data size to be sent is
// returned.  (Otherwise 0 is returned.)  The encrypted data remains in msg in
// that case (send it with sendRemainingData)
unsigned long syncEncryptSend(const SecurityInterface &secItf, WinSocket &socket,
    SspiCtxtHandle &secureCtxt, MsgBuf &msg)
{
    SecBufferDesc sendBufferDesc;
    SecBuffer sendBuffers[4]{};

    sendBufferDesc.ulVersion = SECBUFFER_VERSION;
    sendBufferDesc.cBuffers = 4;
    sendBufferDesc.pBuffers = sendBuffers;

    sendBuffers[0].cbBuffer = msg.headerSize();
    sendBuffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    sendBuffers[0].pvBuffer = reinterpret_cast<void*>(msg.headerBuf());
    sendBuffers[1].cbBuffer = msg.msgSize();
    sendBuffers[1].BufferType = SECBUFFER_DATA;
    sendBuffers[1].pvBuffer = reinterpret_cast<void*>(msg.msgBuf());
    sendBuffers[2].cbBuffer = msg.trailerSize();
    sendBuffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    sendBuffers[2].pvBuffer = reinterpret_cast<void*>(msg.trailerBuf());
    sendBuffers[3].BufferType = SECBUFFER_EMPTY;

    SECURITY_STATUS err = secItf.get().EncryptMessage(secureCtxt.get(), 0, &sendBufferDesc, 0);

    // It's not clear whether EncryptMessage can change the section sizes, we
    // might need to be able to update these sizes in MsgBuf
    if(sendBuffers[0].cbBuffer != msg.headerSize())
    {
        std::cerr << "header changed from" << msg.headerSize() << "to"
            << sendBuffers[0].cbBuffer << std::endl;
        throw std::runtime_error{"header changed"};
    }
    if(sendBuffers[1].cbBuffer != msg.msgSize())
    {
        std::cerr << "message changed from" << msg.msgSize() << "to"
            << sendBuffers[1].cbBuffer << std::endl;
        throw std::runtime_error{"message changed"};
    }
    if(sendBuffers[2].cbBuffer != msg.trailerSize())
    {
        std::cerr << "trailer changed from" << msg.trailerSize() << "to"
            << sendBuffers[2].cbBuffer << std::endl;
        throw std::runtime_error{"trailer changed"};
    }

    return sendRemainingData(socket, msg, msg.totalSize());
}

// Receive and decrypt data.  The data are decrypted in-place in buffer.  If a
// complete TLS message record is received, {pMsg, msgLen} are nonzero and
// denote the message in the buffer.  If additional encrypted data are present,
// {pExtra, extraLen} are nonzero and denote that data in the buffer.
//
// Extra data should be retained at the beginning of buffer for the next
// receive-decrypt.
void syncReceiveDecrypt(const SecurityInterface &secItf, WinSocket &socket,
    SspiCtxtHandle &secureCtxt, std::vector<char> &buffer,
    char *(&pMsg), std::size_t &msgLen, char *(&pExtra), std::size_t &extraLen)
{
    std::size_t queuedData = buffer.size();
    buffer.resize(65536);
    SECURITY_STATUS err{SEC_E_OK};

    pMsg = nullptr;
    msgLen = 0;
    pExtra = nullptr;
    extraLen = 0;

    unsigned long received{0};
    bool wouldBlock{false};

    do
    {
        // Skip the recv() the first time around if there's already queued data,
        // we might already have a full message
        if(err == SEC_E_INCOMPLETE_MESSAGE || queuedData == 0)
        {
            received = socket.recv(buffer.data() + queuedData, buffer.size() - queuedData, 0);
            if(received == SOCKET_ERROR && ::WSAGetLastError() == WSAEWOULDBLOCK)
            {
                // Proceed without any new data (if there is queued data, we
                // could still have a complete message).  We won't try to
                // receive again, wait for more data
                received = 0;
                wouldBlock = true;
            }
            else if(received == 0 || received == SOCKET_ERROR)
            {
                std::cerr << "receiver error: " << received << " - " << std::hex << WSAGetLastError() << std::endl;
                throw WinError{"Server disconnected", ::WSAGetLastError()};
            }
            queuedData += received;
        }

        // Try to decrypt the data
        SecBufferDesc receiveBufferDesc{};
        SecBuffer receiveBuffers[4]{};
        receiveBufferDesc.ulVersion = SECBUFFER_VERSION;
        receiveBufferDesc.cBuffers = 4;
        receiveBufferDesc.pBuffers = receiveBuffers;
        receiveBuffers[0].cbBuffer = queuedData;
        receiveBuffers[0].BufferType = SECBUFFER_DATA;
        receiveBuffers[0].pvBuffer = reinterpret_cast<void*>(buffer.data());
        receiveBuffers[1].BufferType = SECBUFFER_EMPTY;
        receiveBuffers[2].BufferType = SECBUFFER_EMPTY;
        receiveBuffers[3].BufferType = SECBUFFER_EMPTY;

        err = secItf.get().DecryptMessage(secureCtxt.get(), &receiveBufferDesc, 0, nullptr);

        if(err == SEC_I_CONTEXT_EXPIRED)
            throw WinError{"Server signaled end of connection", err};
        // TODO - implement renegotiate
        if(err == SEC_I_RENEGOTIATE)
            throw WinError{"Server requested renegotiate; not implemented", err};
        if(err == SEC_E_OK)
        {
            // Find the data and extra data
            for(std::size_t i=0; i<4 && (!pMsg || !pExtra); ++i)
            {
                if(!pMsg && receiveBuffers[i].BufferType == SECBUFFER_DATA)
                {
                    pMsg = reinterpret_cast<char*>(receiveBuffers[i].pvBuffer);
                    msgLen = receiveBuffers[i].cbBuffer;
                }
                if(!pExtra && receiveBuffers[i].BufferType == SECBUFFER_EXTRA)
                {
                    pExtra = reinterpret_cast<char*>(receiveBuffers[i].pvBuffer);
                    extraLen = receiveBuffers[i].cbBuffer;
                }
            }
            // We got a message, so return - we must return this message to the
            // caller before receiving or decrypting again
            return;
        }
        else if(err != SEC_E_INCOMPLETE_MESSAGE)
            throw WinError{"Error decrypting incoming data", err};
    }
    while(!wouldBlock);

    // We didn't complete a messsage, but the async socket would have had to
    // block.  The entire buffer is "extra data"
    pExtra = buffer.data();
    extraLen = queuedData;
}

void readHttpsConnectResponse(const char *pMsgBegin, const char *pMsgEnd)
{
    // Read the status line
    std::string crLf{"\r\n"};
    auto pStatusLineEnd = std::search(pMsgBegin, pMsgEnd, crLf.begin(), crLf.end());
    // If there were no headers, no CRLF is found (the message does not
    // include the terminating CRLFCRLF), and the entire message is the status
    // line (pStatusLineEnd == pMsgEnd).

    // Check for the 200 status code.  We don't really care about the HTTP
    // version or status message, so just look for " 200 " instead of
    // actually splitting on spaces.
    std::string okStatus{" 200 "};
    if(std::search(pMsgBegin, pStatusLineEnd, okStatus.begin(), okStatus.end())
        == pStatusLineEnd)
    {
        // Didn't find " 200 ".  It's some other result code, or an entirely
        // malformed response.
        std::cerr << "Server responded: ";
        std::cerr.write(pMsgBegin, (pMsgEnd-pMsgBegin));
        std::cerr << std::endl;
        std::cerr << std::endl;
        throw std::runtime_error{"Server rejected HTTP CONNECT"};
    }

    // Success - go ahead with connection.
    // Don't even read the remaining headers if there are any, we don't care
    // about them for HTTP CONNECT.
}

std::string base64Encode(const char *pData, std::size_t len)
{
    const char b64chars[65]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

    std::string result;
    std::size_t base64Len{len / 3 * 4};
    if(len % 3)
        base64Len += 4;
    result.resize(base64Len);

    char *pOut = result.data();
    while(len >= 3)
    {
        auto word = static_cast<std::uint_fast32_t>((pData[0] << 16) | (pData[1] << 8) | pData[2]);

        pOut[0] = b64chars[word >> 18];
        word &= 0x03FFFF;
        pOut[1] = b64chars[word >> 12];
        word &= 0x000FFF;
        pOut[2] = b64chars[word >> 6];
        word &= 0x00003F;
        pOut[3] = b64chars[word];

        pData += 3;
        len -= 3;
        pOut += 4;
    }

    // Handle tail bytes and '=' padding.  There are only two possibilites -
    // len=2 (AAA=) and len=1 (AA==)
    if(len)
    {
        auto tail = static_cast<std::uint_fast32_t>(pData[0] << 16);
        if(len == 2)
            tail |= (pData[1] << 8);

        pOut[0] = b64chars[tail >> 18];
        tail &= 0x03FFFF;
        pOut[1] = b64chars[tail >> 12];
        tail &= 0x000FFF;
        if(len == 2)
            pOut[2] = b64chars[tail >> 6];
        else
            pOut[2] = '=';
        pOut[3] = '=';
    }

    return result;
}

class CliParams
{
public:
    CliParams(int argc, char **argv)
    {
        if(argc != 5)
            throw std::runtime_error{"Incorrect number of arguments"};

        _pBasicAuth = argv[1];
        _pProxyHost = argv[2];
        const char *proxyPortStr{argv[3]};
        _pTunnelTarget = argv[4];
        if(!_pBasicAuth || !_pProxyHost || !proxyPortStr || !_pTunnelTarget)
            throw std::runtime_error{"Incorrect number of arguments"};

        // Parse proxyPortStr; it must only contain digits and result in a value
        // in range [1, 65536].  std::atoi() also allows whitespace, +-, etc., so
        // check digits manually too.
        for(const char *pPortChar = proxyPortStr; *pPortChar; ++pPortChar)
        {
            if(*pPortChar < '0' || *pPortChar > '9')
                throw std::runtime_error{"Invalid proxy port"};
        }
        int parsedPort = std::atoi(proxyPortStr);
        if(parsedPort < 1 || parsedPort > 65535)
            throw std::runtime_error{"Invalid proxy port"};
        _proxyPort = static_cast<std::uint16_t>(parsedPort);
    }

public:
    const char *basicAuth() const {return _pBasicAuth;}
    const char *proxyHost() const {return _pProxyHost;}
    std::uint16_t proxyPort() const {return _proxyPort;}
    const char *tunnelTarget() const {return _pTunnelTarget;}

private:
    const char *_pBasicAuth;
    const char *_pProxyHost;
    std::uint16_t _proxyPort;
    const char *_pTunnelTarget;
};

void printUsage(const char *name)
{
    std::cerr << "usage:" << std::endl;
    std::cerr << "  " << name << " <basic-auth> <proxy-host> <proxy-port> <target>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "example:" << std::endl;
    std::cerr << "  " << name << "  me:mypassword our.proxy.example.com 8443 internal.service.example.com:22" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Creates a tunneled connection through an HTTPS proxy using HTTPS CONNECT." << std::endl;
    std::cerr << "stdin is sent to the tunneled connection, output is sent to stdout." << std::endl;
    std::cerr << "Intended for use as an SSH ProxyCommand." << std::endl;
}

int main(int argc, char **argv)
{
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);

    std::optional<CliParams> params;

    try
    {
        params.emplace(argc, argv);
    }
    catch(const std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        std::cerr << std::endl;
        const char *name = (argc > 0 && argv[0]) ? argv[0] : "win-httpstunnel.exe";
        printUsage(name);
        return -1;
    }

    try
    {
        SECURITY_STATUS err;

        SecurityInterface secItf{};

        Winsock ws{2, 2};
        if(!ws.supportsVersion(2, 2))
            throw std::runtime_error{"Winsock 2.2 is required"};

        SspiCredHandle clientCred;
        createClientCredential(secItf, clientCred);

        // Resolve the proxy hostname
        ADDRINFOA addrInfoHints{};
        addrInfoHints.ai_family = AF_INET;
        addrInfoHints.ai_socktype = SOCK_STREAM;
        addrInfoHints.ai_protocol = IPPROTO_TCP;
        ADDRINFOA *pProxyAddrInfoRaw{};
        err = ::getaddrinfo(params->proxyHost(), "1", &addrInfoHints,
            &pProxyAddrInfoRaw);
        // Own the returned pointer
        std::unique_ptr<ADDRINFOA, decltype(&::freeaddrinfo)> pProxyAddrInfo{pProxyAddrInfoRaw, &::freeaddrinfo};
        if(err || !pProxyAddrInfo)
            throw WinError{"Unable to resolve proxy hostname", err};
        // Although getaddrinfo() returns a linked list of addresses, we asked
        // for IPv4 only, so we just inspect the first one.
        if(pProxyAddrInfo->ai_family != AF_INET ||
            pProxyAddrInfo->ai_addrlen != sizeof(SOCKADDR_IN) || !pProxyAddrInfo->ai_addr)
            throw std::runtime_error{"Proxy host was resolved but did not return an IPv4 address"};
        const SOCKADDR_IN *pProxyAddr = reinterpret_cast<const SOCKADDR_IN*>(pProxyAddrInfo->ai_addr);

        WinSocket socket{AF_INET, SOCK_STREAM, IPPROTO_TCP};
        SOCKADDR_IN sin{};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(params->proxyPort());
        sin.sin_addr.S_un.S_addr = pProxyAddr->sin_addr.S_un.S_addr;

        if(socket.connect(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin)))
        {
            throw WinError{"Unable to connect to remote host", ::WSAGetLastError()};
        }

        SspiCtxtHandle secureCtxt{secItf};
        std::vector<char> receivedData;
        secureHandshake(secItf, clientCred, socket, params->proxyHost(), secureCtxt, receivedData);

        SecPkgContext_StreamSizes sizes{};
        err = secItf.get().QueryContextAttributes(secureCtxt.get(), SECPKG_ATTR_STREAM_SIZES, &sizes);
        if(err != SEC_E_OK)
            throw WinError{"Failed to query message size limits", err};

        MsgBuf sendBuf{sizes};
        std::size_t httpsConnectMsgSize{0};
        appendMsg(sendBuf, httpsConnectMsgSize, "CONNECT ");
        appendMsg(sendBuf, httpsConnectMsgSize, params->tunnelTarget());
        appendMsg(sendBuf, httpsConnectMsgSize, " HTTP/1.1");
        appendMsg(sendBuf, httpsConnectMsgSize, "\r\nUser-Agent: win-httptunnel\r\nProxy-Authorization: Basic ");
        appendMsg(sendBuf, httpsConnectMsgSize,
            base64Encode(params->basicAuth(), std::strlen(params->basicAuth())));
        appendMsg(sendBuf, httpsConnectMsgSize, "\r\n\r\n");
        sendBuf.setMsgSize(httpsConnectMsgSize);

        auto connectUnsent = syncEncryptSend(secItf, socket, secureCtxt, sendBuf);
        // The socket is still blocking at this point, so connectUnsent must be 0
        if(connectUnsent)
            throw std::runtime_error{"Socket did not send entire CONNECT"};

        // At this point, since the TLS handshake is complete, receivedData is
        // now the queue for encrypted data, which might already contain data.
        //
        // decryptedData will be the queue for decrypted received data, which
        // may contain partial HTTP lines until we complete the CONNECT.
        std::vector<char> decryptedData;
        std::string responseEnd{"\r\n\r\n"};
        std::vector<char>::iterator responseEndPos;

        // Receive the HTTP CONNECT response.
        while((responseEndPos = std::search(decryptedData.begin(), decryptedData.end(),
                responseEnd.begin(), responseEnd.end())) == decryptedData.end())
        {
            char *pMsg{}, *pExtra{};
            std::size_t msgLen{}, extraLen{};
            syncReceiveDecrypt(secItf, socket, secureCtxt, receivedData, pMsg, msgLen, pExtra, extraLen);

            // Add the decrypted "message" (which is a TLS message record, not
            // necessarily a complete HTTP response) to the decrypted data
            // queue
            if(pMsg && msgLen > 0)
            {
                auto existingSize = decryptedData.size();
                decryptedData.resize(existingSize + msgLen);
                std::copy(pMsg, pMsg+msgLen, decryptedData.begin() + existingSize);
            }

            // Retain any extra data in the received data queue
            if(pExtra && extraLen > 0)
            {
                receivedData.erase(receivedData.begin(), receivedData.begin() + (pExtra - receivedData.data()));
                receivedData.resize(extraLen);
            }
            else
            {
                receivedData.resize(0);
            }
        }

        // Make sure we got 200 OK
        const char *pResponseEnd = decryptedData.data();
        pResponseEnd += (responseEndPos - decryptedData.begin());
        readHttpsConnectResponse(decryptedData.data(), pResponseEnd);

        // If we got any extra decrypted data, write it out now.
        responseEndPos += responseEnd.size();
        pResponseEnd += responseEnd.size();   // Valid because we matched this string
        std::cout.write(pResponseEnd, decryptedData.end() - responseEndPos);
        std::cout.flush();

        // Poll both stdin and the socket's read/write/close events.  Set up an
        // event to use WaitForMultipleObjects().
        // eventSelect() automatically switches the socket to asynchronous, so
        // now we have to be able to queue outgoing data that would have
        // blocked.
        WinSocketEvent socketEvent;
        if(socket.eventSelect(socketEvent, FD_READ|FD_WRITE|FD_CLOSE) == SOCKET_ERROR)
            throw WinError{"Unable to select read/close events on socket", ::WSAGetLastError()};

        // There are two different ways we could poll, which depends on the
        // condition of the stdin->socket throughput:
        // 1. Socket write has blocked - don't poll stdin (we can't read any
        //    more, input will have to wait until we get an FD_WRITE event)
        // 2. Socket write has not blocked - do poll stdin (we are not expecting
        //    FD_WRITE since nothing has blocked)
        //
        // The mechanism for #2 varies based on whether the input is a pipe due
        // to yet more quirks of Windows.
        enum : std::size_t
        {
            WaitCfgWriteNotBlocked,
            WaitCfgWriteBlocked,
            WaitCfgCount,
        };

        struct
        {
            HANDLE *pHandles;
            DWORD handleCount;
            DWORD waitTimeout;
        } waitConfigs[WaitCfgCount]{};

        HANDLE stdinFile{::GetStdHandle(STD_INPUT_HANDLE)};
        // Set up the "not blocked" config
        HANDLE waitNotBlockedHandles[2]{socketEvent.get(), stdinFile};
        waitConfigs[WaitCfgWriteNotBlocked].pHandles = waitNotBlockedHandles;
        waitConfigs[WaitCfgWriteNotBlocked].handleCount = 2;
        waitConfigs[WaitCfgWriteNotBlocked].waitTimeout = INFINITE;
        // Windows pipes are not waitable objects (:facepalm:)  If stdin is a
        // pipe, we have to fall back to short-polling.
        if(GetFileType(stdinFile) == FILE_TYPE_PIPE)
        {
            waitConfigs[WaitCfgWriteNotBlocked].handleCount = 1;    // Don't wait on the pipe
            waitConfigs[WaitCfgWriteNotBlocked].waitTimeout = 100;  // Wake periodically to try reading from the pipe
        }

        // Set up the "blocked" config
        HANDLE waitBlockedHandles[1]{socketEvent.get()};
        waitConfigs[WaitCfgWriteBlocked].pHandles = waitBlockedHandles;
        waitConfigs[WaitCfgWriteBlocked].handleCount = 1;
        waitConfigs[WaitCfgWriteBlocked].waitTimeout = INFINITE;

        // If the socket blocks, the data to be sent remains in sendBuf, and the
        // remaining data size is held here.  We can't receive when the socket
        // has blocked.
        unsigned long remainingSendSize{0};

        bool continueReading{true};
        do
        {
            std::size_t waitCfg{WaitCfgWriteNotBlocked};
            if(remainingSendSize)
                waitCfg = WaitCfgWriteBlocked;

            switch(::WaitForMultipleObjects(waitConfigs[waitCfg].handleCount,
                waitConfigs[waitCfg].pHandles, FALSE,
                waitConfigs[waitCfg].waitTimeout))
            {
                case WAIT_OBJECT_0: // Socket
                {
                    WSANETWORKEVENTS netEvents{};
                    if(socket.enumNetworkEvents(socketEvent, &netEvents) == SOCKET_ERROR)
                        throw WinError{"Failed to get network events from socket", ::WSAGetLastError()};

                    // FD_READ is "level-triggered", i.e. we don't have to
                    // ensure we read all available data.  Read until we get a
                    // complete TLS message and emit it.
                    if(netEvents.lNetworkEvents & FD_READ)
                    {
                        // Read and forward to stdout
                        char *pMsg{}, *pExtra{};
                        std::size_t msgLen{}, extraLen{};
                        do
                        {
                            syncReceiveDecrypt(secItf, socket, secureCtxt, receivedData, pMsg, msgLen, pExtra, extraLen);
                            if(pMsg && msgLen > 0)
                            {
                                std::cout.write(pMsg, msgLen);
                                std::cout.flush();
                            }

                            if(pExtra && extraLen > 0)
                            {
                                receivedData.erase(receivedData.begin(), receivedData.begin() + (pExtra - receivedData.data()));
                                receivedData.resize(extraLen);
                            }
                            else
                            {
                                receivedData.resize(0);
                            }
                        }
                        // If we read a message and still have extra data, try
                        // to decrypt again, we might already have another
                        // message.
                        while(pMsg && msgLen > 0 && !receivedData.empty());
                    }
                    // FD_WRITE is "edge-triggered", it only triggers when a
                    // write previously blocked, and writing is now possible
                    // again.
                    if(netEvents.lNetworkEvents & FD_WRITE)
                    {
                        // Socket is writable, if data are queued, send them.
                        // If we send everything, we'll start listening to
                        // stdin again (remainingSendSize becomes 0)
                        if(remainingSendSize)
                        {
                            remainingSendSize = sendRemainingData(socket,
                                sendBuf, remainingSendSize);
                        }
                    }
                    if(netEvents.lNetworkEvents & FD_CLOSE)
                    {
                        continueReading = false;
                    }
                    break;
                }
                case WAIT_OBJECT_0 + 1: // stdin
                {
                    sendBuf.setMsgSize(sizes.cbMaximumMessage);
                    DWORD readSize{};
                    if(!::ReadFile(stdinFile, sendBuf.msgBuf(),
                        sendBuf.msgSize(), &readSize, nullptr))
                    {
                        throw WinError{"Failed to read from stdin", ::GetLastError()};
                    }
                    sendBuf.setMsgSize(readSize);
                    // If the socket would block, remainingSendSize becomes
                    // nonzero, so we hold the data in sendBuf and stop
                    // listening to stdin
                    remainingSendSize = syncEncryptSend(secItf, socket,
                        secureCtxt, sendBuf);
                    break;
                }
                case WAIT_TIMEOUT:  // peek stdin pipe
                {
                    // For pipe input, it's important to keep looping here until
                    // no data remain (or until the socket would block).  When
                    // we return, it will be another 100ms before we check the
                    // pipe again.
                    DWORD readSize{};
                    do
                    {
                        if(!PeekNamedPipe(stdinFile, nullptr, 0, nullptr,
                            &readSize, nullptr))
                        {
                            throw WinError{"Failed to check for input on stdin", ::GetLastError()};
                        }
                        // If there is something to read, read up to a message
                        // length (then check again).
                        if(readSize)
                        {
                            sendBuf.setMsgSize(sizes.cbMaximumMessage);
                            if(!ReadFile(stdinFile, sendBuf.msgBuf(),
                                sendBuf.msgSize(), &readSize, nullptr))
                            {
                                throw WinError{"Failed to read from stdin", ::GetLastError()};
                            }
                            sendBuf.setMsgSize(readSize);
                            remainingSendSize = syncEncryptSend(secItf, socket,
                                secureCtxt, sendBuf);
                        }
                    }
                    while(readSize > 0 && remainingSendSize == 0);
                    break;
                }
                default:
                    // Any other result is unexpected.  (WAIT_FAILED is
                    // obviously an error, we don't expect any abandon results
                    // since we are not waiting on mutexes.)
                    throw std::runtime_error{"Failed waiting on input events"};
            }
        }
        while(continueReading);
    }
    catch(const WinError &ex)
    {
        std::cerr << ex.what() << std::endl;
        std::cerr << "(" << ex.getWinErr() << ") " << ex.getWinMsg() << std::endl;
        return -1;
    }
    catch(const std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        return -1;
    }
}
