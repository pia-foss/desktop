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
#line SOURCE_FILE("win_ping.cpp")

#include "win_ping.h"
#include <kapps_core/src/win/win_error.h>
#include <kapps_core/src/winapi.h>
#include <QHostAddress>
#include <iphlpapi.h>
#include <IcmpAPI.h>
#include <WinSock2.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace
{
    struct WinCloseIcmp
    {
        void operator()(HANDLE handle){::IcmpCloseHandle(handle);}
    };
    using WinIcmpHandle = WinGenericHandle<HANDLE, WinCloseIcmp>;

    class WinIcmpRef : public WinIcmpHandle
    {
    public:
        WinIcmpRef()
            : WinIcmpHandle{::IcmpCreateFile()}
        {
            // This can fail according to doc; there's nothing we can do if it
            // does.
            if(!*this)
            {
                kapps::core::WinErrTracer err{::GetLastError()};
                qWarning() << "Failed to open ICMP:" << err;
            }
        }
    };

    // Get the ICMP handle - created when first used.  Remains open for the life
    // of the program.
    WinIcmpRef &getIcmpRef()
    {
        static WinIcmpRef _icmp;
        return _icmp;
    }
}

QPointer<WinIcmpEcho> WinIcmpEcho::send(quint32 address,
                                        std::chrono::milliseconds timeout,
                                        WORD payloadSize,
                                        bool allowFragments)
{
    WinIcmpRef &icmp{getIcmpRef()};
    if(!icmp)
    {
        qWarning() << "Can't send ICMP echo - ICMP handle couldn't be opened";
        return {};
    }

    // Use the same payload as `ping`
    // Per doc, reply buffer must be able to contain all these components:
    // - payload
    // - ICMP_ECHO_REPLY
    // - 8 bytes for an ICMP error
    // - IO_STATUS_BLOCK - 8 bytes, struct is actually defined in DDK it
    //   seems
    //
    // The size of IO_STATUS_BLOCK is hard-coded due to the structure
    // apparently being defined in the DDK.  IcmpSendEcho2() checks the
    // size of the reply buffer, so absolute-worst-case we will fail to send
    // pings.
    // Match default for 'ping' on Windows
    std::vector<std::uint8_t> payloadBuf;
    payloadBuf.resize(payloadSize);
    quint8* payload = payloadBuf.data();
    WORD ReplyBufSize = payloadSize + sizeof(ICMP_ECHO_REPLY) + 8/*ICMP error*/ + 8/*IO_STATUS_BLOCK*/;

    // Create an event that the reply will signal
    WinHandle event{::CreateEventW(nullptr, false, false, nullptr)};
    if(!event)
    {
        kapps::core::WinErrTracer err{::GetLastError()};
        qWarning() << "Unable to ping" << QHostAddress{address}.toString()
            << "- failed to create event:" << err;
        return {};
    }

    // Create a WinIcmpEcho to receive the reply.  This is owned by send() until
    // we have successfully called IcmpSendEcho2() and know a response will be
    // given.
    std::unique_ptr<WinIcmpEcho> pEcho{new WinIcmpEcho{std::move(event), ReplyBufSize}};

    IPAddr pingAddr{};
    pingAddr = htonl(address);

    IP_OPTION_INFORMATION ip_opts{};
    ip_opts.Ttl = 128;
    if (!allowFragments) {
        ip_opts.Flags = IP_FLAG_DF | IP_OPT_ROUTER_ALERT;
    }

    auto result = ::IcmpSendEcho2(icmp, pEcho->_event, nullptr, nullptr,
                                  pingAddr, payload, payloadSize,
                                  allowFragments ? nullptr : &ip_opts,
                                  pEcho->_replyBuffer.data(),
                                  static_cast<WORD>(pEcho->_replyBuffer.size()),
                                  msec(timeout));

    // Doc indicates that IcmpSendEcho2() will return ERROR_IO_PENDING when
    // called asynchronously, but it actually return ERROR_SUCCESS and
    // GetLastError() returns ERROR_IO_PENDING.  Accept either result.
    if(result == ERROR_SUCCESS || result == ERROR_IO_PENDING)
    {
        // Success - disown the WinIcmpEcho and return it
        return {pEcho.release()};
    }

    // Failure - trace the error, let unique_ptr destroy the WinIcmpEcho
    kapps::core::WinErrTracer err{::GetLastError()};
    qWarning() << "Unable to ping" << QHostAddress{address}.toString()
        << "- result" << result << "- error" << err;
    return {};
}

