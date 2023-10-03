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

#pragma once
#include <kapps_core/src/winapi.h>
#include <kapps_core/src/util.h>
#include <kapps_net/net.h>
#include <kapps_core/src/win/win_error.h>
#include <string>
#include <set>

namespace kapps { namespace net {

// A WFP app ID that can be used as a key in containers.
class KAPPS_NET_EXPORT AppIdKey : public kapps::core::OStreamInsertable<AppIdKey>
{
public:
    AppIdKey() : _pBlob{} {} // Empty by default
    // Load the app ID for an app.  This must be an executable path - if it
    // could be a shortcut, use WinLinkReader to resolve it, then pass the
    // result to AppIdKey.
    explicit AppIdKey(const std::wstring &appPath)
        : AppIdKey{} {reset(appPath);}
    AppIdKey(AppIdKey &&other) : AppIdKey{} {*this = std::move(other);}
    ~AppIdKey() {clear();}

public:
    AppIdKey &operator=(AppIdKey &&other)
    {
        std::swap(_pBlob, other._pBlob);
        return *this;
    }

    bool operator==(const AppIdKey &other) const;
    bool operator!=(const AppIdKey &other) const {return !(*this == other);}
    bool operator<(const AppIdKey &other) const;

    // Test if AppIdKey is equal to a copy made with copyData().  Note that both
    // "null" and "empty" AppIdKeys could equal the same byte vector.
    bool operator==(const std::vector<char> &value) const;

    explicit operator bool() const {return _pBlob;}
    bool operator!() const {return empty();}

    void swap(AppIdKey &other) {std::swap(_pBlob, other._pBlob);}

    bool empty() const {return !_pBlob;}
    // data() returns a mutable FWP_BYTE_BLOB* since the corresponding member of
    // FWP_CONDITION_VALUE is not const
    FWP_BYTE_BLOB *data() const {return _pBlob;}
    void clear();
    // Load the app ID for an app.  If it can't be loaded, AppIdKey becomes
    // empty.
    // Like the constructor, the path must be an executable path - if it could
    // be a shortcut, use WinLinkReader to resolve it first.
    void reset(const std::wstring &appPath);

    // Copy the data from this app ID to a byte vector.
    std::vector<char> copyData() const;

    // View of the data as a string that can be inserted into a WFP
    // command, etc.  (This strips any trailing null characters.)
    core::WStringSlice printableString() const;

    void trace(std::ostream &os) const;

private:
    FWP_BYTE_BLOB *_pBlob;
};

// Set of AppIdKey values owned by WinAppMonitor / WinAppTracker.  Since the
// AppIdKeys are not owned by the caller, the caller should not hang on to
// these values after modifying WinAppMonitor; instead use
// AppIdKey::copyData() to get an owned copy of the AppIdKey data.
using AppIdSet = std::set<std::shared_ptr<const AppIdKey>, core::PtrValueLess>;

}}
