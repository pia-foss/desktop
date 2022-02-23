// Copyright (c) 2022 Private Internet Access, Inc.
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

#include "posix_mtu.h"
#include "exec.h"

namespace PosixMtu
{

// Find the interface that would be used to reach a given host, according to the
// routing table.
QString findInterfaceForHost(const QString &host)
{
#if defined(Q_OS_MACOS)
    auto hostItfMatch = Exec::cmdWithRegex(QStringLiteral("route"),
        {QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("-inet"), host},
        QRegularExpression{QStringLiteral("interface: ([^ ]+)$"), QRegularExpression::PatternOption::MultilineOption});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto hostItfMatch = Exec::cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("get"), host},
        QRegularExpression{QStringLiteral("dev ([^ ]+)")});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#endif
    return {};
}

// Find the interface that has the default route.
QString findDefaultInterface()
{
#if defined(Q_OS_MACOS)
    auto defItfMatch = Exec::cmdWithRegex(QStringLiteral("route"),
        {QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("-inet"), QStringLiteral("default")},
        QRegularExpression{QStringLiteral("interface: ([^ ]+)$"), QRegularExpression::PatternOption::MultilineOption});
    if(defItfMatch.hasMatch())
        return defItfMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto hostItfMatch = Exec::cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")},
        QRegularExpression{QStringLiteral("dev ([^ ]+)")});
    if(hostItfMatch.hasMatch())
        return hostItfMatch.captured(1);
#endif
    return {};
}

// Get the MTU for a given interface.
unsigned findInterfaceMtu(const QString &itf)
{
    QString mtuStr;
#if defined(Q_OS_MACOS)
    auto itfMtuMatch = Exec::cmdWithRegex(QStringLiteral("ifconfig"),
        {itf}, QRegularExpression{QStringLiteral("mtu ([0-9]+)")});
    if(itfMtuMatch.hasMatch())
        mtuStr = itfMtuMatch.captured(1);
#elif defined(Q_OS_LINUX)
    auto itfMtuMatch = Exec::cmdWithRegex(QStringLiteral("ip"),
        {QStringLiteral("link"), QStringLiteral("show"), QStringLiteral("dev"),
         itf}, QRegularExpression{QStringLiteral("mtu ([0-9]+)")});
    if(itfMtuMatch.hasMatch())
        mtuStr = itfMtuMatch.captured(1);
#endif
    return mtuStr.toUInt();
}

unsigned findHostMtu(const QString &host)
{
    const auto &hostItf = findInterfaceForHost(host);
    if(!hostItf.isEmpty())
    {
        // Found an interface for the specified host.  Return its MTU, or 0 if
        // the interface doesn't have an MTU specified.  (It doesn't seem
        // sensible to check the default route since we know what interface will
        // be used for this host; if they were different we'd just be applying
        // the MTU from some other irrelevant interface.)
        unsigned mtu = findInterfaceMtu(hostItf);
        qInfo() << "Found MTU" << mtu << "from interface" << hostItf
            << "to host" << host;
        return mtu;
    }

    // We couldn't find an interface for that host, try to find the default
    // interface.
    const auto &defaultItf = findDefaultInterface();
    if(!defaultItf.isEmpty())
    {
        unsigned mtu = findInterfaceMtu(defaultItf);
        qInfo() << "Found MTU" << mtu << "from default interface" << defaultItf;
        return mtu;
    }

    return 0;
}


}
