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

#include <common/src/common.h>
#line HEADER_FILE("progressbar.h")

#ifndef NATIVEACC_PROGRESSBAR_H
#define NATIVEACC_PROGRESSBAR_H

#include <QQuickItem>
#include <QAccessibleValueInterface>
#include "accessibleitem.h"
#include "accutil.h"

namespace NativeAcc {

// ProgressBar models a progress bar; it has the ProgressBar role and a value
// interface to provide min/max/value.
//
// ProgressBar's value and range are provided by bound properties, since these
// are usually custom in QML.
class ProgressBarAttached : public AccessibleItem, private QAccessibleValueInterface
{
    Q_OBJECT

    // The minimum and maximum value
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    // The current value
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)

public:
    ProgressBarAttached(QQuickItem &item);

public:
    double minimum() const {return _minimum;}
    void setMinimum(double minimum);
    double maximum() const {return _maximum;}
    void setMaximum(double maximum);
    double value() const {return _value;}
    void setValue(double value);

    // Overrides of AccessibleItem
    virtual QAccessibleValueInterface *valueInterface() override {return this;}

    // Implementation of QAccessibleValueInterface
    virtual QVariant currentValue() const override {return QVariant::fromValue(_value);}
    virtual QVariant maximumValue() const override {return QVariant::fromValue(_maximum);}
    // Progress bars can't be set, dummy step size
    virtual QVariant minimumStepSize() const override {return QVariant::fromValue(1.0);}
    virtual QVariant minimumValue() const override {return QVariant::fromValue(_minimum);}
    // Stub, progress bars can't be set
    virtual void setCurrentValue(const QVariant &) override {}

signals:
    void minimumChanged();
    void maximumChanged();
    void valueChanged();

private:
    double _minimum, _maximum, _value;
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(ProgressBar, ProgressBarAttached)

#endif
