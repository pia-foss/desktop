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

#include "../fs.h"
#include <dirent.h>
#include "../logger.h"
#include "../util.h"
#include <unistd.h>
#include <dirent.h> // for readdir
#include <libgen.h> // for dirname
#include <sys/stat.h>
#include <fcntl.h>
#include "posix_objects.h"
#include <sys/stat.h>

namespace kapps { namespace core {

namespace fs
{
    bool exists(const std::string &path)
    {
        struct stat buffer{};
        return stat(path.c_str(), &buffer) == 0;
    }

    bool dirExists(const std::string &path)
    {
        struct stat buffer{};
        if(stat(path.c_str(), &buffer) != 0)
            return false;

        return S_ISDIR(buffer.st_mode);
    }

    std::string dirName(const std::string &path)
    {
        // + 1 for null terminator
        std::vector<char> pathCopy(path.size() + 1);
        // Need a copy as dirname() can modify the arg
        path.copy(pathCopy.data(), pathCopy.size());
        return ::dirname(pathCopy.data());
    }

    bool mkDir(const std::string &path, bool silent)
    {
        int ret = ::mkdir(path.c_str(), 0755);
        if(ret && !silent)
        {
            KAPPS_CORE_WARNING() << "::mkdir failed on" << path << "-"
                << ErrnoTracer{};
        }

        return ret == 0;
    }

    bool mkDir_p(const std::string &path, bool silent) {
        if(path.empty() || dirExists(path)) return true;

        // Find the last '/' and recursively ensure parent directory exists
        size_t pos = path.find_last_of('/');
        if(pos != std::string::npos)
        {
            std::string parentPath = path.substr(0, pos);
            if(!mkDir_p(parentPath, silent))
                return false;
        }

        return mkDir(path, silent);
    }

    bool copyFile(const std::string &srcPath, const std::string &destPath, bool silent)
    {
        std::ifstream source{srcPath, std::ios::binary};
        std::ofstream dest{destPath, std::ios::binary};

        if(!source.is_open()) {
            if(!silent)
                KAPPS_CORE_WARNING() << "Error opening source file: " << srcPath << ErrnoTracer{};
            return false;
        }

        if(!dest.is_open()) {
            if(!silent)
                KAPPS_CORE_WARNING() << "Error opening destination file: " << destPath << ErrnoTracer{};
            return false;
        }

        // Use the rdbuf() method to read the contents
        dest << source.rdbuf();

        return true;
    }

    bool copyFilePermissions(const std::string &srcPath, const std::string &destPath)
    {
        // Get metadata of srcFile
        struct stat srcInfo{};
        if(::stat(srcPath.c_str(), &srcInfo))
        {
            // Failing to get metadata we cannot set permissions, trace it and end.
            KAPPS_CORE_WARNING() << "stat()" << ErrnoTracer{};
            return false;
        }

        // We can't just get the result of st_mode as that also includes info
        // about the file type (i.e symlink, regular file, etc)
        mode_t permissionsToCopy = static_cast<mode_t>(srcInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));

        // Set destFile perms to that of srcFile
        // We'd like to imitate the 'cp' command behaviour
        // which also preserves file modes.
        if(::chmod(destPath.c_str(), permissionsToCopy))
        {
            // Failure to set the file mode means we failed
            KAPPS_CORE_WARNING() << "chmod()" << ErrnoTracer{};
            return false;
        }
        // Set destFile owner and group to that of srcFile.
        if(::chown(destPath.c_str(), srcInfo.st_uid, srcInfo.st_gid))
        {
            // Failure to set the owner means we failed
            KAPPS_CORE_WARNING() << "chown()" << ErrnoTracer{};
            return false;
        }
        return true;
    }

    bool writeString(const std::string &path, const std::string &content, bool silent)
    {
        PosixFd fd{::open(path.c_str(), O_WRONLY)};
        if(!fd)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::open failed on" << path << "-"
                    << ErrnoTracer{};
            }
            return false;
        }

        // Try writing content to file
        int ret{-1};
        NO_EINTR(ret = ::write(fd.get(), content.c_str(), content.size()));
        if(ret == -1)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::write failed on" << path << "-"
                    << ErrnoTracer{};
            }
            return false;
        }

        // Success
        return true;
    }

    std::string readString(const std::string &path, size_t bytes, bool silent)
    {
        PosixFd fd{::open(path.c_str(), O_RDONLY)};
        if(!fd)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::open failed on" << path << "-"
                    << ErrnoTracer{};
            }
            return {};
        }

        std::vector<char> content(bytes);

        // Try reading all content from file
        int ret{-1};
        NO_EINTR(ret = ::read(fd.get(), content.data(), content.size()));
        if(ret == -1)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::read failed on" << path << "-"
                    << ErrnoTracer{};
            }
            return {};
        }

        // Success
        return std::string{content.begin(), content.end()};
    }

    struct AutoCloseDir
    {
        DIR *_pDir{nullptr};
        operator DIR *() const { return _pDir; }
        explicit operator bool() { return _pDir != nullptr; }
        ~AutoCloseDir() { if (_pDir) ::closedir(_pDir); }
    };

    // For filter flags, see DT_REG or DT_DIR found in dirent.h
    std::vector<std::string> listFiles(const std::string &dirName, unsigned filterFlags, bool silent)
    {
        AutoCloseDir dir{::opendir(dirName.c_str())};
        if(!dir)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::opendir failed on" << dirName << "-"
                    << ErrnoTracer{};
            }
            return {};
        }

        std::vector<std::string> files;
        files.reserve(500);

        // Must clear errno before calling readdir(3) so we can distinguish
        // end-of-dir from error
        errno = 0;
        while(dirent *entry = ::readdir(dir))
        {
            // No filter flags set or filter flags match, add the file
            if(!filterFlags || (entry->d_type & filterFlags))
                files.emplace_back(entry->d_name);
        }

        // Error
        if(errno != 0 && !silent)
        {
            KAPPS_CORE_WARNING() << "::readdir failed on" << dirName << "-"
                << ErrnoTracer{};
        }

        return files;
    }

    std::string readLink(const std::string &linkName, bool silent)
    {
        std::string buf;
        buf.resize(PATH_MAX);
        auto realSize = ::readlink(linkName.c_str(), &buf[0], buf.size());
        if(realSize == -1)
        {
            if(!silent)
            {
                KAPPS_CORE_WARNING() << "::readlink failed on" << linkName
                    << "-" << ErrnoTracer{};
            }
            return {};
        }
        buf.resize(realSize);
        return buf;
    }
}
}}
