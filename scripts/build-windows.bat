rem Copyright (c) 2019 London Trust Media Incorporated
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
rem It attempts to detect all prerequesites (see README.md).  A qbs profile is
rem created (or updated) by this script for use in the build.

set MODE=release

set BRAND=%1
if ["%BRAND%"]==[""] set BRAND=pia

rem  Detect Qbs
where /Q qbs
if %errorlevel% equ 0 (
  set QBS=qbs
) else if exist "C:\Qt5.12\Tools\QtCreator\bin\qbs.exe" (
  set "QBS=C:\Qt5.12\Tools\QtCreator\bin\qbs.exe"
) else if exist "C:\Qt\Tools\QtCreator\bin\qbs.exe" (
  set "QBS=C:\Qt\Tools\QtCreator\bin\qbs.exe"
) else (
  echo Error: Qbs not found
  goto error
)
echo Found Qbs executable "%QBS%"

rem  Detect Qt
if not defined QTDIR (
  rem Allow C:\Qt as well as C:\Qt5.12, etc.
  for /D %%F in ("C:\Qt*.*") do (
    for /D %%G in ("%%F\?.*") do (
      set "QTDIR=%%G"
    )
  )
)
if not exist "%QTDIR%" (
  echo Error: Unable to find any Qt installation
  goto error
)
for %%G in ("%QTDIR%") do set "QTVER=%%~nxG"
echo Found Qt %QTVER%
for /D %%G in ("%QTDIR%\msvc201?") do set "QT_x86=%%~nxG"
for /D %%G in ("%QTDIR%\msvc201?_64") do set "QT_x64=%%~nxG"
for %%G in (x64,x86) do (
  if [!QT_%%G!] == [] (
    echo Error: Unable to find %%G binaries for Qt %QTVER%
    goto error
  )
)

rem  Detect Visual Studio 2017
for %%G in (Professional,Community,BuildTools) do (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\%%G" (
    set "MSVCROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\%%G"
    if exist "!MSVCROOT!\VC\Auxiliary\Build\vcvarsall.bat" (
      set "MSVCVARS=!MSVCROOT!\VC\Auxiliary\Build\vcvarsall.bat"
    )
    for /D %%H in ("!MSVCROOT!\VC\Tools\MSVC\1?.*") do (
      for %%I in (x64,x86) do (
        if exist "%%H\bin\Host%%I\%%I\cl.exe" set "CL_%%I=%%H\bin\Host%%I\%%I\cl.exe"
      )
    )
  )
)
if ["%MSVCVARS%"]==[""] (
  echo Error: unable to find vcvarsall.bat
  goto error
)
echo Found MSVC environment "%MSVCVARS%"
for %%G in (x64,x86) do (
  if ["!CL_%%G!"]==[""] (
    echo Error: unable to find %%G compiler
    goto error
  )
  echo Found %%G compiler "!CL_%%G!"
)

rem  Detect signtool
if not defined SIGNTOOL (
  where /Q signtool
  if !errorlevel! equ 0 (
    set SIGNTOOL=signtool
  ) else (
    for /D %%G in ("%PROGRAMFILES(X86)%\Windows Kits\10\bin\10.*") do (
      if exist "%%G\x64\signtool.exe" set "SIGNTOOL=%%G\x64\signtool.exe"
    )
  )
)
if ["%SIGNTOOL%"]==[""] (
  echo Warning: no signtool executable found
)
echo Found signtool executable "%SIGNTOOL%"

