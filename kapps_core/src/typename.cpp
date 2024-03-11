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

#include "typename.h"
namespace kapps { namespace core {

// Prototype enum used to find name positions
enum NamePrototypeEnum
{
    // The value name does not contain "NamePrototypeEnum" to ensure we don't
    // mismatch the type name on the value name in the function prototype
    NamePrototypeValue,
};

detail_::NameSlicer::NameSlicer()
    : _typePrefixLen{}, _typeSuffixLen{}, _enumValPrefixLen{}, _enumValSuffixLen{}
{
    // Use a known prototype type to find where the type appears in the
    // function name.  Use an unadorned type like 'int' because a class type
    // can variously become 'Foo' or 'class Foo', etc.
    auto intFuncName = typeFunc<int>();

    // 'int' must appear exactly once - more than once and the prefix/suffix
    // model does not work
    StringSlice intName{"int"};
    auto intPos = intFuncName.find(intName);
    assert(intPos != StringSlice::npos);
    assert(intFuncName.find(intName, intPos+1) == StringSlice::npos);

    _typePrefixLen = intPos;
    _typeSuffixLen = intFuncName.size() - intName.size() - intPos;

    // Use the prototype enum to locate the enum value name.
    // On all supported compilers:
    // - the value name of an unscoped enumeration does not include the enum
    //   name itself (look for "kapps::core::NamePrototypeEnumValue")
    // - the enum name only appears once - in the prefix, except when part of
    //   the value name itself (we don't get something like
    //   "... [class T=enum TestEnum, TestEnum V = Value]", which would require
    //   compensating for the name length more than once).
    //
    // We do check these assumptions below; the algorithm could be generalized
    // if needed but this works for MSVC, GCC, and clang on all platforms we
    // support.
    auto valueFuncName = enumValueFunc<NamePrototypeEnum, NamePrototypeValue>();

    // The enum name appears exactly once in the prefix.  In other contexts it
    // may also appear as part of the value name, but that's fine as it just
    // becomes part of the result.
    auto enumName = sliceType(typeFunc<NamePrototypeEnum>());
    // The expected value name must appear exactly once
    StringSlice valueName{"kapps::core::NamePrototypeValue"};

    auto valPos = valueFuncName.find(valueName);
    assert(valPos != StringSlice::npos);
    assert(valueFuncName.find(valueName, valPos+1) == StringSlice::npos);

    // The enum name appears exactly once in the prefix
    auto enumValPrefix = valueFuncName.substr(0, valPos);
    auto enumNamePos = enumValPrefix.find(enumName);
    assert(enumNamePos != StringSlice::npos);
    assert(enumValPrefix.find(enumName, enumNamePos+1) == StringSlice::npos);
    // The enum name does not appear in the suffix
    assert(valueFuncName.substr(valPos+valueName.size()).find(enumName) == StringSlice::npos);

    _enumValPrefixLen = valPos - enumName.size();
    _enumValSuffixLen = valueFuncName.size() - valPos - valueName.size();
}

StringSlice detail_::NameSlicer::sliceType(StringSlice typeFuncName) const
{
    return typeFuncName.substr(_typePrefixLen,
        typeFuncName.size() - _typePrefixLen - _typeSuffixLen);
}

StringSlice detail_::NameSlicer::sliceEnumValue(StringSlice enumName, StringSlice enumValueFuncName) const
{
    unsigned prefixForEnum = _enumValPrefixLen + enumName.size();
    return enumValueFuncName.substr(prefixForEnum,
        enumValueFuncName.size() - prefixForEnum - _enumValSuffixLen);
}

const detail_::NameSlicer &detail_::nameSlicer()
{
    static const NameSlicer ns;
    return ns;
}

}}
