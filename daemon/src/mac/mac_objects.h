// Copyright (c) 2023 Private Internet Access, Inc.
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
#line HEADER_FILE("mac_objects.h")

#ifndef MAC_OBJECTS_H
#define MAC_OBJECTS_H

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

// This file contains various wrapper types for CoreFoundation handles.  These
// are based on CFHandle, which reference counts the objects.
//
// Most of the methods here are wrappers for CF functions operating on these
// types.  Note that the CF* functions do _not_ check if the "this" reference
// (the first parameter) is nullptr, so all of the wrapper methods should check
// it.  (They do usually allow nullptrs in the remaining parameters.)
//
// The CF containers usually can hold arbitrary pointers or pointers to CF
// objects.  Typically the caller is just expected to know what's supposed to be
// in the container and manage references appropriately.  Most of these methods
// have *Obj* variants that are intended to work on containers of CF objects.
// (Many _only_ have *Obj* variants since those are the only ones being used
// currently.)

// Get the CFTypeID for a given Core Foundation type.
// Specialize this for each required CF type to return the appropriate
// "CF<Type>GetTypeID()".
template<class CFType>
CFTypeID macCfTypeId() {return 0;}

// Test if a CF object is a particular type.  Uses CFGetTypeID() and
// macCfTypeId().
template<class CFType>
bool macCfObjIsType(CFTypeRef obj)
{
    return obj && ::CFGetTypeID(obj) == macCfTypeId<CFType>();
}

// Anything is a CFTypeRef.
template<>
inline bool macCfObjIsType<CFTypeRef>(CFTypeRef obj)
{
    return obj;
}

// Core Foundation resource handle owner.  Releases the handle with CFRelease().
template<class CFRef_t>
class CFHandle
{
public:
    CFHandle() : _ref{} {}
    // Construction with a (non-nullptr) object reference takes ownership of a
    // reference previously retained by the caller (usually as a result of a
    // Copy*() or Create*() call).
    explicit CFHandle(CFRef_t ref) : _ref{ref} {}
    // CFHandle can retain a new reference with this constructor when
    // retain=true.  Use this when retrieving a value from a Get-rule function,
    // for example.
    CFHandle(bool retain, CFRef_t ref) : _ref{ref}
    {
        if(retain && _ref)
            ::CFRetain(_ref);
    }

    CFHandle(CFHandle &&other) : CFHandle() {*this = std::move(other);}
    CFHandle &operator=(CFHandle &&other) {std::swap(_ref, other._ref); return *this;}

    // Copies retain an additional reference
    CFHandle(const CFHandle &other) : CFHandle{true, other.get()} {}
    CFHandle &operator=(const CFHandle &other)
    {
        // Implemented with move ctor
        return *this = CFHandle{true, other.get()};
    }

    ~CFHandle() {reset();}

public:
    explicit operator bool() const {return _ref;}
    const CFRef_t &get() const {return _ref;}

    // Assign a new owned object (or no object).
    // Takes ownership of a reference that previously retained by the caller
    // (usually as a result of a Copy*() or Create*() call).
    void reset(CFRef_t newRef = nullptr)
    {
        if(_ref)
            ::CFRelease(_ref);
        _ref = newRef;
    }

    // Cast to another Core Foundation type.  A specialization of macCfTypeId()
    // must exist for the target type.
    // If the owned object isn't an object of that type, or if no object is
    // owned, a nullptr reference is returned.
    // In any case, this CFHandle keeps its reference to the object (an
    // additional reference is CFRetain()'d if necessary).
    template<class Target_t>
    CFHandle<Target_t> as() const
    {
        if(macCfObjIsType<Target_t>(_ref))
        {
            // The new CFHandle retains an additional reference.
            return CFHandle<Target_t>{true, reinterpret_cast<Target_t>(_ref)};
        }
        return {};
    }

private:
    CFRef_t _ref;
};

// CFArray wrapper.  Like MacDict, has methods intended to work on arrays of
// CF objects.
class MacArray : public CFHandle<CFArrayRef>
{
public:
    template<class CFObjType>
    class ObjView;

public:
    using CFHandle<CFArrayRef>::CFHandle;
    MacArray(CFHandle<CFArrayRef> array) : CFHandle<CFArrayRef>{std::move(array)} {}

public:
    CFIndex getCount() const;
    CFHandle<CFTypeRef> getObjAtIndex(CFIndex idx);

