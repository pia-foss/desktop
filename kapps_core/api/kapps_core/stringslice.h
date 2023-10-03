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
#include "core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Many KApps module APIs use string slices to describe string data.  This is a
// weaker constraint than requiring a null-terminated string - a null-terminated
// string can always be adapted to a string slice, but not vice-versa.  Narrow
// string data is always in UTF-8.
//
// Various platforms have different conventions for strings; using a string
// slice as the fundamental API model promotes interoperability without
// incurring expensive copies of potentially large string data.
//
// To be clear, string slice data is _not_ always null-terminated.  The core
// logger in particular uses this to slice parts of file paths, etc., to
// generate context strings for log messages.
typedef struct KACStringSlice
{
    const char *data;   // May be nullptr if size is 0.
    size_t size;
} KACStringSlice;

// An array of non-owning string references can also be represented.
typedef struct KACStringSliceArray
{
    KACStringSlice *data;
    size_t size;
} KACStringSliceArray;

#ifdef __cplusplus
}
#endif
