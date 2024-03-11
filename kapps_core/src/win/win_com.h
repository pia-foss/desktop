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

#pragma once
#include "winapi.h"
#include <kapps_core/src/logger.h>
#include <kapps_core/core.h>
#include <Unknwn.h>
#include <utility>
#include <cassert>

// TODO - kapps::core namespace

// Owning COM pointer.  Owns a reference to a COM interface.
template<class ComClass>
class WinComPtr
{
public:
    // Create a single instance of a COM class using ::CoCreateInstance()
    static WinComPtr createInst(REFCLSID clsid, DWORD dwClsContext, REFIID iid)
    {
        WinComPtr<ComClass> pResult;
        HRESULT createErr = ::CoCreateInstance(clsid, nullptr,
            dwClsContext, iid, pResult.receiveV());
        if(FAILED(createErr))
        {
            KAPPS_CORE_WARNING() << "Unable to create COM object -" << createErr;
        }
        return pResult;
    }
    // Create a single in-process instance of a COM class
    static WinComPtr createInprocInst(REFCLSID clsid, REFIID iid)
    {
        return createInst(clsid, CLSCTX_INPROC_SERVER, iid);
    }

    // Create a single _local server_ instance of a COM class using ::CoCreateInstance()
    static WinComPtr createLocalInst(REFCLSID clsid, REFIID iid)
    {
        return createInst(clsid, CLSCTX_LOCAL_SERVER, iid);
    }

public:
    WinComPtr() : _pObj{} {}
    explicit WinComPtr(ComClass *pObj) : _pObj{pObj} {}
    WinComPtr(const WinComPtr &other) : WinComPtr{} {*this = other;}
    WinComPtr(WinComPtr &&other) : WinComPtr{} {*this = std::move(other);}
    ~WinComPtr() {reset();}

    WinComPtr &operator=(const WinComPtr &other)
    {
        reset(other._pObj);
        if(_pObj)
            _pObj->AddRef();
        return *this;
    }
    WinComPtr &operator=(WinComPtr &&other)
    {
        std::swap(_pObj, other._pObj);
        return *this;
    }

public:
    ComClass *operator->()
    {
        assert(_pObj);
        return _pObj;
    }
    ComClass &operator*()
    {
        assert(_pObj);
        return *_pObj;
    }

    explicit operator bool() const {return _pObj;}
    bool operator!() const {return !_pObj;}

    ComClass *get() const {return _pObj;}
    operator ComClass *() const {return _pObj;}

    // Take ownership of a new object.  (Does not add a reference, takes
    // ownership of a reference obtained by the caller.)
    void reset(ComClass *pObj = nullptr)
    {
        if(_pObj)
            _pObj->Release();
        _pObj = pObj;
    }

    // Get a writeable pointer-to-pointer to receive a COM object, from
    // CoCreateInstance(), etc.  Either ComClass** or void** can be used
    // (ComClass** is common in specific "creator" methods, void** is common for
    // general methods like QueryInterface).
    //
    // If an object is currently owned, it's released - the returned writeable
    // pointer is initially nullptr.
    //
    // If an object is acquired and written to the pointer given, a reference
    // becomes owned by WinComPtr.  The resulting pointer-to-pointer should not
    // normally be stored or reused, instead call receive() again to ensure no
    // objects are leaked.
    ComClass **receive() {reset(); return &_pObj;}
    void **receiveV() {return reinterpret_cast<void**>(receive());}

    // Query for a different interface to this object.  Returns nullptr if the
    // interface isn't supported.
    // The caller must provide both the interface type and IID; these must
    // match.
    // If no object is held, this just returns nullptr.
    template<class OtherComClass>
    WinComPtr<OtherComClass> queryInterface(REFIID iid)
    {
        WinComPtr<OtherComClass> pResult;
        if(_pObj)
            _pObj->QueryInterface(iid, pResult.receiveV());
        return pResult;
    }

private:
    ComClass *_pObj;
};

// Owned COM variant.
//
// The VARIANT is always initialized with VariantInit() - it's safe to
// reinitialize over this, since this just marks the variant as VT_EMPTY.
//
// For COM functions that take a pointer to a VARIANT and populate it, use
// receive() to ensure the VARIANT is clear and then obtain a writeable pointer.
class KAPPS_CORE_EXPORT WinComVariant
{
public:
    WinComVariant() {::VariantInit(&_variant);}
    // Not copiable/movable (could be implemented though)
    WinComVariant(const WinComVariant &) = delete;
    WinComVariant &operator=(const WinComVariant &) = delete;
    ~WinComVariant() {clear();}

public:
    void clear() {::VariantClear(&_variant);}
    VARIANT *receive() {clear(); return &_variant;}
    VARIANT &get() {return _variant;}

private:
    VARIANT _variant;
};

// COM initializer - owns the initialization count (uninitializes in its
// destructor).  It's a QObject so it can be parented to a worker thread's
// object owner.
//
// If COM initialization fails, WinComInit traces the failure and continues
// anyway (callers have to be prepared for COM object creation to fail anyway).
class KAPPS_CORE_EXPORT WinComInit
{
public:
    WinComInit();
    ~WinComInit();

private:
    bool _comInitialized;
};
