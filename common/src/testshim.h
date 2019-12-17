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

#include "common.h"
#line HEADER_FILE("testshim.h")

#ifndef TESTSHIM_H
#define TESTSHIM_H

#ifdef UNIT_TEST
#include <functional>
#include <typeindex>
#endif

// TestShim provides a way to create objects that can be (optionally) mocked in
// unit tests.
//
// Create objects dynamically with TestShim::create().  In normal builds, this
// just creates an object with new and returns it.
//
// In unit test builds, the unit test can install a mock with
// TestShim::installMock().  If the code under test attempts to instantiate the
// mocked class with TestShim::create(), the mock class is instantiated instead.
//
// This is mostly useful when the mocked type has virtual methods that should be
// reimplemented for unit tests, such as to mock out network requests.  It's
// less useful for non-virtual methods, since the code under test will still
// call the real implementation, but it still might be usable if the mocked
// type's constructor can alter the underlying object in some way that's useful
// for unit tests.
//
// The parameters passed to create() are not forwarded to the mock type's
// constructor currently.
namespace TestShim
{
#ifdef UNIT_TEST
    namespace detail_
    {
        // Install a creator function for a particular type.
        // The creator function must return a pointer to the type represented by
        // mockedType.
        COMMON_EXPORT void installMock(const std::type_index &mockedType,
                         std::function<void*()> mockCreator);

        // Create an instance of the mock for a type, if there is one.
        // The returned pointer is to the type represented by mockedType.
        // If there is no mock for this type, returns nullptr.
        COMMON_EXPORT void *createMock(const std::type_index &mockedType);
    }

    // Install a mock for a type in a unit test.
    // The type must be default-constructible (the arguments passed to create()
    // are not forwarded to the mock type's constructor)
    //
    // The type should be derived from the mocked type.  (A pointer to the type
    // must be convertible to a pointer of type MockedType.)
    template<class MockedType, class Mock>
    void installMock()
    {
        detail_::installMock({typeid(MockedType)},
                             [](){
                                MockedType *pMockObj{new Mock{}};
                                return reinterpret_cast<void*>(pMockObj);
                             });
    }
#endif

    // Create an object that can be replaced with a mock in unit tests.
    // The object is allocated with new, and the caller must ensure that it's
    // cleaned up.
    template<class T, class... Args>
    T *create(Args &&... args)
    {
#ifdef UNIT_TEST
        void *pMock = detail_::createMock({typeid(T)});
        if(pMock)
            return reinterpret_cast<T*>(pMock);
#endif

        return new T{std::forward<Args>(args)...};
    }
}

#endif // TESTSHIM_H