rem  Detect presence of 7-Zip
if not exist "%PROGRAMFILES%\7-Zip\7z.exe" (
  echo Error: 7-Zip not found ^(install from https://7-zip.org/^)
  goto error
)
echo Found 7-Zip

echo.

rem  Create/overwrite custom Qbs profiles
for %%G in (x64,x86) do (
  "%QBS%" setup-toolchains --type msvc "!CL_%%G!" pia-msvc-%%G
  "%QBS%" setup-qt "%QTDIR%\!QT_%%G!\bin\qmake.exe" pia-qt-%%G
  echo Setting profile 'pia-msvc-%%G' as new base profile for 'pia-qt-%%G'
  "%QBS%" config profiles.pia-qt-%%G.baseProfile pia-msvc-%%G
)

rem  We're currently in scripts/ via the pushd %~dp0 at the top of this file,
rem  so switch to the repository root instead
cd ..
set "ROOT=%cd%"

set OUTDIR=out\%BRAND%
set BUILDDIR=%OUTDIR%\build
set ARTIFACTSDIR=%OUTDIR%\artifacts

if exist %BUILDDIR% (
  rmdir /S /Q %BUILDDIR%
  if !errorlevel! neq 0 (
    echo Error: unable to delete existing build directory
    goto error
  )
)

if exist %ARTIFACTSDIR% (
  rmdir /S /Q %ARTIFACTSDIR%
  if !errorlevel! neq 0 (
    echo Error: unable to delete existing artifacts directory
    goto error
  )
)

mkdir %BUILDDIR%
mkdir %ARTIFACTSDIR%

rem  Fetch a region list to bundle
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://www.privateinternetaccess.com/vpninfo/servers?version=1001&client=x-alpha', 'daemon\res\json\servers.json')"
if %errorlevel% neq 0 (
  echo Error: Unable to fetch region list
  goto error
)
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://www.privateinternetaccess.com/vpninfo/shadowsocks_servers', 'daemon\res\json\shadowsocks.json')"
if %errorlevel% neq 0 (
  echo Error: Unable to fetch shadowsocks region list
  goto error
)

rem  Build all products in the project in debug/release mode as well
rem  as both 32-bit and 64-bit configurations (use an in-source build
rem  directory or GitLab won't let us pick up artifacts)
rem
rem  Note: This also executes the autotest runner for all unit tests
for %%G in (x64,x86) do (
  echo.
  echo Building %%G %MODE% configuration...
  echo.

  rem  Use a local scope for each build
  setlocal

  rem  Change current directory to the Qt bin directory, so all commands and
  rem  libraries are implicitly findable by things like the AutotestRunner
  pushd "%QTDIR%\!QT_%%G!\bin"

  rem  Import MSVC build environment
  call "%MSVCVARS%" %%G >NUL

  rem  Execute Qbs build
  "%QBS%" build --file "%ROOT%\pia_desktop.qbs" --build-directory "%ROOT%\%BUILDDIR%\%%G" profile:pia-qt-%%G config:%MODE% project.brandCode:"%BRAND%"  --all-products

  rem  Return to previous directory
  popd

  rem  End local scope and restore original environment
  endlocal

  rem  Check if anything went wrong
  if !errorlevel! neq 0 (
    echo.
    echo Error: %%G build failed
    goto error
  )

  rem  Copy installers to artifacts directory
  for %%H in (%MODE%) do (
    set /a LOOP_INDEX = 0
    for /f "usebackq tokens=*" %%X IN ("%ROOT%\%BUILDDIR%\%%G\%%H\version\version.txt") do (
      IF /I "!LOOP_INDEX!" EQU "0" set PRODUCT_VERSION=%%X
      IF /I "!LOOP_INDEX!" EQU "1" set PRODUCT_NAME=%%X
      IF /I "!LOOP_INDEX!" EQU "2" set PACKAGE_NAME=%%X
      set /A LOOP_INDEX += 1
    )

    copy /Y %BUILDDIR%\%%G\%%H\installer\*.exe %ARTIFACTSDIR% >NUL

    rem Copy pia-integtest.zip as a build artifact
    pushd %BUILDDIR%\%%G\%%H
    for /R %%I in (%BRAND%-integtest.zip) do copy /Y "%%I" %ROOT%\%ARTIFACTSDIR% >nul
    ren %ROOT%\%ARTIFACTSDIR%\%BRAND%-integtest.zip %BRAND%-integtest-!PACKAGE_NAME!.zip
    popd
  )


  rem Generate debugging artifacts for distributable builds or for `master` builds
  if not defined PIA_BRANCH_BUILD set GEN_DEBUG=1
  if ["%PIA_BRANCH_BUILD%"] == ["master"] set GEN_DEBUG=1
  if defined GEN_DEBUG (
    setlocal
    echo Generating debug distributable
    set PLATFORM=win
    set ARCH=%%G
    call "%ROOT%\scripts\bash.bat" "%ROOT%\scripts\gendebug.sh"
    endlocal
  )
)

rem  Copy .ts files (from x64 release) to output directory
copy /Y %BUILDDIR%\x64\release\translations\translations.zip %ARTIFACTSDIR%
copy /Y %BUILDDIR%\x64\release\translations\en_US.onesky.ts %ARTIFACTSDIR%

echo.
echo Build complete.
echo.

echo Artifacts produced:
for %%G in ("%ARTIFACTSDIR%\*") do echo   %%~nxG (%%~zG bytes)

echo.
echo Done.

:end
popd
endlocal
exit /b %errorlevel%

:error
echo.
echo Build failed with error %errorlevel%!
if %errorlevel% equ 0 set errorlevel=1
goto end
