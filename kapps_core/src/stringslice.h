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
#include <kapps_core/core.h>
#include <kapps_core/stringslice.h>
#include <kapps_core/arrayslice.h>
#include "util.h"
#include <vector>
#include <cassert>
#include <string>
#include <locale>
#include <ostream>
#include <array>
#include <stdexcept>
#include <limits>

namespace kapps { namespace core {

// ArraySlice is a non-owning view of any contiguous range of elements.  It can
// be a slice of a std::vector, std::array, std::string, a byte buffer from
// some platform API, etc.
//
// This is useful to pass contiguous data around without arbitrarily requiring
// some ownership model, like a const std::vector<T>& would.  The data can also
// be re-sliced, iterated, etc., which are nontrivial with a plain data pointer
// and range.
//
// The underlying data can be either mutable (ArraySlice<some_type>) or
// immutable (ArraySlice<const some_type>).
//
// ArraySlice<const T> forms the basis for BasicStringSlice but is also useful
// on its own.
template<class T>
class ArraySlice
{
public:
    // Mutable T if T is const; used for additional constructors below
    using mT = typename std::remove_const<T>::type;

public:
    using iterator = T*;
    using const_iterator = const T*;
    static constexpr size_t npos = -1;

public:
    // Create an empty slice
    ArraySlice() : _pBegin{}, _pEnd{} {}

    // Create a slice from a range - T* begin and end
    ArraySlice(T *pBegin, T *pEnd)
        : _pBegin{pBegin}, _pEnd{pEnd}
    {
        // The data pointers can both be nullptr, but if one is valid the other
        // must be too
        assert((pBegin && pEnd) || pBegin == pEnd);
    }
    // Create a slice from a range - T* and count
    ArraySlice(T *pData, std::size_t count)
        : ArraySlice{pData, pData + count}
    {
        // pData can be nullptr only if count is zero
        assert(pData || !count);
    }

    // If T is const, ArraySlice can be constructed from ArraySlice<mT>.
    // (If T is non-const, this actually user-defines the copy constructor, but
    // the behavior is the same and it's not straightforward to cram a
    // std::enable_if<> in here since no part of the method call is deduced.)
    ArraySlice(const ArraySlice<mT> &array)
        : ArraySlice{array.data(), array.size()}
    {}

    // Create an ArraySlice from a std::vector.
    //
    // If T is const, we need a const std::vector<mT> &.  (std::vector<T>
    // doesn't exist in that case; std::vector requires a non-CV type.)
    //
    // If T is mutable, we need a std::vector<T>& - it must be mutable.
    ArraySlice(typename copy_const<T, std::vector<mT>>::type &array)
        : ArraySlice{array.data(), array.size()}
    {}

    // Create an ArraySlice from std::array.  Unlike std::vector, std::array
    // does accept a const type.  Also, std::array<T, N> is _not_ convertible to
    // std::array<const T, N>.
    //
    // So we need to accept:
    // - for any T -> std::array<T> &  (must be mutable array of mutable elements)
    // - for const T -> (const) std::array<T> &  (any array of const elements)
    // - for const T -> (const) std::array<mT> &  (any array of mutable elements)
    //
    // In each case, allow the compiler to deduce ElementT, even though it must
    // be T or mT (it can't be a convertible type), then apply enable_if to
    // enforce our requirements.

    // 1. For any T -> std::array<T> &
    template<class ElementT, std::size_t N,
             class = typename std::enable_if<std::is_same<ElementT, T>::value>::type>
    ArraySlice(std::array<ElementT, N> &array)
        : ArraySlice{array.data(), array.size()}
    {}
    // 2. For const T -> const std::array<T> &
    // 3. For const T -> (const) std::array<mT> &
    // Checking that 'const ElementT' == 'T' verifies that T is const (it can't
    // possibly match const ElementT otherwise) and allows ElementT to be either
    // T or mT.
    template<class ElementT, std::size_t N,
             class = typename std::enable_if<std::is_same<const ElementT, T>::value>::type>
    ArraySlice(const std::array<ElementT, N> &array)
        : ArraySlice{array.data(), array.size()}
    {}

