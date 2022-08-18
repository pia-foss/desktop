// Copyright (c) 2022 Private Internet Access, Inc.
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

#include "util.h"
std::string KAPPS_CORE_EXPORT qs::joinVec(const std::vector<std::string> &vec, const std::string &del)
{
  std::string result;
  for(size_t i=0; i < vec.size(); ++i)
  {
      result += vec[i];
      if(i < vec.size() - 1)
          result += del;
  }
  return result;
}

void KAPPS_CORE_EXPORT qs::detail::formatImpl(std::stringstream &s, const char *format)
{
    s << format;
}

bool KAPPS_CORE_EXPORT kapps::core::removeFile(const std::string &path)
{
#ifdef KAPPS_CORE_OS_WINDOWS
    return std::filesystem::remove(std::filesystem::u8path(path));
#else
    return ::unlink(path.c_str()) == 0;
#endif
}
