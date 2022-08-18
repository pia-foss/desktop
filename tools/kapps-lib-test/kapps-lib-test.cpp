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

#include <kapps_core/logger.h>
#include <kapps_net/firewall.h>
#include <iostream>
#include <cassert>
#include <kapps_net/src/firewall.h>
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/coreprocess.h>

std::ostream &operator<<(std::ostream &os, const KACStringSlice &str)
{
    return os.write(str.data, str.size);
}

const char *getLevelName(int level)
{
    switch(level)
    {
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_FATAL:
            return "fatal";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_ERROR:
            return "error";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_WARNING:
            return "warning";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_INFO:
            return "info";
        case KAPPS_CORE_LOG_MESSAGE_LEVEL_DEBUG:
            return "debug";
        default:
            return "??";
    }
}

// Logging sink for the test - logs to stdout
void writeLogMsg(void *pContext, const ::KACLogMessage *pMessage)
{
    assert(pMessage);   // Guaranteed by the logger

    // The actual message can contain line breaks (this often happens when
    // tracing tool output, config dumps, etc.), so a real implementation may
    // want to split the message on line breaks and emit the line prefix for
    // each line.
    std::cout << "[" << pMessage->module << "][" << pMessage->category
        << "][" << getLevelName(pMessage->level) << "][" << pMessage->file
        << "][" << pMessage->line << "] " << pMessage->pMessage << std::endl;
}

void initLogging()
{
    // Enable logging and set up a logging sink (using the C-linkage public API)
    ::KACEnableLogging(true);
    // Use our log sink as the callback.  We don't need any context in this
    // example.
    ::KACLogCallback sinkCallback{};
    sinkCallback.pWriteFn = &writeLogMsg;
    // KACLogInit copies the callback struct, so we can safely let
    // sinkCallback be destroyed
    ::KACLogInit(&sinkCallback);
}

void firewallAboutToApplyCallback()
{
    std::cout << "Firewall invoked about-to-apply callback" << std::endl;
}

void firewallDidApplyCallback()
{
    std::cout << "Firewall invoked did-apply callback" << std::endl;
}

void cppFirewall()
{
    std::cout << "hello world\n";
    kapps::net::FirewallConfig config;
    config.brandInfo.code = "pia";
    config.brandInfo.identifier = "com.privateinternetaccess.vpn";
#if defined(KAPPS_CORE_OS_LINUX)
    config.bypassFile = "bypass/file";
    config.vpnOnlyFile = "vpnOnly/file";
    config.defaultFile = "default/file";
    config.brandInfo.cgroupBase = 1383;
    config.brandInfo.fwmarkBase = 12817;
#endif

    kapps::net::Firewall fw{config};
    fw.applyRules({});
}

void configureFirewall()
{
    // Try configuring the firewall.  This is just a sample, it just produces
    // some logging (the firewall implementation isn't actually in this module
    // yet).
    ::KANFirewallConfig firewallConfig{};
    firewallConfig.pAboutToApplyRules = &firewallAboutToApplyCallback;
    firewallConfig.pDidApplyRules = &firewallDidApplyCallback;

    std::cout << "Invoking firewall config and apply" << std::endl;
    ::KANConfigureFirewall(&firewallConfig);
    ::KANApplyFirewallRules();
    std::cout << "Firewall config completed" << std::endl;
}

void Process_readAllStandardOutputAndErrorLong()
{
#ifdef KAPPS_CORE_OS_POSIX
    std::cout << __FUNCTION__ << std::endl;
#ifdef KAPPS_CORE_OS_LINUX
    // On Linux, -L shows all threads, which makes the output really huge and
    // is good for testing saturation of the pipe buffer.  There's no equivalent
    // on macOS.
    const char *pPsFormat{"-ALf"};
#else
    const char *pPsFormat{"-Af"};
#endif
    kapps::core::Process p{"/bin/ps", {pPsFormat}};
    kapps::core::StringSink out, err;
    p.run(out.readyFunc(), err.readyFunc());
    std::cout << "exit code " << p.exitCode() << std::endl;

    std::cout << "StandardOutput" << std::endl;
    std::string outputData{std::move(out).data()};
    kapps::core::StringSlice output{outputData};
    if(output.size() > 200)
    {
        // Print crudely truncated output since this is very long by design (first
        // and last 100 chars)
        std::cout << output.substr(0, 100) << "..." << std::endl;
        std::cout << "..." << std::endl;
        std::cout << "..." << output.substr(output.size()-100) << std::endl;
    }
    else
        std::cout << output << std::endl;
    std::cout << "StandardError" << std::endl;
    std::cout << err.data() << std::endl;
#endif
}

void Process_readAllStandardOutputAndErrorShort()
{
#ifdef KAPPS_CORE_OS_POSIX
    std::cout << __FUNCTION__ << std::endl;
    kapps::core::Process p{"/bin/bash", {"-c", "echo hello stdout; echo hello stderr >&2"}};
    kapps::core::StringSink out, err;
    p.run(out.readyFunc(), err.readyFunc());
    std::cout << "exit code " << p.exitCode() << std::endl;
    std::cout << "StandardOutput\n" << out.data() << std::endl;
    std::cout << "StandardError\n" << err.data() << std::endl;
#endif
}

void Exec_cmdWithOutput()
{
#ifdef KAPPS_CORE_OS_POSIX
    std::cout << __FUNCTION__ << std::endl;
    std::cout << kapps::core::Exec::cmdWithOutput("/bin/ps", {}) << std::endl;
#endif
}

void Exec_bashWithOutput()
{
#ifdef KAPPS_CORE_OS_POSIX
    std::cout << __FUNCTION__ << std::endl;
    // This command generates both stdout and stderr
    std::cout << kapps::core::Exec::bashWithOutput("echo hello stdout; echo hello stderr >&2", false) << std::endl;
#endif
}

int main()
{
    initLogging();
    configureFirewall();
    //cppFirewall();
    Process_readAllStandardOutputAndErrorLong();
    Exec_cmdWithOutput();
    Exec_bashWithOutput();

    return 0;
}