WinIcmpEcho::WinIcmpEcho(WinHandle event, std::size_t replyBufSize)
    : _event{std::move(event)}, _notifier{_event}
{
    _replyBuffer.resize(replyBufSize);
    connect(&_notifier, &QWinEventNotifier::activated, this,
            &WinIcmpEcho::onEventActivated);
}

bool WinIcmpEcho::shouldTraceIcmpError(DWORD status) const
{
    // Ignore statuses that indicate normal failures (timeout, unreachable)
    switch(status)
    {
        case IP_SUCCESS:
            // Not usually passed to this function
            return false;
        case IP_REQ_TIMED_OUT:
            // This is normal; don't trace it - LatencyBatch traces regions that
            // fail to respond all at once
            return false;
        case IP_DEST_NET_UNREACHABLE:
        case IP_DEST_HOST_UNREACHABLE:
            // These also happen under normal circumstances - just lack of
            // network connectivity.  Don't trace.
            return false;
        default:
            // Unexpected error - trace it
            return true;
    }
}

std::wstring WinIcmpEcho::ipErrorString(DWORD status) const
{
    enum
    {
        BufferSize = 1024
    };
    wchar_t buffer[BufferSize];
    DWORD actualLen{BufferSize-1};
    if(::GetIpErrorString(status, buffer, &actualLen) == NO_ERROR)
    {
        // Success, use this string.
        return {buffer, actualLen};
    }

    // If GetIpErrorString() fails, doc says we should fall back to
    // FormatMessage().  This probably isn't really needed here - in one case we
    // trace both anyway since we can't be sure which type of result it is, and
    // in ICMP_ECHO_REPLY::Status we probably only get ICMP_ codes.
    //
    // Grab it anyway though just to err on the side of getting _something_,
    // this is just for tracing.
    return kapps::core::WinErrTracer{status}.message();
}

void WinIcmpEcho::onEventActivated()
{
    // We've received a response or timeout.  Destroy WinIcmpEcho when we're
    // done here.
    std::unique_ptr<WinIcmpEcho> pThisRef{this};

#ifdef _WIN64
    using EchoReplyStruct = ICMP_ECHO_REPLY32;
#else
    using EchoReplyStruct = ICMP_ECHO_REPLY;
#endif

    // Parse the reply buffer
    DWORD result = ::IcmpParseReplies(_replyBuffer.data(), _replyBuffer.size());
    if(result == 0)
    {
        // This is unusual - this isn't what happens for a timeout, we get a
        // response with a timeout status in that case.
        kapps::core::WinErrTracer err{::GetLastError()};
        // Doc is unclear here - documentation seems to indicate that we should
        // observe IP_REQ_TIMED_OUT, etc., from the ICMP_ECHO_REPLY::Status
        // field, but that's not what happens in practice.
        //
        // In reality, we get a 0 result from IcmpParseEchoReplies(), and
        // GetLastError() returns an IP_* value (which is itself unclear whether
        // it could also return NT error codes in addition to IP codes; these
        // overlap).  Probably ICMP_ECHO_REPLY::Status was only useful for
        // the original IcmpSendEcho().
        //
        // If the last error is an unimportant IP error, skip it (even though
        // we're not 100% sure that it's actually an IP error and not the
        // same-numbered NT error).
        if(shouldTraceIcmpError(err.code()))
        {
            // Include both IP and NT messages since we're not sure which it is.
            qWarning() << "Couldn't parse ICMP echo reply status:" << err.code()
                << "- possible messages:" << ipErrorString(err.code()) << "/"
                << err.message();
        }
        emit receivedError(err.code());
        return;
    }

    const EchoReplyStruct *pReplyData = reinterpret_cast<const EchoReplyStruct*>(_replyBuffer.data());

    // We don't really expect to get more than one response.  If we did, trace
    // them, and then just use the first response.
    if(result > 1)
    {
        qWarning() << "Received" << result << "responses to ICMP echo, expected 1";
        for(int i=0; i<result; ++i)
        {
            qWarning() << "-" << i << "-"
                << QHostAddress{ntohl(pReplyData[0].Address)}.toString()
                << "- status:" << pReplyData[0].Status << "- rtt:"
                << pReplyData[0].RoundTripTime;
        }
    }

    quint32 replyAddr = ntohl(pReplyData->Address);
    if(pReplyData->Status == IP_SUCCESS)
        emit receivedReply(replyAddr);
    else if(shouldTraceIcmpError(pReplyData->Status))
    {
        qWarning() << "Received error from ICMP echo to"
            << QHostAddress{replyAddr}.toString() << "-"
            << pReplyData->Status << "-" << ipErrorString(pReplyData->Status);
        emit receivedError(pReplyData->Status);
    }
}
