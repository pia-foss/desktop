// Copyright (c) 2019 London Trust Media Incorporated
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
// DashboardContentInterface is the interface used to describe the dashboard
// content to DashboardWindow/DashboardPopup.  This defines the 'content'
// properties in both DashboardWindow and DashboardPopup.
QtObject {
  // Current height of the content
  property int pageHeight
  // Likely maximum page height of the content when expanded (used to place
  // dashboard)
  property int maxPageHeight
  // Header top color - used to color the arrow when shown
  property color headerTopColor
}
