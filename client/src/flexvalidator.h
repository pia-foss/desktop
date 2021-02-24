// Copyright (c) 2021 Private Internet Access, Inc.
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
#line HEADER_FILE("flexvalidator.h")

#ifndef FLEXVALIDATOR_H
#define FLEXVALIDATOR_H
#include <QRegExpValidator>

// This validator works exactly like QRegExpValidator but
// gives the opportunity to fix the input before validation.
// Such fixes could involve trimming whitespace, forcing the input into a certain format, etc
// The fixInput() method must be implemented in QML by the user, and is optional.
// e.g: The following validator trims whitespace from the input before attempting validation
//    FlexValidator {
//      id: loginValidator
//      regExp: /(?:[0-9A-Za-z])+$/
//      function fixInput(input) { return input.trim() }
//    }
class FlexValidator : public QRegExpValidator
{
    Q_OBJECT

public:
    QValidator::State virtual validate(QString &input, int &pos) const
    {
        invokefixInputCallback(input);
        return QRegExpValidator::validate(input, pos);
    }

private:
    void invokefixInputCallback(QString &input) const
    {
        QVariant returnedValue;
        const QMetaObject *metaObject = this->metaObject();
        // Lookup the user-defined QML fixInput() method
        int methodIndex = metaObject->indexOfMethod("fixInput(QVariant)");

        // If the QML side did not implement a fixInput() method, then just return
        if(methodIndex < 0) return;

        QMetaMethod method = metaObject->method(methodIndex);
        // Invoke the user-defined QML fixInput() method
        // Requires a non-const QObject* so need to use const_cast to remove constness
        bool result = method.invoke(const_cast<FlexValidator*>(this), Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue),
            Q_ARG(QVariant, QVariant{input}));

        if(result)
            // modify the input as per the fixInput() method
            input = returnedValue.toString();
    }
};
#endif