    // A MacArray of CF objects can be iterated using view<> with the CF type of
    // the objects in the array, such as CFStringRef (or CFTypeRef for generic
    // CF objects).
    // For example:
    //     for(const auto &stringElement : macArray.view<CFStringRef>)
    //         /*stringElement is CFHandle<CFStringRef>*/
    //     for(const auto &objElement : macArray.view<CFTypeRef>)
    //         /*objElement is CFHandle<CFTypeRef>*/
    template<class CFObjType>
    ObjView<CFObjType> view() {return {*this};}
};


// Iterable view of a MacArray that iterates CFHandle<CFObjType> values.
// Array elements that aren't of type CFObjType (or that are nullptr) are
// returned as nullptr values.
template<class CFObjType>
class MacArray::ObjView
{
public:
    class Iterator
    {
    public:
        Iterator() : _idx{0} {}
        Iterator(CFIndex idx, MacArray array) : _idx{idx}, _array{std::move(array)} {}

    public:
        bool operator==(const Iterator &other) const
        {
            return _idx == other._idx && _array.get() == other._array.get();
        }
        bool operator!=(const Iterator &other) const {return !(*this == other);}

        Iterator &operator++() {++_idx; return *this;}

        CFHandle<CFObjType> operator*()
        {
            return _array.getObjAtIndex(_idx).template as<CFObjType>();
        }

    private:
        CFIndex _idx;
        MacArray _array;
    };

public:
    ObjView(MacArray array) : _array{std::move(array)} {}

    Iterator begin() {return {0, _array};}
    Iterator end() {return {_array.getCount(), _array};}

private:
    MacArray _array;
};

// CFDictionary wrapper.  Most methods here are intended to work on dictionaries
// of CF objects, though CFDictionaries in principle can hold any void* values.
// ('*Obj*' methods apply to dictionaries of CF object references.)
class MacDict : public CFHandle<CFDictionaryRef>
{
public:
    using CFHandle<CFDictionaryRef>::CFHandle;
    MacDict(CFHandle<CFDictionaryRef> dict) : CFHandle<CFDictionaryRef>{std::move(dict)} {}

public:
    // Get the count of items in the dictionary
    CFIndex getCount();

    // Get a value from a dictionary, where that value is a CF object type.
    // CFDictionary allows any void* values as keys.
    CFHandle<CFTypeRef> getValueObj(const void *key);

    // Get all the keys and values from a dictionary of CF objects as MacArrays.
    // The returned arrays retain references to the returned objects.
    // The results are returned as a pair of arrays - the first array is the
    // array of keys, the second is the array of values.
    std::pair<MacArray, MacArray> getObjKeysValues();
};

// Wrapper for CFStringRef.  Mostly just makes bridging to QString and tracing
// slightly easier.
class MacString : public CFHandle<CFStringRef>
{
public:
    using CFHandle<CFStringRef>::CFHandle;
    MacString(CFHandle<CFStringRef> str) : CFHandle<CFStringRef>{std::move(str)} {}

public:
    QString toQString() const {return QString::fromCFString(get());}
    // TODO: don't go through QString
    std::string toStdString() const {return toQString().toStdString();}
};

inline std::ostream &operator<<(std::ostream &os, const MacString &str)
{
    return os << str.toQString();
}

// Specializations of macCfTypeId().
template<>
inline CFTypeID macCfTypeId<CFDictionaryRef>() {return ::CFDictionaryGetTypeID();}
template<>
inline CFTypeID macCfTypeId<CFStringRef>() {return ::CFStringGetTypeID();}
template<>
inline CFTypeID macCfTypeId<CFArrayRef>() {return ::CFArrayGetTypeID();}
template<>
inline CFTypeID macCfTypeId<CFNumberRef>() {return ::CFNumberGetTypeID();}
template<>
inline CFTypeID macCfTypeId<SCNetworkInterfaceRef>() {return ::SCNetworkInterfaceGetTypeID();}
#endif
