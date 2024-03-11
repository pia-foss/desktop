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
import QtQuick.Controls 2.3
import QtQuick.Window 2.3
import QtQuick.Layouts 1.3
import "../../../../javascript/app.js" as App
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  id: performanceModule
  implicitHeight: 160
  moduleKey: 'performance'

  //: Screen reader annotation for the "Performance" tile containing the
  //: performance graph.
  tileName: uiTr("Performance tile")
  NativeAcc.Group.name: tileName

  readonly property double maxOnChart: {
    // The maximum on the chart must be at least 1 so we can divide by it to
    // scale the bars.  This works correctly if all bars have height 0; they use
    // the 1-pixel minimum height.
    var max = 1
    for(var i=0; i<Daemon.state.intervalMeasurements.length; ++i)
      max = Math.max(max, Daemon.state.intervalMeasurements[i].received)
    return max
  }
  readonly property int limitItems: 32
  readonly property int barWidth: 8
  // The last measurement from intervalMeasurements if any measurement is
  // available, undefined if no measurement is available.
  readonly property var lastInterval: {
    if(Daemon.state.intervalMeasurements.length > 0)
      return Daemon.state.intervalMeasurements[Daemon.state.intervalMeasurements.length-1]
    // otherwise, undefined
  }

  // Length of an interval in seconds
  readonly property int intervalSec: 5

  // Current timestamp, ticks on 1 second intervals while connected.  Uses the
  // same monotonic clock as Daemon.state.connectionTimestamp.
  property real currentTimestamp: 0

  // Update the current timestamp every 1 second while connected, so the timer
  // ticks.  The actual duration is computed from the connection and current
  // timestamps, instead of explicitly ticking it 1 second every timer interval,
  // so it won't be affected by drift, missed timers due to system load, etc.
  Timer {
    id: durationTimer
    interval: 1000
    repeat: true
    running: Daemon.state.connectionTimestamp > 0
    onTriggered: currentTimestamp = NativeHelpers.getMonotonicTime()
    onRunningChanged: {
      // When the timer starts (due to the connection being established, or due
      // to loading this component when the connection is already up),
      // immediately set the current timestamp so we show the correct time
      // before the timer elapses the first time.
      if(running) {
        if(currentTimestamp === 0)
          currentTimestamp = NativeHelpers.getMonotonicTime()
      }
      else
        currentTimestamp = 0
    }
  }

  Text {
    text: uiTr("PERFORMANCE")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
  }

  function formatMbps(val) {
    // val contains number of bytes, convert to number of bits
    val *= 8
    // Intevals are 5 seconds long
    val /= intervalSec

    // Low values are rounded down to 0 Mbps. Show anything below 0.5 Mbps as kbps
    if(val > 500 * 1000) {
      return uiTr("%1 Mbps").arg((val / (1000 * 1000)).toFixed(1))
    } else {
      // Unlike 'Mbps', 'kbps' is preferred over 'Kbps' - capital K for 'kilo'
      // is nonstandard even in data rate measurements.  'Kbps' is still
      // somewhat commonly used, but 'kbps' still seems to be more common.
      return uiTr("%1 kbps").arg(Math.round(val/ 1000))
    }

  }

  Rectangle {
    id: chartWrapper
    color: Theme.dashboard.performanceChartBackgroundColor
    border.color: Theme.dashboard.performanceChartBorderColor
    radius: 4
    x: 20
    y: 36
    height: 80
    width: 260

    // The chart can be focused to navigate the bars with arrow keys.
    activeFocusOnTab: true

    //: Screen reader annotation for the "Performance" graph, which lists recent
    //: performance measurements.
    NativeAcc.Chart.name: uiTr("Performance history")

    Rectangle {
      id: bandwidthTooltip

      readonly property int tooltipEdgeMargin: 5

      x: {
        if(!barsList.highlightBar)
          return 0

        // Get the X-coordinate of the bar in our parent's coordinate system
        var x = barsList.highlightBar.mapToItem(parent, 0, 0).x
        // We can't be closer to the left edge than the margin
        var minX = tooltipEdgeMargin
        // We can't be closer to the right edge than the width of the tooltip
        // plus the margin
        var maxX = parent.width - width - tooltipEdgeMargin

        x = Math.min(maxX, x)
        x = Math.max(minX, x)
        return x
      }
      y: tooltipEdgeMargin
      visible: !!barsList.highlightBar
      height: 16
      // The width of the whole tooltip is the right edge of the text
      width: tooltipText.x + tooltipText.implicitWidth
      color: 'transparent'
      Image {
        source: Theme.dashboard.performanceDownloadImage
        width: 9
        height: 12
        x: 0
        y: 2
      }

      Text {
        id: tooltipText
        x: 13
        y: 0
        text: {
          if(barsList.highlightBar)
            return formatMbps(barsList.highlightBar.bytesReceived)
          return ''
        }
        color: Theme.dashboard.performanceChartText
      }
    }

    // The bars are laid out statically (they don't depend on the interval
    // measurements model) so we don't get new cursor-enter events when the
    // measurements change.
    RowLayout {
      id: barsList

      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      // Leave space for the bandwidth indicator above
      height: 0.7 * parent.height
      width: limitItems * barWidth
      layoutDirection: Qt.RightToLeft
      spacing: 0

      readonly property int visibleBarCount: Math.min(Daemon.state.intervalMeasurements.length, limitItems)

      // The bar that's hovered by the cursor, if any
      readonly property var cursorBarIndex: {
        // Only bars that actually show data can be pointed
        for(var i=0; i<visibleBarCount && i<barsRepeater.count; ++i) {
          var bar = barsRepeater.itemAt(i)
          if(bar.barHovered)
            return i
        }
        return -1
      }

      // The index of the bar that was last highlighted with the keyboard
      property int keyboardBarIndex: -1

      // The index of the bar that is ultimately highlighted, taking into
      // account either cursor or keyboard highlight
      readonly property var highlightBarIndex: {
        // Prefer keyboard if the last input was from the keyboard (and it's
        // really a valid bar).  Input from the cursor clears keyboardBarIndex
        if(chartFocusCue.show && keyboardBarIndex >= 0 &&
           keyboardBarIndex < visibleBarCount)
          return keyboardBarIndex

        return cursorBarIndex
      }

      // The actual bar that's highlighted
      readonly property var highlightBar: barsRepeater.itemAt(highlightBarIndex)

      Repeater {
        id: barsRepeater
        model: limitItems

        Item {
          id: intervalBar
          Layout.fillHeight: true
          width: barWidth

          // index=0 is the newest bar (the last measurement, at the right edge)
          readonly property var measurement: {
            var intervalIndex = Daemon.state.intervalMeasurements.length - 1 - index
            if(intervalIndex >= 0 && intervalIndex < Daemon.state.intervalMeasurements.length)
              return Daemon.state.intervalMeasurements[intervalIndex]
            // Otherwise undefined
          }

          property alias barHovered: hoverWatch.containsMouse
          readonly property double bytesReceived: measurement ? measurement.received : 0

          // Show only when there's actually a measurement for this position
          readonly property bool hasMeasurement: !!measurement

          readonly property int secondsAgo: (index+1) * intervalSec
          readonly property int nameMinutePart: Math.floor(secondsAgo / 60)
          readonly property int nameSecondPart: secondsAgo % 60

          NativeAcc.ValueText.name: {
            // No annotation if there's no measurement for this interval
            if(!hasMeasurement)
              return ''

            // qsTr() supports a plural disambiguation, but we haven't verified
            // whether this works with OneSky, and here we have two values to
            // negotiate anyway.  Only a few combinations are actually valid
            // though since the range is from 0:05 - 2:45 on multiples of 5.
            if(nameSecondPart === 0) {
              if(nameMinutePart === 1) {
                //: Performance graph, 1:00 ago (screen reader annotation)
                return uiTr("1 minute ago")
              }
              if(nameMinutePart === 2) {
                //: Performance graph, 2:00 ago (screen reader annotation)
                return uiTr("2 minutes ago")
              }
            }
            else {
              if(nameMinutePart === 0) {
                //: Performance graph, 0:05-0:55 ago, always a multiple of 5 (screen reader annotation)
                return uiTr("%1 seconds ago").arg(nameSecondPart)
              }
              else if(nameMinutePart == 1) {
                //: Performance graph, 1:05-1:55 ago, always a multiple of 5 (screen reader annotation)
                return uiTr("1 minute %1 seconds ago").arg(nameSecondPart)
              }
              else if(nameMinutePart == 2) {
                //: Performance graph, 2:05-2:55 ago, always a multiple of 5 (screen reader annotation)
                return uiTr("2 minutes %1 seconds ago").arg(nameSecondPart)
              }
            }

            console.warn('No annotation configured for %1 minutes %2 seconds'.arg(nameMinutePart).arg(nameSecondPart))
            return ''
          }

          NativeAcc.ValueText.value: {
            var speed = formatMbps(bytesReceived)
            //: Screen reader annotation for performance bar - download speed
            //: from a past interval, like "3.5 Mbps download speed".
            //: Speed uses the "%1 Mbps" or "%1 kbps" string from the
            //: performance graph
            return uiTr("%1 download speed").arg(speed)
          }

          MouseArea {
            id: hoverWatch
            hoverEnabled: true
            anchors.fill: parent
            // Propagate clicks to the background
            propagateComposedEvents: true
            // When a new bar becomes hovered, treat that as a cursor input, clear
            // the keyboard highlight if it's active
            onEntered: {
              if(measurement)
                barsList.keyboardBarIndex = -1
            }
            // When the chart is clicked, grab focus so keyboard events will
            // work as expected
            onClicked: {
              if(measurement)
                barsList.keyboardBarIndex = -1
              chartWrapper.forceActiveFocus(Qt.MouseFocusReason)
            }
          }
          Rectangle {
            color: index === barsList.highlightBarIndex ? Theme.dashboard.performanceChartBarActive : Theme.dashboard.performanceChartBarInactive
            // Show at least 1 px for each bar
            height: Math.max(1, parent.height * bytesReceived / maxOnChart)
            width: barWidth / 2
            x: barWidth / 4
            y: parent.height - height
            visible: hasMeasurement
          }
        }
      }
    }

    Keys.onPressed: {
      // Start from the currently-highlighted bar, regardless of whether that's
      // due to keyboard or cursor interaction
      var nextIndex = barsList.highlightBarIndex
      // These lay out right-to-left, directions are reversed
      if(event.key === Qt.Key_Left) {
        // Left doesn't need any special logic for nextIndex < 0
        ++nextIndex
      }
      else if(event.key === Qt.Key_Right) {
        // If no bar is highlighted, start from the left edge
        nextIndex = (nextIndex >= 0) ? nextIndex - 1 : barsList.visibleBarCount-1
      }
      else if(event.key === Qt.Key_Home)
        nextIndex = barsList.visibleBarCount-1
      else if(event.key === Qt.Key_End)
        nextIndex = 0
      else  // Not a key we care about, ignore
        return

      // Keep the index within the limits
      nextIndex = Math.max(0, nextIndex)
      nextIndex = Math.min(barsList.visibleBarCount-1, nextIndex)
      barsList.keyboardBarIndex = nextIndex
      chartFocusCue.reveal()
      event.accepted = true
    }
  }

  OutlineFocusCue {
    id: chartFocusCue
    anchors.fill: chartWrapper
    control: chartWrapper
  }

  StaticImage {
    id: downloadArrow
    source: Theme.dashboard.performanceDownloadImage
    width: 9
    height: 12
    x: 23
    y: 130
    //: Screen reader annotation for the "download" arrow labeling the download
    //: speed display
    label: uiTr("Download speed")
  }

  ValueText {
    id: currentDownload
    x: 36
    y: 128
    text: lastInterval ? formatMbps(lastInterval.received) : '---'
    label: downloadArrow.label
    color: Theme.dashboard.performanceChartText
  }

  StaticImage {
    id: uploadArrow
    source: Theme.dashboard.performanceUploadImage
    width: 9
    height: 12
    x: 122
    y: 130
    //: Screen reader annotation for the "upload" arrow labeling the upload
    //: speed display
    label: uiTr("Upload speed")
  }

  ValueText {
    id: currentUpload
    x: 135
    y: 128
    text: lastInterval ? formatMbps(lastInterval.sent) : '---'
    label: uploadArrow.label
    color: Theme.dashboard.performanceChartText
  }

  StaticImage {
    id: durationClock
    anchors.right: durationText.left
    anchors.rightMargin: 4
    y: 130
    source: Theme.dashboard.performanceDurationImage
    width: 12
    height: 12
    //: Screen reader annotation for the clock icon labeling the connection
    //: duration display
    label: uiTr("Connection duration")
  }

  function formatTimePart(value) {
    // Leading zeroes might or might not be desirable for all languages - a
    // format like '3 h 45' might not want them.  The leading zero is rendered
    // using a localizable resource so this can be controlled per-language.
    //
    // The numbers are always rendered using Arabic numerals.  It's not clear
    // whether some languages might prefer to use different numerals for a
    // duration - other fields like port numbers do not change, for example.
    //
    // If we set the default locale, we could leave this up to the translator to
    // choose between %1/%L1, but setting the default locale has many other
    // effects (such as QString::toInt()/toDouble(), scroll bar positions, etc.)
    // that are very widespread and risk other issues.  Unfortunately, there's
    // no way to just call 'arg()' with a specific locale, it always uses the
    // default.
    if(value < 10) {
      //: Render a time part (hours/minutes/seconds) for the connection duration
      //: that's less than 10.  This can pad the value with a leading 0, such as
      //: '0%1'.
      return uiTr("0%1", "short-time-part").arg(value)
    }
    //: Render a time part (hours/minutes/seconds) for the connection duration
    //: that's 10 or more.
    return uiTr("%1", "long-time-part").arg(value)
  }

  ValueText {
    id: durationText

    // Normally, use a fixed width similar to the up/down widths, and anchor to
    // the right of the chart.
    // Some localizations might be slightly too long - if necessary, scoot this
    // to the left slightly so the right edge still meets the right of the
    // chart.
    anchors.right: chartWrapper.right
    y: 128
    width: Math.max(implicitWidth, 47)

    text: {
      if(Daemon.state.connectionTimestamp === 0)
        return '---'

      var timeElapsed = Math.round((currentTimestamp - Daemon.state.connectionTimestamp) / 1000)
      var hours = Math.floor(timeElapsed / 3600)
      var mins = Math.floor((timeElapsed % 3600) / 60)
      var sec = (timeElapsed % 60)

      if(hours > 0) {
        //(translator comment)
        //: Connection duration template for 1 hour or more - %1 is hours, %2 is minutes
        //: Hours and minutes are rendered with the time-part or short-time-part strings
        return uiTr("%1:%2", "duration-hours-minutes").arg(formatTimePart(hours)).arg(formatTimePart(mins))
      }
      //: Connection duration template for less than 1 hour - %1 is minutes, %2 is seconds
      //: Minutes and seconds are rendered with the time-part or short-time-part strings
      return uiTr("%1:%2", "duration-minutes-seconds").arg(formatTimePart(mins)).arg(formatTimePart(sec))
    }
    label: durationClock.label

    color: Theme.dashboard.performanceChartText
  }
}
