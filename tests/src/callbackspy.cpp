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

#include "callbackspy.h"
bool CallbackSpy::checkSingle() const
{
    if(_spy.size() != 1)
    {
        qWarning() << "Got" << _spy.size() << "signals, expected 1";
        return false;
    }

    return true;
}

bool CallbackSpy::checkError(Error::Code code) const
{
    if(!checkSingle())
        return false;

    // Verify that the Error code is correct.
    //
    // This is also used for success to check that the error code is Success.
    // QVariant::value() returns a default constructed value if the value isn't
    // of the expected type, so make sure it's really an Error to ensure that an
    // unexpected type results in a failure in that case.
    if(!_spy[0][0].canConvert<Error>())
    {
        qWarning() << "Result wasn't an error, expected code" << code << "- got"
            << _spy[0][0];
        return false;
    }

    if(_spy[0][0].value<Error>().code() != code)
    {
        qWarning() << "Expected error code" << code << "- got"
            << _spy[0][0].value<Error>();
        return false;
    }

    return true;
}

bool CallbackSpy::checkSuccess() const
{
    return checkError(Error::Code::Success);
}
