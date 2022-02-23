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
import QtQuick.Layouts 1.3
import "../client"
import "../core"
import "../daemon"

Item {
  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 10
    GroupBox {
      title: "Connected Server"
      Layout.fillWidth: true
      GridLayout {
        columns: 2
        Text {
          Layout.preferredWidth: 60
          text: "IP"
        }
        Text {
          text: {
            if(Daemon.state.connectedServer && Daemon.state.connectedServer.ip)
              return Daemon.state.connectedServer.ip
            return "---"
          }
        }
        Text {
          Layout.preferredWidth: 60
          text: "CN"
        }
        Text {
          text: {
            if(Daemon.state.connectedServer && Daemon.state.connectedServer.commonName)
              return Daemon.state.connectedServer.commonName
            return "---"
          }
        }
      }
    }

    GroupBox {
      title: "VPN locations"
      Layout.fillWidth: true
      GridLayout {
        columns: 2
        Text {
          Layout.preferredWidth: 60
          text: "Chosen"
        }
        Text {
          text: {
            if(Daemon.state.vpnLocations.chosenLocation)
              return Daemon.state.vpnLocations.chosenLocation.id
            return "Auto"
          }
        }
        Text {
          Layout.preferredWidth: 60
          text: "Best"
        }
        Text {
          text: {
            if(Daemon.state.vpnLocations.bestLocation)
              return Daemon.state.vpnLocations.bestLocation.id
            return "---"
          }
        }
        Text {
          Layout.preferredWidth: 60
          text: "Next"
        }
        Text {
          text: {
            if(Daemon.state.vpnLocations.nextLocation)
              return Daemon.state.vpnLocations.nextLocation.id
            return "---"
          }
        }
      }
    }

    GroupBox {
      title: "Manual Server"
      Layout.fillWidth: true
      GridLayout {
        columns: 3
        Text {
          Layout.preferredWidth: 120
          text: "IP"
        }
        TextField {
          id: manualIpText
          selectByMouse: true
        }
        RowLayout {
          Text {
            text: "OpenVPN cipher negotiation:"
          }
          RadioButton {
            id: manualOpenvpnPiaSS
            text: "pia-signal-settings"
          }
          RadioButton {
            id: manualOpenvpnNcp
            text: "NCP"
          }
        }
        Text {
          Layout.preferredWidth: 120
          text: "CN"
        }
        TextField {
          id: manualCnText
          selectByMouse: true
        }
        Item {width: 1; height: 1}
        Text {
          Layout.preferredWidth: 120
          text: "OpenVPN UDP Ports"
        }
        TextField {
          id: manualUdpPortsText
          selectByMouse: true
        }
        Text {
          text: "(Optional) Override OpenVPN UDP ports for this server.  Ex: \"1197,1198\"."
        }
        Text {
          Layout.preferredWidth: 120
          text: "OpenVPN TCP Ports"
        }
        TextField {
          id: manualTcpPortsText
          selectByMouse: true
        }
        Text {
          text: "(Optional) Override OpenVPN TCP ports for this server.  Ex: \"501,502\"."
        }
        Text {
          Layout.preferredWidth: 120
          text: "Region"
        }
        TextField {
          id: manualRegionText
          selectByMouse: true
        }
        Text {
          text: "(Optional) Corresponding PIA region ID, provides \"meta\" services"
        }
        Text {
          Layout.preferredWidth: 120
          text: "Groups"
        }
        TextField {
          id: manualGroupsText
          selectByMouse: true
        }
        Text {
          text: "(Optional) Server list group names.  Default is \"ovpntcp,ovpnudp,wg\""
        }
        Row {
          Layout.columnSpan: 3
          spacing: 5
          Item {
            Layout.fillWidth: true
          }
          Button {
            id: manualClearButton
            width: 80
            text: "Clear"
            onClicked: {
              manualIpText.text = ""
              manualCnText.text = ""
              manualOpenvpnNcp.checked = false
              manualOpenvpnPiaSS.checked = true
              manualUdpPortsText.text = ""
              manualTcpPortsText.text = ""
              manualRegionText.text = ""
              manualGroupsText.text = ""
              manualSaveButton.save()
            }
          }
          Button {
            id: manualSaveButton
            width: 80
            text: "Save"
            function save() {
              let groups = [], udpPorts = [], tcpPorts = []
              // If manualGroupsText.text is empty (""), leave groups empty too.
              // (Splitting an empty string yields [""], not [].)
              if(manualGroupsText.text)
                groups = manualGroupsText.text.split(',').map(s => s.trim())
              if(manualUdpPortsText.text)
                udpPorts = manualUdpPortsText.text.split(',').map(s => Number.parseInt(s.trim(), 10))
              if(manualTcpPortsText.text)
                tcpPorts = manualTcpPortsText.text.split(',').map(s => Number.parseInt(s.trim(), 10))
              // Update the groups text too in case the trim removed any
              // whitespace
              manualGroupsText.text = groups.join(',')
              Daemon.applySettings({
                manualServer: {
                  ip: manualIpText.text,
                  cn: manualCnText.text,
                  openvpnNcpSupport: manualOpenvpnNcp.checked,
                  openvpnUdpPorts: udpPorts,
                  openvpnTcpPorts: tcpPorts,
                  serviceGroups: groups,
                  correspondingRegionId: manualRegionText.text
                }
              })
            }
            function update() {
              manualIpText.text = Daemon.settings.manualServer.ip
              manualCnText.text = Daemon.settings.manualServer.cn
              manualOpenvpnNcp.checked = Daemon.settings.manualServer.openvpnNcpSupport
              manualOpenvpnPiaSS.checked = !manualOpenvpnNcp.checked
              manualUdpPortsText.text = Daemon.settings.manualServer.openvpnUdpPorts.join(',')
              manualTcpPortsText.text = Daemon.settings.manualServer.openvpnTcpPorts.join(',')
              manualGroupsText.text = Daemon.settings.manualServer.serviceGroups.join(',')
              manualRegionText.text = Daemon.settings.manualServer.correspondingRegionId
            }
            onClicked: manualSaveButton.save()

            Connections {
              target: Daemon.settings
              function onManualServerChanged(){manualSaveButton.update()}
            }
            Component.onCompleted: manualSaveButton.update()
          }
        }
      }
    }
  }
}
