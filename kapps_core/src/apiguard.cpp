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

#include "apiguard.h"
#include "logger.h"

namespace kapps { namespace core {

namespace detail_
{
    void traceGuardException(const std::exception &ex)
    {
        KAPPS_CORE_ERROR() << "API call resulted in exception" << ex.what();
    }

    void verify(const void *p, const StringSlice name)
    {
        if(!p)
        {
            KAPPS_CORE_ERROR() << "nullptr passed to API for" << name;
            throw std::runtime_error{"nullptr passed to API"};
        }
    }

    void verify(const void *p, size_t count, const StringSlice valueName)
    {
        if(!p && count)
        {
            KAPPS_CORE_ERROR() << "nullptr passed to API for array of"
                << valueName << "with nonzero size" << count;
            throw std::runtime_error{"nullptr passed to API for nonempty array"};
        }
    }
}

}}