    // Create an ArraySlice from T[].
    //
    // Like std::array, a regular array can contain const types.  Unlike
    // std::array, a const& to a regular array makes no difference, as
    // references themselves can't be modified.  Also unlike std::array,
    // references to U[] can be converted to a reference to const U[].
    //
    // As a result, T(&)[] is the only type we need to accept.
    template<std::size_t N>
    ArraySlice(T (&array)[N])
        : ArraySlice{&array[0], N}
    {}

public:
    T &operator[](std::size_t pos) {return _pBegin[pos];}
    const T &operator[](std::size_t pos) const {return _pBegin[pos];}

    explicit operator bool() const {return !empty();}
    bool operator!() const {return empty();}

    std::size_t hash() const
    {
        std::size_t h{};
        for(const T &v : *this)
            h = hashAccumulate(h, v);
        return h;
    }

private:
    // Returns iterators to the first non-equal pair of elements in *this and
    // other, or {end(), other.end()} if all elements match.
    //
    // If the slices differ in size and all prefix elements match, returns
    // {end(), <mismatch>} or {<mismatch>, other.end()} depending on which
    // slice is longer.
    auto mismatch(const ArraySlice &other) const
        -> std::pair<const_iterator, const_iterator>
    {
        auto itVal = begin();
        auto itOtherVal = other.begin();
        while(itVal != end() && itOtherVal != other.end())
        {
            // Find the first pair of mismatch values
            if(!(*itVal == *itOtherVal))
                break;
            // Values are the same, keep checking
            ++itVal;
            ++itOtherVal;
        }

        return {itVal, itOtherVal};
    }

public:
    bool operator==(const ArraySlice &other) const
    {
        if(size() != other.size())
            return false;
        // Sizes are the same, so just check if there's any mismatch
        // (they're equal if mismatch() reached the end of the slices)
        return mismatch(other).first == end();
    }
    bool operator!=(const ArraySlice &other) const {return !(*this == other);}
    bool operator<(const ArraySlice &other) const
    {
        auto mismatchPos = mismatch(other);
        // If there was no mismatching element in this, this is less if other
        // is longer, otherwise they are the same.
        if(mismatchPos.first == end())
            return mismatchPos.second != other.end();
        // If there was no mismatching element in other, this is equal or
        // greater (not less in any case)
        if(mismatchPos.second == other.end())
            return false;
        // Otherwise, there's a mismatching element in both, this is less if our
        // element is less.
        return *mismatchPos.first < *mismatchPos.second;
    }
    bool operator>(const ArraySlice &other) const {return other < *this;}
    bool operator<=(const ArraySlice &other) const {return !(*this > other);}
    bool operator>=(const ArraySlice &other) const {return !(*this < other);}

public:
    // front() and back() - undefined if the array is empty
    T &front() {assert(!empty()); return *_pBegin;}
    const T &front() const {assert(!empty()); return *_pBegin;}
    T &back() {assert(!empty()); return *(_pEnd-1);}
    const T &back() const {assert(!empty()); return *(_pEnd-1);}

    // data() - undefined if the array is empty
    T *data() {return _pBegin;}
    const T *data() const {return _pBegin;}

    // begin() and end() - return equal iterators if the array is empty
    iterator begin() {return _pBegin;}
    const_iterator begin() const {return _pBegin;}
    const_iterator cbegin() const {return _pBegin;}
    iterator end() {return _pEnd;}
    const_iterator end() const {return _pEnd;}
    const_iterator cend() const {return _pEnd;}

    bool empty() const {return _pBegin == _pEnd;}
    std::size_t size() const {return _pEnd - _pBegin;}
    std::size_t length() const {return size();}

    ArraySlice subslice(std::size_t pos = 0, std::size_t count = npos) const
    {
        if(pos > size())
            throw std::out_of_range("pos out of range in ArraySlice::subslice()");
        std::size_t resultLen = std::min(count, size() - pos);
        return {_pBegin + pos, _pBegin + pos + resultLen};
    }

    // Create a vector containing this ArraySlice's content
    std::vector<mT> to_vector() const {return {begin(), end()};}

private:
    T *_pBegin;
    T *_pEnd;
};

// Simple string slice class - std::basic_string_view() requires C++17.  This
// is a non-owning view of string data, which is particularly useful to pass
// around string data without arbitrarily requiring some ownership model, like
// a const std::string & would.
//
// StringSlice does not guarantee that its contents are null-terminated.
//
// The foundation of BasicStringSlice is an ArraySlice<const CharT>.
// BasicStringSlice adds on behavior that makes sense when the values form a
// character string, such as comparison, prefix matching, searching, etc.
//
// The string data must outlive the StringSlice (just as it would when storing
// a const char *, etc.)
template<class CharT, class Traits = std::char_traits<CharT>>
class BasicStringSlice : public ArraySlice<const CharT>
{
public:
    // Member of a template-parameter-dependent base class, would require
    // qualification everywhere otherwise
    static constexpr auto npos = ArraySlice<const CharT>::npos;

public:
    using ArraySlice<const CharT>::ArraySlice;

