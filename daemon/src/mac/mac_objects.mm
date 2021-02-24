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
#line SOURCE_FILE("mac_objects.mm")

#include "mac_objects.h"

CFIndex MacDict::getCount()
{
    if(!*this)
        return 0;

    return ::CFDictionaryGetCount(get());
}

CFHandle<CFTypeRef> MacDict::getValueObj(const void *key)
{
    if(!*this)
        return {};

    // Get rule, so retain a new reference to the result
    return {true, ::CFDictionaryGetValue(get(), key)};
}

std::pair<MacArray, MacArray> MacDict::getObjKeysValues()
{
    auto count = getCount();

    // Covers !*this and empty dictionaries (we would try to access the data
    // pointer of an empty vector without this check)
    if(count <= 0)
        return {};

    Q_ASSERT(*this);    // Consequence of count > 0

    // Allocate flat buffers for the dictionary to return its keys and values
    std::vector<CFTypeRef> keyBuf, valueBuf;
    keyBuf.resize(count);
    valueBuf.resize(count);

    ::CFDictionaryGetKeysAndValues(get(), keyBuf.data(), valueBuf.data());

    // Create CFArrays containing those items; passing kCFTypeArrayCallBacks
    // causes them to retain references to those objects.
    MacArray keyArray{::CFArrayCreate(nullptr, keyBuf.data(), count,
                      &kCFTypeArrayCallBacks)};
    MacArray valueArray{::CFArrayCreate(nullptr, valueBuf.data(), count,
                        &kCFTypeArrayCallBacks)};
    return {std::move(keyArray), std::move(valueArray)};
}

CFIndex MacArray::getCount() const
{
    if(!*this)
        return 0;
    return ::CFArrayGetCount(get());
}

CFHandle<CFTypeRef> MacArray::getObjAtIndex(CFIndex idx)
{
    if(!*this)
        return {};

    // Get rule - retain new reference
    return {true, ::CFArrayGetValueAtIndex(get(), idx)};
}
