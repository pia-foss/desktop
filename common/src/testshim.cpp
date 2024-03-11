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

#include "common.h"
#line SOURCE_FILE("testshim.cpp")

#include "testshim.h"

#ifdef UNIT_TEST
#include <QHash>

namespace TestShim
{
    namespace detail_
    {
        QMap<std::type_index, std::function<void*()>> mockCreators;

        void installMock(const std::type_index &mockedType,
                         std::function<void*()> mockCreator)
        {
            mockCreators[mockedType] = std::move(mockCreator);
        }

        void *createMock(const std::type_index &mockedType)
        {
            auto itCreator = mockCreators.find(mockedType);
            // There's a creator for this type, call it and return the mock.
            if(itCreator != mockCreators.end())
                return (*itCreator)();

            // There's no mock for this type.
            return nullptr;
        }
    }
}

#endif