    // BasicStringSlice can be constructed from a matching basic_string.
    BasicStringSlice(const std::basic_string<CharT, Traits> &str)
        : ArraySlice<const CharT>{str.data(), str.data() + str.size()}
    {}

    // In addition to ArraySlice's constructors, BasicStringSlice can be
    // constructed from a null-terminated string.  Note that when creating a
    // BasicStringSlice from a char[] (where the size is known at compile time,
    // ArraySlice's array constructor is preferred.
    //
    // This is usually ideal because it avoids a Traits::length() (however minor
    // that may be), but the behavior may be unexpected if the character array
    // contained embedded nulls, etc.
    BasicStringSlice(const CharT *pStr)
        : ArraySlice<const CharT>{pStr, pStr ? pStr+Traits::length(pStr) : nullptr}
    {
    }

public:
    bool operator==(BasicStringSlice other) const {return compare(other) == 0;}
    bool operator!=(BasicStringSlice other) const {return compare(other) != 0;}
    bool operator<(BasicStringSlice other) const {return compare(other) < 0;}
    bool operator>(BasicStringSlice other) const {return compare(other) > 0;}
    bool operator<=(BasicStringSlice other) const {return compare(other) <= 0;}
    bool operator>=(BasicStringSlice other) const {return compare(other) >= 0;}

public:
    int compare(BasicStringSlice other) const
    {
        int prefixCompare = Traits::compare(this->data(), other.data(),
            std::min(this->size(), other.size()));
        if(prefixCompare != 0)
            return prefixCompare;

        // Prefixes are the same; if one string is longer, it is greater.
        if(this->size() > other.size())
            return 1;
        if(this->size() < other.size())
            return -1;
        return 0;
    }

    bool starts_with(BasicStringSlice value) const
    {
        return substr(0, value.size()) == value;
    }
    bool starts_with(CharT c) const
    {
        return !this->empty() && Traits::eq(this->front(), c);
    }
    bool ends_with(BasicStringSlice value) const
    {
        // If the string is smaller than the value, then it doesn't end with it.
        // Necessary so (size()-value.size()) below is nonnegative.
        if(this->size() < value.size())
            return false;
        return substr(this->size()-value.size(), value.size()) == value;
    }
    bool ends_with(CharT c) const
    {
        return !this->empty() && Traits::eq(this->back(), c);
    }
    bool contains(BasicStringSlice value) const
    {
        return find(value) != npos;
    }
    bool contains(CharT c) const
    {
        return find(c) != npos;
    }

    BasicStringSlice substr(std::size_t pos = 0, std::size_t count = npos) const
    {
        if(pos > this->size())
            throw std::out_of_range("pos out of range in BasicStringSlice::substr()");
        std::size_t resultLen = std::min(count, this->size() - pos);
        return {this->data() + pos, this->data() + pos + resultLen};
    }

    std::size_t find(BasicStringSlice value, std::size_t pos = 0) const
    {
        auto suffix = substr(pos);
        auto itMatch = std::search(suffix.begin(), suffix.end(), value.begin(),
            value.end(), &Traits::eq);
        if(itMatch == suffix.end())
            return npos;
        return itMatch - this->begin();
    }
    std::size_t find(CharT c, std::size_t pos = 0) const
    {
        return find({&c, &c+1}, pos);
    }
    // rfind
    // find_first_of
    // find_first_not_of
    // find_last_of
    // find_last_not_of

    // Convert to a std::string - the result is always UTF-8 and null-terminated.
    // For StringSlice this just copies the data; for WStringSlice it's
    // converted to UTF-8 from UTF-16/32 (depending on the platform's wchar_t).
    std::string to_string() const;

