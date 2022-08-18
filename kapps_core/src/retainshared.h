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

#pragma once
#include <kapps_core/core.h>
#include "logger.h"
#include "util.h"
#include <mutex>

namespace kapps::core {

// A std::enable_shared_from_this<> base that also exposes explicit "retain" and
// "release" operations.  This is used for shared-ownership objects that can be
// retained externally via the C API.
//
// retain() and release() should generally not be used for C++ internals or
// appliations using the C++ API - use a std::shared_ptr<> instead.  retain()
// and release() are only for the C API.
//
// Retainable objects can be created on the stack too (such as to move to or
// from).  Of course, std::shared_ptr cannot own these, so C API references
// can't be retained either, and libraries obviously cannot return these objects
// to API (obviously since they are on the stack, but also because they cannot
// be retained).
template<class T>
class RetainSharedFromThis : public std::enable_shared_from_this<T>
{
protected:
    // Like std::enable_shared_from_this, constructors are protected to prevent
    // accidental slicing.
    RetainSharedFromThis() = default;
    // Copying does not copy any references
    RetainSharedFromThis(const RetainSharedFromThis &other) : std::enable_shared_from_this<T>{} {}
    // Moving does not move any references
    RetainSharedFromThis(RetainSharedFromThis &&) : std::enable_shared_from_this<T>{} {}
    RetainSharedFromThis &operator=(const RetainSharedFromThis &other)
    {
        this->std::enable_shared_from_this<T>::operator=(other);
        return *this;
    }
    RetainSharedFromThis &operator=(RetainSharedFromThis &&other)
    {
        this->std::enable_shared_from_this<T>::operator=(std::move(other));
        return *this;
    }

    ~RetainSharedFromThis() = default;

public:
    // Retain a reference for the C API.  The object must have at least one
    // reference, either from a C++ std::shared_ptr or by the C API.  (The very
    // first reference must always be a std::shared_ptr.)
    void retain() const
    {
        std::unique_lock<std::mutex> lock{_apiRetainMutex};

        // If the retain count is 0, try to acquire the owner before actually
        // incrementing the ref count.  If there is no shared_ptr owner right
        // now, shared_from_this() will throw, and we want to leave the ref
        // count at 0 in that case.
        if(_apiRetainCount == 0)
        {
            try
            {
                _apiRetainOwner = this->shared_from_this();
            }
            catch(const std::exception &ex)
            {
                // Trace some specifics to add context.  This is an error in
                // either the library (providing an object to the C API that
                // can't be retained, i.e. there is no C++ owner), or in the
                // application (possibly retaining an object that was destroyed)
                KAPPS_CORE_ERROR() << "Object of type" << core::typeName<T>()
                    << "cannot be retained; this could be caused by a library or application error"
                    << ex.what();
                throw;
            }
        }
        ++_apiRetainCount;
    }

    void release() const
    {
        std::unique_lock<std::mutex> lock{_apiRetainMutex};

        if(_apiRetainCount == 0)
        {
            // This is always error in the application, it's trying to release
            // a reference when it does not own any.
            KAPPS_CORE_ERROR() << "Object of type" << core::typeName<T>()
                << "was released when no references were owned";
            throw std::runtime_error{"released reference that was not owned"};
        }

        if(--_apiRetainCount == 0)
        {
            // The C API no longer references this object, release
            // _apiRetainOwner.
            //
            // If there are no other std::shared_ptr references, this will
            // destroy the object.  That must happen after releasing the
            // mutex lock, which is correct because the object cannot be
            // re-referenced after all references are dropped.
            //
            // To do that, pull out the owner, then release the lock, and let
            // the local shared_ptr decide whether to destroy the object.
            std::shared_ptr<const T> lastApiOwner;
            lastApiOwner.swap(_apiRetainOwner);
            lock.unlock();
        }
    }

private:
    // When the C API has retained at least one reference, an internal
    // shared_ptr<> is used to hold a combined reference on the object.
    //
    // This may be possible with an atomic reference count (to avoid locking the
    // mutex for retain and release that don't touch the shared_ptr), but it is
    // trickier than a typical ref count because the C API retain count _can_
    // increase again from 0 to 1 (as long as another std::shared_ptr<> ref was
    // held internally).  Since this can occur concurrently with another thread
    // dropping the retain count from 1 to 0, we need to protect the reference
    // count and the internal shared_ptr together.
    //
    // These are mutable because they do not affect the observable state of the
    // object.
    mutable std::mutex _apiRetainMutex;
    mutable unsigned _apiRetainCount{0};
    mutable std::shared_ptr<const T> _apiRetainOwner;
};

}
