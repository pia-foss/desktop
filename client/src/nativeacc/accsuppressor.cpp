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

#include "common.h"
#line SOURCE_FILE("accsuppressor.cpp")

#include "accsuppressor.h"
#include <QQuickItem>

namespace NativeAcc {

QAccessibleInterface *AccSuppressor::interfaceFactory(const QString &classname,
                                                      QObject *pObject)
{
    // If it's a QQuickItem, return a stub.  We've already missed our chance to
    // return a NativeAcc implementation at this point.
    if(dynamic_cast<QQuickItem*>(pObject))
        return new AccSuppressor{pObject};
    return nullptr;
}

}
