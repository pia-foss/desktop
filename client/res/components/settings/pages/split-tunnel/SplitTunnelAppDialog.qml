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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import "../"


//
//
// "Add application" overlay
//
//
OverlayDialog {
  id: addApplicationDialog

  function openDialog() {
    // Skip the app list for Linux since we don't scan apps on Linux currently.
    if(Qt.platform.os === 'windows') {
      addApplicationDialog.open();
      SplitTunnelManager.scanApplications();
    } else {
      if (Qt.platform.os === 'osx')
        browseAppDialog.folder = 'file:///Applications'
      browseAppDialog.open();
    }
  }

  function strLocaleCompare(first, second) {
    return first.localeCompare(second, Client.state.activeLanguage.locale)
  }

  readonly property var scannedApplications: {
    var apps = SplitTunnelManager.scannedApplications
    // Apps are sorted by comparing [folders..., name] as if it was one array
    // for each entry.  That is, "Programs/B_Product.lnk" comes between
    // "Programs/A_Corp/Product.lnk" and "Programs/C_Corp/Product.lnk".
    // The naive way to do this would be to actually build a temporary array for
    // each entry and then lexically compare the entries, but building temporary
    // arrays in every comparison for a large sort tends to be very expensive.
    apps.sort(function(first, second) {
      // Compare folders arrays first
      var i, cmp
      for(i=0; i<first.folders.length && i<second.folders.length; ++i) {
        cmp = strLocaleCompare(first.folders[i], second.folders[i])
        if(cmp !== 0)
          return cmp
      }

      // Compare the next entry for each item
      var firstNextName = (i < first.folders.length) ? first.folders[i] : first.name
      var secondNextName = (i < second.folders.length) ? second.folders[i] : second.name
      cmp = strLocaleCompare(firstNextName, secondNextName)
      if(cmp !== 0)
        return cmp

      // Either first or second's [folders..., name] is an exact prefix of the
      // other.  The shorter one comes first.
      return first.folders.length - second.folders.length
    })
  }
  property int browseAppSelectedIndex: -1

  readonly property bool applicationScanRunning: SplitTunnelManager.scanActive
  readonly property var webkitApps: [
        "/Applications/Safari.app",
        "/Applications/Mail.app",
        "/Applications/App Store.app",
        // Paths are different on Catalina for Mail and App Store
        "/System/Applications/Mail.app",
        "/System/Applications/App Store.app"
  ]


  function addPathToSplitTunnelRules (path, linkTarget) {
    if(!SplitTunnelManager.validateCustomPath(path)) {
      if(Qt.platform.os === 'linux') {
        wSettings.alert(uiTr("Only executable files can be excluded from VPN. Please select an executable program or shell script."), uiTr("Unable to exclude application"),  'warning');
      }
      return;
    }
    var rules= JSON.parse(JSON.stringify(Daemon.settings.splitTunnelRules))

    // Check if the app that was added is a webkit app
    if(webkitApps.indexOf(path) >= 0) {
      // If a rule listing for webkit apps is not already available, then add it
      if(rules.findIndex(item => {return item.path === SplitTunnelManager.macWebkitFrameworkPath}) === -1) {
        rules.push({path: SplitTunnelManager.macWebkitFrameworkPath, mode: "exclude", linkTarget: ""});
      }
    }

    // If the path already exists don't add duplicate apps
    for(var i = 0; i < rules.length; i ++) {
      if(rules[i].path === path) {
        return;
      }
    }

    var rule = {
      path: path,
      linkTarget: linkTarget || "",
      mode: "exclude"
    };

    rules.push(rule);
    Daemon.applySettings({splitTunnelRules: rules});
  }

  FileDialog {
    id: browseAppDialog
    fileMode: FileDialog.OpenFile
    nameFilters: {
      if(Qt.platform.os === 'osx')
        return ["Applications (*.app)"]
      if(Qt.platform.os === 'windows')
        return ["Applications (*.exe)"]
      return ["All files (*)"]
    }
    onAccepted: {
      var path = file.toString()
      var file0S = "file:"
      var file2S = "file://"
      var file3S = "file:///"
      if(Qt.platform.os === 'windows')
      {
        // On Windows we can get `file:///C:/Program Files/Git/git-bash.exe`
        // or for a UNC path, `file://smb-server.example/data/git-bash.exe`
        if(path.startsWith(file3S))
          path = path.substring(file3S.length)
        else
          path = path.substring(file0S.length)
      }
      else
      {
        // On Unix platforms we get `file:///Applications/Skype.app`
        path = path.substring(file2S.length)
      }

      addPathToSplitTunnelRules(path);
      if(addApplicationDialog.opened) {
        addApplicationDialog.close();
      }
    }
  }

  buttons: [ Dialog.Ok, Dialog.Cancel ]
  canAccept: browseAppSelectedIndex >= 0
  title: uiTranslate("SplitTunnelAddAppRow", "Add Application")
  contentWidth: 300
  contentHeight: 300
  onAccepted: {
    var path = scannedApplications[browseAppSelectedIndex].path;
    // On Windows, we have to read the link target in the user session, not
    // in the service - see SplitTunnelRule::linkTarget
    var linkTarget
    if(Qt.platform.os === 'windows' && !path.startsWith("uwp:") && path.endsWith(".lnk")) {
      linkTarget = SplitTunnelManager.readWinLinkTarget(path)
      // Bail if we failed to read the link target (traced by
      // SplitTunnelManager).  Should not normally happen since we check link
      // targets before displaying links, but it could happen if the link was
      // changed while the dialog was open.
      if(!linkTarget)
        return
    }
    addPathToSplitTunnelRules(path, linkTarget)
  }

  Item {
    height: 300
    width: 300

    ColumnLayout {
      anchors.fill: parent
      spacing: 5
      RowLayout {
        Layout.fillWidth: true
        spacing: 5

        // Search bar
        Rectangle {
          Layout.fillWidth: true
          height: 35
          color: Theme.settings.splitTunnelInputBackgroundColor
          border.color: Theme.settings.splitTunnelInputBorderColor
          ThemedTextField {
            id: searchField
            anchors.fill: parent
            anchors.margins: 3
            palette.text: Theme.settings.splitTunnelInputTextColor
            background: Item{}
            placeholderText: uiTr("Search")
            label: placeholderText
          }
        }

        // "Browse" button
        Rectangle {
          width: 85
          height: 35
          color: Theme.settings.inputButtonBackgroundColor
          Text {
            id: browseButtonText
            color: Theme.settings.inputButtonTextColor
            text: uiTr("Browse")
            anchors.centerIn: parent
          }

          ButtonArea {
            anchors.fill: parent
            name: browseButtonText.text
            onClicked: browseAppDialog.open()
            cursorShape: Qt.PointingHandCursor
          }
        }
      }

      // List of applications
      Rectangle {
        id: scannedAppList
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Theme.settings.hbarBackgroundColor
        border.color: Theme.settings.hbarBottomBorderColor
        border.width: 1

        // scannedAppList handles keyboard nav for the list elements.
        activeFocusOnTab: true

        // Loading indicator
        Item {
          anchors.fill: parent
          visible: applicationScanRunning

          Image {
            id: spinnerImage
            height: 40
            width: 40
            source: Theme.settings.spinnerImage
            anchors.centerIn: parent

            RotationAnimator {
              target: spinnerImage
              running: applicationScanRunning
              from: 0;
              to: 360;
              duration: 1000
              loops: Animation.Infinite
            }
          }
        }

        ThemedScrollView {
          id: scannedAppScrollView
          visible: !applicationScanRunning
          ScrollBar.vertical.policy: ScrollBar.AlwaysOn
          label: uiTr("Applications")
          anchors.fill: parent
          contentWidth: parent.width
          contentHeight: scannedAppLayout.implicitHeight
          clip: true

          Flickable {
            id: scannedAppFlickable
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: false

            ColumnLayout {
              id: scannedAppLayout
              width: parent.width
              spacing: 0

              Repeater {
                id: scannedAppRepeater

                model: scannedApplications

                //
                // "Add application" delegate
                //
                delegate: Item {
                  id: appRow
                  Layout.fillWidth: true
                  readonly property string appName: modelData.name
                  readonly property string appPath: modelData.path
                  readonly property var includedApps: modelData.includedApps

                  readonly property int heightNoSubtitle: 30
                  Layout.preferredHeight: includedApps.length === 0 ? heightNoSubtitle : 40

                  readonly property bool matchesSearch: {
                    // For performing a search on the list of applications,
                    // We perform the search check on each item in the repeater instead
                    // of filtering the model data
                    //
                    // We do this because filtering the model data appears to cause the app
                    // icon extraction to re-run more frequently because QML elements are
                    // added and removed more frequently, triggering the "image://appicon" provider
                    //
                    // By doing this, all QML elements are cached and are hidden when search results are
                    // not valid
                    if(searchField.text.length > 0) {
                      if(appName.toLowerCase().includes(searchField.text.toLowerCase()))
                        return true
                      for(var i=0; i<includedApps.length; ++i) {
                        if(includedApps[i].toLowerCase().includes(searchField.text.toLowerCase()))
                          return true
                      }
                      return false
                    }

                    return true;
                  }
                  visible: matchesSearch

                  readonly property string includedAppsLine: {
                    var sortedApps = includedApps.slice().sort(function(first, second) {
                      first = first.toLowerCase()
                      second = second.toLowerCase()
                      if(first < second)
                        return -1
                      if(first > second)
                        return 1
                      return 0
                    })
                    return sortedApps.join(", ")
                  }

                  readonly property NativeAcc.TableRow accRow: NativeAcc.TableRow {
                    name: appName
                    item: appRow
                    selected: isSelected
                    outlineExpanded: false
                    outlineLevel: 0
                  }

                  readonly property NativeAcc.TableCellButton accAppCell: NativeAcc.TableCellButton {
                    name: {
                      if(includedAppsLine)
                        return appName + " - " + includedAppsLine
                      return appName
                    }
                    item: appRow
                    onActivated: mouseClicked()
                  }

                  readonly property bool isSelected: index === browseAppSelectedIndex

                  function mouseClicked() {
                    scannedAppList.forceActiveFocus(Qt.MouseFocusReason)
                    browseAppSelectedIndex = index
                  }

                  // highlight if item is selected
                  Rectangle {
                    color: Theme.settings.inputDropdownSelectedColor
                    anchors.fill: parent
                    anchors.leftMargin: 1
                    anchors.rightMargin: 1
                    visible: isSelected
                    opacity: 0.4
                  }

                  SplitTunnelAppIcon {
                    x: (heightNoSubtitle - height) / 2
                    y: (parent.height - height) / 2
                    // 20px fits the layout ideally, but Windows icons scale
                    // poorly (designers rarely include a 20x20 size), so use
                    // 16 instead.  This also works well for 200% scale (32 is
                    // better than 40); at non-integer scale the icons will
                    // still be scaled.
                    width: Qt.platform.os === 'windows' ? 16 : 20
                    height: width
                    appPath: modelData.path
                  }

                  Text {
                    y: 4
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    text: appName
                    color: Theme.settings.inputLabelColor
                    elide: Text.ElideRight
                  }

                  Text {
                    y: 22
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    text: includedAppsLine
                    color: Theme.settings.inputDescriptionColor
                    font.pixelSize: 10
                    elide: Text.ElideRight
                  }

                  Rectangle {
                    anchors.bottom: parent.bottom
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    color: Theme.settings.splitTunnelItemSeparatorColor
                    opacity: 0.3
                  }

                  // Plain MouseArea - keyboard nav and accessibility are
                  // handled by scannedAppList
                  MouseArea {
                    anchors.fill: parent
                    onClicked: mouseClicked()
                  }
                } // delegate
              } // Repeater scannedAppRepeater
            } // ColumnLayout scannedAppLayout
          } // Flickable
        } // ThemedScrollView

        // Representation of the rows as a flat table suitable for use with
        // keyboard nav utilities.  (Mainly recaptures the controls' visibility
        // as "disabled" - we don't want to put that on the original table, it
        // would cause the rows to be rebuilt during search.)
        property var accTable: {
          // Manual dependency on children - itemAt() doesn't add this
          var childrenDep = scannedAppRepeater.children

          var table = []
          for(var i=0; i<scannedAppRepeater.count; ++i) {
            var appItem = scannedAppRepeater.itemAt(i)
            if(!appItem)
              continue
            table.push({name: appItem.appName, path: appItem.appPath,
                        disabled: !appItem.matchesSearch, item: appItem})
          }

          return table
        }

        Keys.onPressed: {
          var nextIndex = KeyUtil.handleVertKeyEvent(event, accTable, 'name',
                                                     browseAppSelectedIndex)

          if(nextIndex !== -1) {
            browseAppSelectedIndex = nextIndex
            focusCue.reveal()

            // Scroll to reveal this item
            var item = scannedAppRepeater.itemAt(browseAppSelectedIndex)
            if(item) {
              var itemBound = item.mapToItem(scannedAppFlickable.contentItem,
                                             0, 0, item.width, item.height)
              Util.ensureScrollViewVertVisible(scannedAppScrollView,
                                               scannedAppScrollView.ScrollBar.vertical,
                                               itemBound.y, itemBound.height)
            }
          }
        }

        OutlineFocusCue {
          id: focusCue
          anchors.fill: parent
          control: scannedAppList
        }

        //: Screen reader annotation for the "application list" from which a
        //: user can select an application to exclude.
        NativeAcc.Table.name: {
          if(applicationScanRunning) {
            //: Screen reader annotation used for the split tunnel app list when
            //: the apps are still being loaded.
            return uiTr("App list, loading")
          }
          //: Screen reader annotation for the split tunnel app list.
          return uiTr("App list")
        }

        property NativeAcc.TableColumn appColumn: NativeAcc.TableColumn {
          //: Screen reader annotation for the column listing the application
          //: in the existing application list (this table has only one column)
          name: uiTr("App")
          item: scannedAppList
        }

        NativeAcc.Table.columns: [
          { property: "app", column: appColumn }
        ]

        NativeAcc.Table.rows: {
          var tblRows = []
          var accRow
          for(var i=0; i<accTable.length; ++i) {
            accRow = accTable[i]
            tblRows.push({id: accRow.appPath, row: accRow.item.accRow,
                          app: accRow.item.accAppCell})
          }

          return tblRows
        }

        NativeAcc.Table.navigateRow: browseAppSelectedIndex
        NativeAcc.Table.navigateCol: 0  // Just one column
      } // Rectangle scannedAppList
    } // ColumnLayout

    TextLink {
      text: uiTr("Refresh")
      anchors.left: parent.left
      anchors.top: parent.bottom
      anchors.topMargin: 36
      enabled: !applicationScanRunning
      underlined: enabled
      onClicked: {
        SplitTunnelManager.scanApplications(true);
      }
    }
  }
}
