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
import QtQuick.Window 2.3
import QtGraphicalEffects 1.0
import "../common"
import "../core"
import "../daemon"
import "../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

Item {
  id: newChangelog

  readonly property int wireguardImageHeight: 272
  readonly property int contentMarginX: 25
  readonly property int contentMarginY: 25
  readonly property int subheadingImageWidth: 120

  implicitHeight: callingCardRoot.height
            
  //: Screen reader annotation for the "checkmark" bullets used in
  //: the What's New view
  readonly property string checkBulletLabel: uiTranslate("ChangelogWindow", "Checkmark bullet")

  Rectangle {
    id: callingCardRoot
    width: parent.width
    height: cardsLayoutColumn.height
    gradient: Gradient {
      GradientStop {
        position: 0.0
        color: Theme.changelog.whatsNewBackgroundGradientStart
      }
      GradientStop {
        position: 0.33
        color: Theme.changelog.whatsNewBackgroundGradientEnd
      }
      GradientStop {
        position: 1.0
        color: Theme.changelog.whatsNewBackgroundGradientEnd
      }
    }

    MouseArea {
      id: pma // parallax mouse area
      anchors.fill: parent
      hoverEnabled: true
    }

    // Column layout for different CTA items
    Column {
      id: cardsLayoutColumn
      width: parent.width
      spacing: 0

      ////////////////////////////////////
      // Calling Card Headline: Wireguard
      ///////////////////////////////////
      Item {
        id: wireguardRoot
        width: parent.width
        height: Math.max(wireguardTextContainer.y + wireguardTextContainer.height + contentMarginY,
                         wireguardFeatureImage.y + wireguardFeatureImage.height + 5)
        Image {
          id: wireguardBackgroundImage
          anchors.fill: parent
          source: Theme.changelog.wireguardBgImage
          rtlMirror: true
          opacity: Theme.light ? 0.25 : 1.0
        }

        // This is kept as a rectangle, so we can add a border to debug the bounding box
        // due to the nature of the wireguard "planet" image, there is some overlap of
        // text rectangle and the feature image. Drawing a border helps determine
        // exactly how much overlap is taking place, since we cannot rely on text alone
        Rectangle {
          color: "transparent"
          //border.color: "yellow"
          id: wireguardTextContainer
          anchors.top: wireguardRoot.top
          anchors.left: wireguardRoot.left
          anchors.right: wireguardFeatureImage.left
          anchors.topMargin: contentMarginY
          anchors.leftMargin: contentMarginX
          anchors.rightMargin: -30
          height: wireguardTextButtonsLayout.height
          ColumnLayout {
            id: wireguardTextButtonsLayout
            width: parent.width
            spacing: 0
            StaticText {
              id: wireguardHeadline
              Layout.fillWidth: true
              color: Theme.dashboard.textColor
              text: uiTranslate("ChangelogWindow", "Try WireGuard® today!")
              font.pixelSize: 32
              wrapMode: Text.WordWrap
            }

            StaticText {
              Layout.fillWidth: true
              Layout.topMargin: 14
              Layout.bottomMargin: 4
              color: Theme.dashboard.textColor
              text: uiTranslate(
                      "ChangelogWindow",
                      "It's a new, more efficient VPN protocol that offers:")
              font.pixelSize: 15
              wrapMode: Text.Wrap
            }

            RowLayout {
              Layout.topMargin: 14
              StaticImage {
                label: newChangelog.checkBulletLabel
                source: Theme.changelog.wireguardCheckImage
                width: 14
                height: 14
              }

              StaticText {
                Layout.leftMargin: 6
                color: Theme.dashboard.textColor
                font.pixelSize: 16
                text: uiTranslate("ChangelogWindow", "Better performance")
              }
            }

            RowLayout {
              Layout.topMargin: 6
              StaticImage {
                label: newChangelog.checkBulletLabel
                source: Theme.changelog.wireguardCheckImage
                width: 14
                height: 14
              }

              StaticText {
                Layout.leftMargin: 6
                color: Theme.dashboard.textColor
                font.pixelSize: 16
                text: uiTranslate("ChangelogWindow", "Lower CPU usage")
              }
            }

            RowLayout {
              Layout.topMargin: 6
              StaticImage {
                source: Theme.changelog.wireguardCheckImage
                label: newChangelog.checkBulletLabel
                width: 14
                height: 14
              }

              StaticText {
                Layout.leftMargin: 6
                color: Theme.dashboard.textColor
                font.pixelSize: 16
                text: uiTranslate("ChangelogWindow", "Longer battery life")
              }
            }

            RowLayout {
              Layout.topMargin: 22
              Item {
                id: wireguardCtaButton
                property bool enabled: {
                  return Daemon.settings.method !== "wireguard"
                      && Daemon.state.wireguardAvailable
                }

                Layout.minimumWidth: Math.max(wireguardCtaMessage.contentWidth + 30, 180)
                Layout.maximumWidth: Math.max(wireguardCtaMessage.contentWidth + 30, 180)
                Layout.preferredHeight: 28

                BorderImage {
                  width: Math.max(wireguardCtaMessage.contentWidth + 30, 180)
                  height: 28
                  border {left: 14; top: 14; right: 14; bottom: 14}
                  horizontalTileMode: BorderImage.Stretch
                  verticalTileMode: BorderImage.Stretch
                  source: wireguardCtaButton.enabled ? Theme.changelog.ctaButtonEnabledImage : Theme.changelog.ctaButtonDisabledImage
                }

                Text {
                  id: wireguardCtaMessage
                  anchors.centerIn: parent
                  font.capitalization: Font.AllUppercase
                  text: {
                    Daemon.settings.method
                        === "wireguard" ? uiTranslate(
                                            "ChangelogWindow",
                                            "WireGuard® is enabled") : uiTranslate(
                                            "ChangelogWindow",
                                            "Try WireGuard® now")
                  }
                  color: wireguardCtaButton.enabled ? Theme.changelog.wireguardCtaTextColor : Theme.changelog.wireguardCtaDisabledTextColor
                }

                ButtonArea {
                  enabled: wireguardCtaButton.enabled
                  anchors.fill: parent
                  name: wireguardCtaMessage.text
                  cursorShape: Qt.PointingHandCursor
                  onClicked: {
                    Daemon.applySettings({
                                           "method": "wireguard"
                                         })
                  }
                }
              }

              TextLink {
                underlined: true
                Layout.leftMargin: 6
                text: uiTranslate("ChangelogWindow", "Learn more")
                link: "https://www.privateinternetaccess.com/blog/wireguide-all-about-the-wireguard-vpn-protocol/"
              }
            }

            StaticText {
              Layout.topMargin: 5
              font.pixelSize: 12
              opacity: Theme.dark ? 0.6 : 0.9
              color: Theme.changelog.wireguardWarningColor
              visible: {
                return Daemon.state.connectionState === "Connected"
                    && Daemon.state.connectedConfig.method !== "wireguard"
                    && Daemon.settings.method === "wireguard"
              }

              text: uiTranslate("ChangelogWindow",
                                "Please reconnect to activate WireGuard®")
            }

            StaticText {
              Layout.topMargin: 5
              font.pixelSize: 12
              opacity: Theme.dark ? 0.6 : 0.9
              color: Theme.changelog.wireguardWarningColor
              visible: {
                return !Daemon.state.wireguardAvailable
              }
              linkColor: color
              text: {
                let src = uiTranslate(
                      "ChangelogWindow",
                      "Please [[click Here]] to re-install WireGuard Network Adapter")
                src = src.replace("[[", "<a href='#'>")
                src = src.replace("]]", "</a>")
                return src
              }
              MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
              }
              onLinkActivated: {
                wSettings.selectPage(wSettings.page.help)
                wSettings.showSettings()
              }
            }
          }
        }

        DropShadow {
          visible: Theme.dark
          // We cannot change the source/fill dynamically depending on
          // the theme, because that causes a crash in software mode.
          anchors.fill: wireguardTextContainer
          source: wireguardTextContainer
          color: Qt.rgba(0, 0, 0, 0.5)
          horizontalOffset: 0
          verticalOffset: 2
          radius: 4
          spread: 0
        }

        Item {
          id: wireguardFeatureImage

          // ranges from -0.5 to +0.5 depending on mouse's X/Y axis w.r.t the center point of the screen
          // each element moves w.r.t the center point as a function of this
          // A 0.6 multiplier is used to apply a global uniform reduction which can be decreased to further decrease the effect.
          readonly property double px: (pma.mouseX - pma.width / 2) / pma.width
                                       * wireguardFeatureImage.width * 0.4
          readonly property double py: (pma.mouseY - pma.width / 2) / pma.height
                                       * wireguardFeatureImage.height * 0.4

          anchors.right: wireguardRoot.right
          anchors.top: wireguardRoot.top
          anchors.topMargin: 20
          anchors.rightMargin: -10
          height: wireguardFeatureImageBgLayer.height
          width: wireguardImageHeight * 1.4

          // Just annotate the bottom layer of the stacked images
          StaticImage {
            id: wireguardFeatureImageBgLayer
            //: Screen reader annotation for the WireGuard feature image on the
            //: What's New view
            label: uiTranslate("ChangelogWindow", "WireGuard feature: robot on a rocket")
            source: Theme.changelog.wireguardPartCircle
            width: wireguardImageHeight * 1.6
            anchors.top: wireguardFeatureImage.top
            fillMode: Image.PreserveAspectFit
          }
          Image {
            width: wireguardImageHeight * 0.8
            fillMode: Image.PreserveAspectFit
            x: wireguardImageHeight * 0.84 + wireguardFeatureImage.px * 0.05
            y: wireguardImageHeight * 0.3 + wireguardFeatureImage.py * 0.05
            source: Theme.changelog.wireguardPartAsteroid
          }
          Image {
            source: Theme.changelog.wireguardPartMoon
            fillMode: Image.PreserveAspectFit
            width: wireguardImageHeight * 0.24
            x: wireguardImageHeight * 0.5 + wireguardFeatureImage.px * 0.08
            y: wireguardImageHeight * 0.1 + wireguardFeatureImage.py * 0.08
          }
          Image {
            source: Theme.changelog.wireguardPartRocket
            fillMode: Image.PreserveAspectFit
            width: wireguardImageHeight * 0.9
            x: wireguardImageHeight * 0.37 + wireguardFeatureImage.px * 0.15
            y: wireguardImageHeight * 0.1 + wireguardFeatureImage.py * 0.15
          }
        }
      }

      Item {
        width: 1
        height: 10
      }

      // END WIREGUARD CALLING CARD
      Rectangle {
        x: contentMarginX
        width: parent.width - 2*contentMarginX
        color: Theme.changelog.separatorBorderColor
        opacity: 0.25
        height: 1
      }

      Item {
        width: 1
        height: 20
      }

      // Calling card: Split tunnel
      RowLayout {
        x: contentMarginX
        width: parent.width - 2*contentMarginX

        StaticImage {
          id: splitTunnelImage
          //: Screen reader annotation for the split tunnel image in the
          //: What's New view
          label: uiTranslate("ChangelogWindow", "Split tunnel feature")
          Layout.preferredWidth: subheadingImageWidth
          Layout.preferredHeight: subheadingImageWidth * 421 / 400
          Layout.leftMargin: 15
          Layout.rightMargin: 15
          source: Theme.changelog.splitTunnelImage
          fillMode: Image.PreserveAspectCrop
        }

        ColumnLayout {
          Layout.fillWidth: true
          Layout.fillHeight: true
          Layout.leftMargin: contentMarginX

          StaticText {
            id: splitTunnelFeatureHeadline
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            text: uiTranslate("ChangelogWindow",
                              "Control which apps use our VPN!")
            color: Theme.dashboard.textColor
            font.pixelSize: 24
          }

          OneLinkMessage {
            id: splitTunnelFeatureText
            Layout.topMargin: 10
            Layout.fillWidth: true
            //: The [[double square brackets]] are replaced by a link. Please use them for the relavant link text.
            text: uiTranslate(
                    "ChangelogWindow",
                    "With our comprehensive split tunneling functionality, you can control which apps use the VPN and when. Check out our Split Tunnel [[app examples]] to see what's possible.")
            onLinkActivated: {
              Qt.openUrlExternally(
                    "https://www.privateinternetaccess.com/helpdesk/kb/articles/split-tunnel-app-examples")
            }
          }

          Item {
            Layout.fillHeight: true
          }
        }
      }

      Item {
        width: 1
        height: 25
      }

      Rectangle {
        x: contentMarginX
        width: parent.width - 2*contentMarginX
        color: "#979797"
        opacity: 0.25
        height: 1
      }

      Item {
        width: 1
        height: 20
      }

      // Calling card: CLI
      RowLayout {
        x: contentMarginX
        width: parent.width - 2*contentMarginX

        StaticImage {
          id: terminalFeatureImage
          //: Screen reader annotation for the command-line interface feature
          //: on the What's New view
          label: uiTranslate("ChangelogWindow", "Command-line interface feature")
          Layout.leftMargin: 15
          Layout.rightMargin: 15
          Layout.preferredWidth: subheadingImageWidth
          Layout.preferredHeight: subheadingImageWidth * 416 / 360
          source: Theme.changelog.terminalImage
        }

        ColumnLayout {
          Layout.fillWidth: true
          Layout.fillHeight: true
          Layout.leftMargin: contentMarginX

          StaticText {
            id: terminalFeatureHeadline
            text: uiTranslate(
                    "ChangelogWindow",
                    "Have you tried our command-line interface (CLI)?")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            color: Theme.dashboard.textColor
            font.pixelSize: 24
          }

          StaticText {
            id: terminalFeatureText1
            Layout.topMargin: 10
            Layout.fillWidth: true
            // This has to be one giant line for translation.
            //: CLI stands for command-line interface: https://en.wikipedia.org/wiki/Command-line_interface
            //: "piactl" is the name of the command used to control PIA from the CLI,
            //: this should not be translated.
            text: uiTranslate(
                    "ChangelogWindow",
                    "The PIA desktop app comes with a CLI for added functionality. Invoke \"piactl\" and check it out today! You can do things like control PIA, integrate PIA hotkeys, and even automate PIA functionality with scripts, or make custom integrations.")
            wrapMode: Text.WordWrap
            color: Theme.dashboard.textColor
            font.pixelSize: 13
          }
          OneLinkMessage {
            id: terminalFeatureText2
            Layout.topMargin: 10
            Layout.fillWidth: true

            //: The [[double square brackets]] are replaced by a link. Please use them for the relavant link text.
            text: uiTranslate(
                    "ChangelogWindow",
                    "Make something cool with the CLI? Show it off on the [[PIA Forum]]!")
            onLinkActivated: {
              Qt.openUrlExternally(
                    "https://www.privateinternetaccess.com/helpdesk/kb/articles/pia-desktop-command-line-interface")
            }
          }

          Item {
            Layout.fillHeight: true
          }
        }
      }

      Item {
        width: 1
        height: 20 + contentMarginY
      }
    }
  }
}
