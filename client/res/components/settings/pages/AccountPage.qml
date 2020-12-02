// Copyright (c) 2020 Private Internet Access, Inc.
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
import QtQuick.Layouts 1.3
import "."
import "../inputs"
import "../stores"
import "../../common"
import "../../client"
import "../../daemon"
import "../../theme"
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.BrandHelper 1.0

Page {
  ColumnLayout {
    visible: Daemon.account.loggedIn
    anchors.fill: parent
    anchors.leftMargin: Theme.settings.narrowPageLeftMargin
    anchors.rightMargin: Theme.settings.narrowPageLeftMargin
    spacing: 6

    InputLabel {
      id: usernameLabel
      text: uiTr("Username")
    }
    ValueText {
      text: Daemon.account.username
      label: usernameLabel.text
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
    }

    // Spacer between groups
    Item {
      width: 1
      height: 4
    }

    InputLabel {
      id: subscriptionLabel
      text: uiTr("Subscription")
    }
    ValueHtml {
      label: subscriptionLabel.text
      text: {
        var text = Daemon.account.planName || '<font color="%1">%2</font>'.arg(Theme.settings.inputDescriptionColor).arg("---");
        if (Daemon.account.expired && !Daemon.account.needsPayment) {
          text = '<font color="%1">%2</font>'.arg(Theme.dashboard.notificationErrorLinkColor).arg(uiTr("Expired"));
        }
        if (Daemon.account.expirationTime) {
          var linkColor = Daemon.account.expired ? Theme.dashboard.notificationErrorLinkColor : Theme.settings.inputDescriptionColor

          var linkMsg
          if(Daemon.account.expired)
            linkMsg = uiTr("(expired on %1)")
          else if(Daemon.account.recurring)
            linkMsg = uiTr("(renews on %1)")
          else
            linkMsg = uiTr("(expires on %1)")

          var expDate = new Date(Daemon.account.expirationTime)
          var expDateStr = expDate.toLocaleString(Qt.locale(Client.state.activeLanguage.locale), Locale.ShortFormat)
          linkMsg = linkMsg.arg(expDateStr)

          text += '&nbsp; <font color="%1">%2</font>'.arg(linkColor).arg(linkMsg)
        }
        return text;
      }
      color: Theme.settings.inputLabelColor
      font.pixelSize: Theme.settings.inputLabelTextPx
    }
    InputDescription {
      visible: Daemon.account.expireAlert
      text: (Daemon.account.recurring ? uiTr("Renews in %1 days") : uiTr("Expires in %1 days")).arg(Daemon.account.daysRemaining)
    }
    TextLink {
      text: Daemon.account.plan === 'trial' ? uiTr("Purchase Subscription") : Daemon.account.recurring ? uiTr("Manage Subscription") : uiTr("Renew Subscription")
      underlined: true
      visible: (Daemon.account.expireAlert || Daemon.account.plan === 'trial') && Daemon.account.renewURL !== '' && BrandHelper.brandCode === 'pia'
      link: Daemon.account.renewURL
    }

    // Spacer between groups
    Item {
      width: 1
      height: 10
    }

    TextLink {
      text: uiTr("Manage My Account")
      underlined: true
      link: BrandHelper.getBrandParam("manageAccountLink")
    }
    TextLink {
      text: uiTr("Logout / Switch Account")
      underlined: true
      onClicked: Daemon.logout()
    }

    // Vertical spacer
    Item {
      Layout.fillHeight: true
      width: 1
    }
  }
  StaticText {
    visible: !Daemon.account.loggedIn
    text: uiTr("Not logged in");
    color: Theme.settings.inputLabelDisabledColor
    font.pixelSize: Theme.settings.inputLabelTextPx
  }
}
