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

import QtQuick 2.0
import PIA.NativeDaemon 1.0

QtObject {
  readonly property bool loggedIn: NativeDaemon.account.loggedIn
  readonly property string username: NativeDaemon.account.username
  readonly property string token: NativeDaemon.account.token
  readonly property string plan: NativeDaemon.account.plan
  readonly property bool active: NativeDaemon.account.active
  readonly property bool canceled: NativeDaemon.account.canceled
  readonly property bool recurring: NativeDaemon.account.recurring
  readonly property bool needsPayment: NativeDaemon.account.needsPayment
  readonly property int daysRemaining: NativeDaemon.account.daysRemaining
  readonly property bool renewable: NativeDaemon.account.renewable
  readonly property string renewURL: NativeDaemon.account.renewURL
  readonly property double expirationTime: NativeDaemon.account.expirationTime
  readonly property bool expireAlert: NativeDaemon.account.expireAlert
  readonly property bool expired: NativeDaemon.account.expired

  // Human readable plan name
  readonly property string planName: {
    if (!loggedIn || !username || !NativeDaemon.state.hasAccountToken) return '';
    if (!active) return uiTr("Deactivated");
    switch (plan) {
    case 'monthly': return uiTr("One Month Plan");
    case 'quarterly': return uiTr("Three Month Plan");
    case 'sixmontly': return uiTr("Six Month Plan");
    case 'yearly': return uiTr("One Year Plan");
    case 'biyearly': return uiTr("Two Year Plan");
    case 'three_years': return uiTr("Three Year Plan");
    case 'trial': return uiTr("Trial");
    default: return '';
    }
  }
}
