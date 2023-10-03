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

#include <stdio.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <sys/errno.h>


int main (int argc, char** argv) {
  if(argc != 2) {
    printf ("Usage: ./pia-unquarantine <full path to .app>\n");
    return 1;
  }

  const char *packagePath = argv[1];
  const char* attrName = "com.apple.quarantine";
  struct stat path_stat;
  stat(packagePath, &path_stat);

  if(S_ISDIR(path_stat.st_mode)) {
      int removeAttrResult = removexattr(packagePath, attrName, 0);
      if(removeAttrResult == 0) {
        printf ("Removed attr successfully.\n");
        return 0;
      } else {
        if(errno == ENOATTR) {
          printf ("Attribute not set.\n");
          return 0;
        }
        printf ("Failed to remove attr due to error - ", errno);
    }
  }
  else {
    printf ("Path is not a valid directory\n");
  }

  return 1;
}
