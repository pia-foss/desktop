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

#ifndef STATICSIGNAL_H
#define STATICSIGNAL_H

#include "common.h"
#include <QObject>

// Class that can be used to define a "static" signal in a class.  Qt doesn't
// have any concept of a static signal; it has to be in an object.
//
// Instead, a static object of this type can be created and used as a plain
// signal.  Usually, the object should be named for the signal, such as:
//   - static StaticSignal myStaticVarChange;
//   ...meanwhile...
//   - QObject::connect(&myStaticVarChange, &StaticSignal::signal, this, ...);
//   ...elsewhere...
//   - emit myStaticVarChange.signal()
class COMMON_EXPORT StaticSignal : public QObject
{
    Q_OBJECT
signals:
    void signal();
};

#endif
