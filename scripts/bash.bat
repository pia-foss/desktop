rem Copyright (c) 2020 Private Internet Access, Inc.
rem
rem This file is part of the Private Internet Access Desktop Client.
rem
rem The Private Internet Access Desktop Client is free software: you can
rem redistribute it and/or modify it under the terms of the GNU General Public
rem License as published by the Free Software Foundation, either version 3 of
rem the License, or (at your option) any later version.
rem
rem The Private Internet Access Desktop Client is distributed in the hope that
rem it will be useful, but WITHOUT ANY WARRANTY; without even the implied
rem warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem GNU General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License
rem along with the Private Internet Access Desktop Client.  If not, see
rem <https://www.gnu.org/licenses/>.

@echo off
setlocal EnableDelayedExpansion

rem This just locates "bash.exe" and runs it with the arguments given.
rem It preserves the current working directory.

if exist "C:\Program Files\Git\bin\bash.exe" (
  set "BASH=C:\Program Files\Git\bin\bash.exe"
) else (
  where /Q bash.exe
  if %errorlevel% equ 0 (
    set BASH=bash.exe
  ) else (
    echo Unable to find bash.exe
    exit /b 1
  )
)

"%BASH%" %*
