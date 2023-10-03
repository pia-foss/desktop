rem Copyright (c) 2023 Private Internet Access, Inc.
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
pushd %~dp0

rem This script builds the PIA desktop client for Windows.
rem This runs rake for each configuration (which attempts to detect all
rem prerequisites).

rem  We're currently in scripts/ via the pushd %~dp0 at the top of this file,
rem  so switch to the repository root instead
cd ..
set "ROOT=%cd%"

set "RUBYOPT=-Eutf-8"

rem  Build all products in the project in release mode for both 32-bit and
rem  64-bit configurations

set "BUILD64=0"
set "BUILD32=0"
if "%1"=="all" (
  set "BUILD64=1"
  set "BUILD32=1"
)
if "%1"=="x86_64" (
  set "BUILD64=1"
)
if "%1"=="x86" (
  set "BUILD32=1"
)

if %BUILD64%==1 (
  echo.
  echo Building x86_64...
  echo.

  rem  'rake' refers to rake.cmd on Windows; running it directly causes delayed
  rem  expansion to be disabled.  (Wrapping this in a local scope doesn't help.)
  rem  Call it in a subshell to isolate it.
  call rake clean VARIANT=release ARCHITECTURE=x86_64
  call rake all VARIANT=release ARCHITECTURE=x86_64

  rem  Check if anything went wrong
  if !errorlevel! neq 0 (
    echo.
    echo Error: x86_64 build failed: !errorlevel!
    goto error
  )
)

if %BUILD32%==1 (
  echo.
  echo Building x86...
  echo.

  rem  'rake' refers to rake.cmd on Windows; running it directly causes delayed
  rem  expansion to be disabled.  (Wrapping this in a local scope doesn't help.)
  rem  Call it in a subshell to isolate it.
  call rake clean VARIANT=release ARCHITECTURE=x86
  call rake all VARIANT=release ARCHITECTURE=x86

  rem  Check if anything went wrong
  if !errorlevel! neq 0 (
    echo.
    echo Error: x86 build failed: !errorlevel!
    goto error
  )
)

:end
popd
endlocal
exit /b %errorlevel%

:error
echo.
echo Build failed with error %errorlevel%!
if %errorlevel% equ 0 set errorlevel=1
goto end