    // Find the common prefix between this string slice and another.  If there
    // is none, returns an empty string slice.  The returned slice references
    // this slice's data.
    BasicStringSlice common_prefix(const BasicStringSlice &other) const
    {
        auto itChar = this->begin();
        auto itOtherChar = other.begin();
        while(itChar != this->end() && itOtherChar != other.end() &&
            Traits::eq(*itChar, *itOtherChar))
        {
            ++itChar;
            ++itOtherChar;
        }

        return {this->begin(), itChar};
    }
};

using StringSlice = BasicStringSlice<char>;
using WStringSlice = BasicStringSlice<wchar_t>;

namespace detail_
{
    // Get either char16_t or char32_t depending on the size of wchar_t.  This is
    // needed in the template parameters to std::codecvt, so we can tell it we
    // want UTF-16/32 -> UTF-8 rather than the system's wide and narrow character
    // sets (the system's narrow char set on Windows might not be UTF-8).
    template<std::size_t> class UnicodeChar;
    template<> class UnicodeChar<sizeof(char16_t)> { public: using T = char16_t; };
    template<> class UnicodeChar<sizeof(char32_t)> { public: using T = char32_t; };

    // Convert a string sliec to UTF-8.  Provide a sink function where blocks
    // of converted characters will be passed (as char* and std::size_t)
    template<class CharT, class Traits, class ToUtf8SinkFuncT>
    void toUtf8(BasicStringSlice<CharT, Traits> value, ToUtf8SinkFuncT sinkFunc)
    {
        // Use codecvt<char{16,32}_t, char>, not codecvt<wchar_t, char>.  The
        // char{16,32}_t one converts to UTF-8; the wchar_t one converts to "the
        // system's narrow character set".
        using UCharT = typename UnicodeChar<sizeof(CharT)>::T;
        // std::codecvt has a protected destructor
        class SliceCodecvt : public std::codecvt<UCharT, char, std::mbstate_t>
        {
        public:
            using std::codecvt<UCharT, char, std::mbstate_t>::codecvt;
        };
        SliceCodecvt cvt;
        std::mbstate_t state{};
        std::array<char, 256> outBuf;

        const UCharT *pIn{reinterpret_cast<const UCharT*>(value.begin())};
        const UCharT *pEnd{reinterpret_cast<const UCharT*>(value.end())};
        char *pOutNext{outBuf.data()};
        while(pIn != pEnd)
        {
            const UCharT *pNext{pIn};
            switch(cvt.out(state, pIn, pEnd, pNext, outBuf.data(),
                outBuf.data() + outBuf.size(), pOutNext))
            {
                case std::codecvt_base::noconv:
                    // The codecvt says it is non-converting; just truncate each
                    // character.  This shouldn't happen though since we are always
                    // converting char{16,32}_t to char.
                    while(pNext != pEnd)
                    {
                        // Crudely truncate and write one char at a time, this
                        // shouldn't occur anyway
                        char nextTrunc{static_cast<char>(*pNext)};
                        sinkFunc(&nextTrunc, 1);
                        ++pNext;
                    }
                    break;
                case std::codecvt_base::ok:
                case std::codecvt_base::partial:
                    sinkFunc(outBuf.data(), pOutNext - outBuf.data());
                    break;
                default:
                case std::codecvt_base::error:
                    // An invalid character was encountered - it's not clear whether
                    // any prior valid characters would have been converted.  Write
                    // any valid output that was produced, then write a replacement
                    // character and skip a character.
                    sinkFunc(outBuf.data(), pOutNext - outBuf.data());
                    sinkFunc("\xEF\xBF\xBD", 3);   // U+FFFD "Replacement Character" in UTF-8
                    ++pNext;
                    break;
            }

            pIn = pNext;
        }
    }

