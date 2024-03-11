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
#include <string>
#include <vector>

namespace kapps { namespace core {

// Abstractions for basic filesystem operations across Windows and POSIX.  See
// posix/fs.cpp.
//
// Errors from the underlying system calls are traced by default.  If errors
// might be expected and noisy, pass silent=true to suppress the trace.
//
// TODO: These are not implemented for Windows yet.
namespace fs
{
    // Create a file
    bool KAPPS_CORE_EXPORT touch(const std::string &path, uint16_t mode=0644);
    // Whether a file exists on disk
    bool KAPPS_CORE_EXPORT exists(const std::string &path);
    // Whether a directory exists on disk - returns false if given a file
    bool KAPPS_CORE_EXPORT dirExists(const std::string &path);
    // Return the parent directory of a path - does not query disk
    std::string KAPPS_CORE_EXPORT dirName(const std::string &path);
    // Make a directory
    bool KAPPS_CORE_EXPORT mkDir(const std::string &path, bool silent=false);
    // Make all directories (recursively) for a given path, analogous to mkdir -p in Linux
    bool KAPPS_CORE_EXPORT mkDir_p(const std::string &path, bool silent=false);
    // Copy a file from a src path to a destination path
    bool KAPPS_CORE_EXPORT copyFile(const std::string &srcPath, const std::string &destPath, bool silent=false);
    // Copy the file mode, owner, and group from the srcPath to the destPath.
    bool KAPPS_CORE_EXPORT copyFilePermissions(const std::string &srcPath, const std::string &destPath);
    // Read the target of a filesystem symlink.  Returns the target path, or
    // "" if it couldn't be read.
    std::string KAPPS_CORE_EXPORT readLink(const std::string &path, bool silent=false);
    bool KAPPS_CORE_EXPORT writeString(const std::string &path, const std::string &content, bool silent=false);
    std::string KAPPS_CORE_EXPORT readString(const std::string &path, size_t bytes, bool silent=false);
    // Returns a std::vector<std::string> of the files inside a directory
    std::vector<std::string> KAPPS_CORE_EXPORT listFiles(const std::string &dirName, unsigned filterFlags, bool silent=false);
    bool KAPPS_CORE_EXPORT isSymlink(const std::string &path);
    bool KAPPS_CORE_EXPORT createSymlink(const std::string &from, const std::string &to);
}
}}
