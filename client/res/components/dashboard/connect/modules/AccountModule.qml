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

import QtQuick 2.9
import QtQuick.Controls 2.3
import "../../../client"
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  implicitHeight: 80
  moduleKey: 'account'

  //: Screen reader annotation for the "Subscription" tile, should reflect the
  //: name of the tile
  tileName: uiTr("Subscription tile")
  NativeAcc.Group.name: tileName

  Text {
    id: title
    text: uiTr("SUBSCRIPTION")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 20
    width: 260
    elide: Text.ElideRight
  }

  ValueHtml {
    id: plan
    //: Screen reader annotation for the subscription status display in the
    //: Subscription tile, usually the same as the tile name (but not all-caps)
    label: uiTr("Subscription")
    text: {
      var text = Daemon.account.planName || '<font color="%1">%2</font>'.arg(Theme.dashboard.moduleSecondaryTextColor).arg("---");
      if (Daemon.account.expired && !Daemon.account.needsPayment) {
        text = '<font color="%1">%2</font>'.arg(Theme.dashboard.notificationErrorLinkColor).arg(uiTr("Expired"));
      }
      if (!Daemon.account.recurring && Daemon.account.daysRemaining > 0) {
        text += '&nbsp; <font color="%1">%2</font>'.arg(Theme.dashboard.moduleSecondaryTextColor).arg(uiTr("(%1 days left)").arg(Daemon.account.daysRemaining));
      }
      return text;
    }

    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 20
    y: 40
    width: expiration.visible ? 170 : 260
    elide: Text.ElideRight
  }

  StaticText {
    id: expirationLabel
    visible: expiration.visible
    text: Daemon.account.expired ? uiTr("Expired on") : uiTr("Renews on")
    color: expiration.color
    font.pixelSize: Theme.dashboard.moduleSublabelTextPx
    x: expiration.x
    anchors.bottom: title.bottom
  }

  ValueText {
    id: expiration

    visible: Daemon.account.expirationTime > 0 && (Daemon.account.expired || (Daemon.account.recurring && Daemon.account.daysRemaining < 100))
    label: expirationLabel.text
    text: new Date(Daemon.account.expirationTime).toLocaleDateString(Qt.locale(Client.state.activeLanguage.locale), Locale.ShortFormat)
    color: Daemon.account.expired ? Theme.dashboard.notificationErrorLinkColor : Theme.dashboard.moduleSecondaryTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 200
    anchors.bottom: plan.bottom
  }
}
