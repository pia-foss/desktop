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

#include "pathmtu.h"
#include <kapps_core/src/ipaddress.h>
#if defined(Q_OS_WIN)
    #include "win/win_daemon.h"
    #include "win/win_interfacemonitor.h"
    #include <common/src/win/win_util.h>
    #include <kapps_core/src/win/win_error.h>
#endif

namespace
{
    // The VPN server's gateway address in Gen4 is always 10.0.0.1.  MTU
    // detection pings are always sent through the tunnel - this ensures that
    // the proper VPN encapsulation overhead is always accounted for, and it's
    // robust in case (say) a firewall treats ICMP vs. UDP traffic differently,
    // etc.
    enum : quint32
    {
        VpnServerGateway = 0x0A000001,  // 10.0.0.1
    };
}

Executor MtuPinger::_executor{CURRENT_CATEGORY};

MtuPinger::MtuPinger(std::shared_ptr<NetworkAdapter> pTunnelAdapter,
                     int maxTunnelMtu, int mtuSetting)
    : _pTunnelAdapter{std::move(pTunnelAdapter)}, _mtu{1200}, _goodMtu{1200},
      _badMtu{maxTunnelMtu+1}, _retryCounter{0}
{
#if !defined(Q_OS_WIN)
    connect(&_ping, &PosixPing::receivedReply, this, &MtuPinger::receivedReply);
#endif
    connect(&_pingTimeout, &QTimer::timeout, this, &MtuPinger::timeout);
    _pingTimeout.setInterval(3000);

    // Auto MTU - detect automatically, using maxMtu as upper bound
    if(mtuSetting < 0)
    {
        start();
    }
    // "Small packets" or some other specific MTU requested
    else if(mtuSetting > 0)
    {
        // Use the requested MTU if it is smaller than the calculated MTU.
        // Keep the calculated MTU if the requested MTU was larger.
        applyMtu(std::min(maxTunnelMtu, mtuSetting));
    }
    // _connectingConfig.mtu() == 0 ("Large packets") - use max MTU, no detection
    else
    {
        applyMtu(maxTunnelMtu);
    }
}

void MtuPinger::start()
{
    // Check if we're done - the range is closed to < 10 bytes.  We don't probe
    // further to get down to the exact byte; this adds a lot of time for little
    // improved accuracy (failed probes can only be detected by a timeout)
    if (_goodMtu > _badMtu || _badMtu - _goodMtu < 10)
    {
        qInfo() << "MTU search done with final range" << _badMtu << "-" <<
            _goodMtu << ", choose MTU" << _goodMtu;
        applyMtu(_goodMtu);
        return;
    }

    // Test the midpoint of the current range
    _mtu = (_goodMtu + _badMtu) / 2;
    qInfo() << "try MTU: " << _mtu;
    _pingTimeout.start(3000);
#if defined(Q_OS_WIN)
     std::chrono::milliseconds timeout{3000};
     QPointer<WinIcmpEcho> echo = WinIcmpEcho::send(VpnServerGateway, timeout, _mtu - 28, false);
     connect(echo.data(), &WinIcmpEcho::receivedReply, this, &MtuPinger::receivedReply);
     connect(echo.data(), &WinIcmpEcho::receivedError, this, &MtuPinger::receivedError);
#else
    _ping.sendEchoRequest(VpnServerGateway, _mtu - 28, false);
#endif
}

void MtuPinger::receivedReply(quint32 addr)
{
    _pingTimeout.stop();
    _goodMtu = _mtu;
    _retryCounter = 0;
    qInfo() << "MTU" << _mtu << "succeeded, now have range" << _badMtu
            << "-" << _goodMtu;
    start();
}

void MtuPinger::timeout()
{
    _pingTimeout.stop();
    _retryCounter++;
    if (_retryCounter > 2) {
        _badMtu = _mtu;
        qInfo() << "MTU" << _mtu << "timed out for all" << _retryCounter <<
          "attempts, now have range" << _badMtu << "-" << _goodMtu;
        _retryCounter = 0;
        // apply MTU closer to the target, note that the good MTU can't be
        // applied here since larget packets need to come through
        applyMtu(_badMtu);
    } else
        qInfo() << "MTU" << _mtu << "timed out after try" <<
            _retryCounter << ", try again";
    start();
}

void MtuPinger::receivedError(int code)
{
    timeout();
}

void MtuPinger::applyMtu(int mtu)
{
    qInfo() << "set MTU: " << mtu;
#if defined(Q_OS_WIN)
    auto pWinAdapter = std::static_pointer_cast<WinNetworkAdapter>(_pTunnelAdapter);

    MIB_IPINTERFACE_ROW tunItf{};
    InitializeIpInterfaceEntry(&tunItf);
    tunItf.Family = AF_INET;
    tunItf.InterfaceLuid.Value = pWinAdapter->luid();
    auto getResult = GetIpInterfaceEntry(&tunItf);
    if(getResult != NO_ERROR)
    {
        qWarning() << "Unable to get interface state to set MTU to" << mtu
            << "-" << kapps::core::WinErrTracer{getResult};
    }
    else
    {
        tunItf.NlMtu = mtu;
        auto setResult = SetIpInterfaceEntry(&tunItf);
        if(setResult != NO_ERROR)
        {
            qWarning() << "Unable to set interface MTU to" << mtu << "-"
                << kapps::core::WinErrTracer{setResult};
        }
        else
        {
            qInfo() << "Set interface MTU to" << mtu;
        }
    }
#endif
#if defined(Q_OS_MACOS)
    _executor.bash(QStringLiteral("ifconfig %1 mtu %2").arg(_pTunnelAdapter->devNode()).arg(mtu));
#endif
#if defined(Q_OS_LINUX)
    _executor.bash(QStringLiteral("ip link set mtu %1 dev %2").arg(mtu).arg(_pTunnelAdapter->devNode()));
#endif
}
