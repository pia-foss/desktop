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

rem Version logged in order to track script revisions since this file has been
rem manually distributed to some users.
set VERSION=2019-07-12b

set PIA_ARG_DNS=

rem No logging unless enabled with --log (by default, redirect to NUL)
set LOG=NUL

:pia_arg_loop
if not "%1"=="" (
  if "%1"=="--dns" (
    set PIA_ARG_DNS=%2
    shift
    shift
  ) else if "%1"=="--log" (
    call :init_log %2
    shift
    shift
  ) else if "%1"=="--" (
    rem Done parsing PIA args
    shift
    goto pia_arg_loop_done
  ) else (
    echo Unknown option %1 >> "%LOG%"
    shift
  )
)
goto pia_arg_loop

:pia_arg_loop_done

rem OpenVPN also passes a number of arguments, but we don't need any of these
rem right now (we use the dev environment variable instead of the tun_dev
rem parameter)
rem tun_dev tun_mtu link_mtu ifconfig_local_ip ifconfig_remote_ip [init|restart]

if "%script_type%"=="up" (
  rem Delete all DNS servers that might already exist.
  rem Customer logs show that it is possible for DNS servers to be left around
  rem on the TAP adapter somehow.  This should be benign normally since they
  rem wouldn't be used while the adapter is "disconnected", but they need to be
  rem removed now that we're reconnecting.
  echo Delete all DNS servers from dev %dev_idx% >> "%LOG%"
  netsh interface ipv4 delete dnsservers %dev_idx% address=all >> "%LOG%"
  echo result: %errorlevel% >> "%LOG%"

  rem Add DNS servers
  set /A DNS_IDX = 1
  :add_dns_split_loop
  for /f "tokens=1* delims=:" %%G IN ("%PIA_ARG_DNS%") DO (
    echo Add dev %dev_idx% DNS !DNS_IDX!: %%G >> "%LOG%"
    netsh interface ipv4 add dnsservers %dev_idx% address=%%G index=!DNS_IDX! validate=no >> "%LOG%"
    echo result: %errorlevel% >> "%LOG%"
    rem If this fails, abort
    if %errorlevel% neq 0 exit /b 1

    set /A DNS_IDX += 1
    set PIA_ARG_DNS=%%H
    if not "%PIA_ARG_DNS%"=="" goto add_dns_split_loop
  )
) else if "%script_type%"=="down" (
  rem Delete DNS servers
  :del_dns_split_loop
  for /f "tokens=1* delims=:" %%G IN ("%PIA_ARG_DNS%") DO (
    echo Delete dev %dev_idx% DNS: %%G >> "%LOG%"
    netsh interface ipv4 delete dnsservers %dev_idx% address=%%G validate=no >> "%LOG%"
    echo result: %errorlevel% >> "%LOG%"
    rem Ignore failure to delete DNS servers

    set PIA_ARG_DNS=%%H
    if not "%PIA_ARG_DNS%"=="" goto del_dns_split_loop
  )
)

rem Otherwise, some other script action, don't care about any other action

echo Completed >> "%LOG%"
rem Exit successfully (%errorlevel% could be nonzero if we were deleting DNS
rem servers, and the last one failed)
exit /b 0

goto :eof

rem Initialize logging to %1
:init_log
  rem When disconnecting, if the log file doesn't exist, don't create it.
  rem Otherwise, turning off debug logging while connected would mean that the
  rem file is recreated on disconnect (since the script parameters were set at
  rem connect time.)
  rem There's a chance this could prevent logging when it's turned on if the
  rem user manually deleted the log; etc., but the "down" logging is much less
  rem important than the "up" logging anyway.
  if "%script_type%"=="down" (
    if not exist "%~1" (
      exit /b 0
    )
  )
  rem Rotate the log if necessary
  call :rotate_log %1
  rem Enable logging to the file specified
  set "LOG=%~1"
  echo: >> "%LOG%"
  echo: >> "%LOG%"
  echo %date% %time% Version %VERSION% >> "%LOG%"
  echo Script type: %script_type% >> "%LOG%"
  exit /b 0

rem Rotate log file if it has exceeded 256KB, pass log file as %1
:rotate_log
  rem Get the size of the file
  set "size=%~z1"
  if "%size%" == "" (set size=0)
  if %size% gtr 262144 (
    pushd "%~dp1"
    ren "%~nx1" "%~nx1.old"
    popd
  )
  exit /b 0
