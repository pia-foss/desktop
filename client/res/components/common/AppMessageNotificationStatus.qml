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

import QtQuick 2.9
import "../client"
import "../daemon"
import "../settings"

NotificationStatus {
  property var appMessage
  message: active ? getTranslation(appMessage.messageTranslations) : ""
  links: active ? buildLinks() : []
  active: Daemon.settings.showAppMessages && (appMessage ? appMessage.id !== 0 : false)
  dismissed: appMessage.id === Daemon.settings.lastDismissedAppMessageId
  dismissible: true

  function dismiss() {
    Daemon.applySettings({lastDismissedAppMessageId: appMessage.id})
  }

  function buildLinks() {
    return appMessage.hasLink ? [{text: getTranslation(appMessage.linkTranslations), clicked: performAction}] : []
  }

  function performAction() {
    // Web links
    if(appMessage.uriAction) {
      Qt.openUrlExternally(appMessage.uriAction)
    }
    // View links
    // settings/foo  => to open a specific settings page
    // regions       => to open the regions list
    if(appMessage.viewAction) {
      var [wnd, pageName] = appMessage.viewAction.split("/")
      if(wnd == "settings") {
        ClientNotifications.showPage(pageName)
      }
      else if(wnd == "regions") {
        ClientNotifications.showRegions()
      }
    }
    // Settings changes
    // A settingsAction if it exists is a javascript object that
    // looks like this: {killswitch: "auto"}
    if(appMessage.settingsAction && Object.keys(appMessage.settingsAction).length != 0) {
      Daemon.applySettings(appMessage.settingsAction)
    }
  }

  // Display the translation for the current language
  function getTranslation(translations) {
    // Fall back to english if no translation available
    return translations[Client.settings.language] || translations["en-US"]
  }
}