    // Specialization; no need to convert for a char string slice, it's already
    // UTF-8
    template<class Traits, class ToUtf8SinkFuncT>
    void toUtf8(BasicStringSlice<char, Traits> value, ToUtf8SinkFuncT sinkFunc)
    {
        sinkFunc(value.data(), value.size());
    }
}

template<class T>
inline std::ostream &operator<<(std::ostream &os, const ArraySlice<T> &value)
{
    auto itVal = value.begin();
    if(itVal == value.end())
    {
        os << "()"; // Empty slice
        return os;
    }

    os << '(' << *itVal;
    ++itVal;
    while(itVal != value.end())
    {
        os << ", " << *itVal;
        ++itVal;
    }
    os << ')';
    return os;
}

inline std::ostream &operator<<(std::ostream &os, StringSlice value)
{
    os.write(value.data(), value.size());
    return os;
}

template<class CharT, class Traits>
std::ostream &operator<<(std::ostream &os, BasicStringSlice<CharT, Traits> value)
{
    detail_::toUtf8(value, [&](const char *pText, std::size_t len)
    {
        os.write(pText, len);
    });
    return os;
}

template<class CharT, class Traits>
std::string BasicStringSlice<CharT, Traits>::to_string() const
{
    std::string result;
    // Guess that the string is probably mostly ASCII, so the number of chars
    // will remain about the same
    result.reserve(this->size());
    detail_::toUtf8(*this, [&](const char *pText, std::size_t len)
    {
        result.append(pText, len);
    });
    return result;
}

// Parse a decimal integer to a numeric type.  The string must contain only:
// - an optional negative sign (if the target type is signed)
// - one or more digits
//
// If the string is not a valid integer, a std::runtime_error is thrown.
// If the string exceeds the range for the target type, a std::range_error is
// thrown.
//
// (Unlike std::strtol() and friends:
//  - leading whitespace is not allowed
//  - trailing non-numeric chars are not allowed
//  - unary plus sign is not allowed
//  - locale-specific formats are not allowed
//  - the string does not need to be null-terminated)
//
// NOTE: The range_error conditions are slightly imprecise, it's possible to
// throw a range_error when a runtime_error is more appropriate.  For
// example: parseInteger<std::int8_t>("128eeee") throws range_error because the
// addition of '8' overflows, but the remaining characters aren't even digits
// at all.
template<class IntegerT, class CharT, class TraitsT>
IntegerT parseInteger(BasicStringSlice<CharT, TraitsT> str)
{
    if(!str)    // Empty string
        throw std::runtime_error{"cannot parse empty string as number"};

    auto itChar = str.begin();
    IntegerT negate = 1;
    IntegerT limit = std::numeric_limits<IntegerT>::max();
    if(*itChar == '-')
    {
        if(!std::numeric_limits<IntegerT>::is_signed)
            throw std::range_error{"negative value out of range for unsigned integer type"};
        // Negate each digit as it's added.  Don't negate at the end, because
        // the lower limit wouldn't be representable (-128 for signed 8-bit;
        // we can't represent +128).
        negate = -1;
        ++itChar;
        limit = std::numeric_limits<IntegerT>::min();
    }

    IntegerT result{};
    while(itChar != str.end())
    {
        if(TraitsT::lt(*itChar, '0') || TraitsT::lt('9', *itChar))
            throw std::runtime_error{"cannot parse non-digit as number"};

        // Multiply by 10 and then add the digit value, but check for possible
        // overflow.

        // Can we multiply by 10?
        if(result != 0 && limit / result < 10)
            throw std::range_error{"integer value out of range"};
        result *= 10;
        // Compute the next digit
        IntegerT digit = static_cast<IntegerT>(*itChar - '0');
        // Can we add the digit?
        // (Skip the check if result is 0, because for negate=-1, we can't
        // negate 'limit': -min() isn't representable.)
        if(result != 0 && (limit - result) * negate < digit)
            throw std::range_error{"integer value out of range"};
        result += digit * negate;

        ++itChar;
    }

    return result;
}

// Adapt ArraySlice to KACArraySlice for external API - note that this is
// one-way.
template<class T>
KACArraySlice toApi(ArraySlice<T> array) {return {array.data(), array.size(), sizeof(T)};}
// Adapt between StringSlice and KACStringSlice for external API.  Note that
// fromApi() tolerates data==nullptr with nonzero size.
inline KACStringSlice toApi(StringSlice str) {return {str.data(), str.size()};}
inline StringSlice fromApi(KACStringSlice str) {return {str.data, str.data ? str.size : 0};}
// Adapt from KACStringSliceArray to a std::vector<const StringSlice>.  The
// reverse is not directly possible since an array of KACStringSlices would need
// be allocated to populate the KACStringSliceArray.
KAPPS_CORE_EXPORT std::vector<StringSlice> fromApi(KACStringSliceArray strArray);

}}

KAPPS_CORE_STD_HASH_TEMPLATE(kapps::core::ArraySlice<T>, class T);
KAPPS_CORE_STD_HASH_TEMPLATE(kapps::core::BasicStringSlice<CharT KAPPS_CORE_COMMA Traits>, class CharT, class Traits);
