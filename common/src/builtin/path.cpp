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

#include "common.h"
#include "path.h"
#include "version.h"
#include "brand.h"
#include "logging.h"
#include "product.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace Files
{
    const QString wireguardGoBasename{QStringLiteral(BRAND_CODE "-wireguard-go")};
}

Path Path::InstallationDir;
Path Path::BaseDir;
Path Path::ExecutableDir;
Path Path::LibraryDir;
Path Path::ResourceDir;
Path Path::InstallationExecutableDir;
Path Path::DaemonDataDir;
Path Path::DaemonUpdateDir;
Path Path::DaemonSettingsDir;
Path Path::DaemonExecutable;
Path Path::ClientExecutable;
Path Path::DaemonLogFile;
Path Path::ConfigLogFile;
Path Path::UpdownLogFile;
Path Path::DaemonDiagnosticsDir;
Path Path::DaemonLocalSocket;
Path Path::DaemonHelperIpcSocket;
Path Path::ClientCrashReportDir;
Path Path::DaemonCrashReportDir;
Path Path::SupportToolExecutable;
Path Path::OpenVPNWorkingDir;
Path Path::OpenVPNExecutable;
Path Path::OpenVPNConfigFile;
Path Path::OpenVPNUpDownScript;
Path Path::HnsdExecutable;
Path Path::SsLocalExecutable;
Path Path::UnboundExecutable;
Path Path::UnboundConfigFile;
Path Path::UnboundDnsStubConfigFile;
Path Path::WireguardGoExecutable;
Path Path::WireguardInterfaceFile;
Path Path::ModernShadowsocksOverride;
Path Path::ModernRegionOverride;
Path Path::ModernRegionMetaOverride;
Path Path::ModernShadowsocksBundle;
Path Path::ModernRegionBundle;
Path Path::ModernRegionMetaBundle;
#ifdef Q_OS_WIN
Path Path::TapDriverDir;
Path Path::WfpCalloutDriverDir;
Path Path::WireguardServiceExecutable;
Path Path::WireguardConfigFile;
#elif defined(Q_OS_MAC)
Path Path::SplitTunnelKextPath;
Path Path::PcapDir;
#endif
#ifdef Q_OS_LINUX
Path Path::VpnExclusionsFile;
Path Path::VpnOnlyFile;
Path Path::ParentVpnExclusionsFile;
#endif
Path Path::ClientDataDir;
Path Path::ClientSettingsDir;
Path Path::ClientLogFile;
Path Path::CliLogFile;
#ifdef Q_OS_MAC
Path Path::ClientUpdateDir;
Path Path::ClientLaunchAgentPlist;
#endif

#ifdef Q_OS_LINUX
Path Path::ClientAutoStartFile;
#endif // Q_OS_LINUX

Path Path::DebugFile;

#if defined(Q_OS_WIN)
# define PIA_DAEMON_FILENAME BRAND_CODE "-service.exe"
# define PIA_CLIENT_FILENAME BRAND_CODE "-client.exe"
#elif defined(Q_OS_MACOS)
# define PIA_DAEMON_FILENAME BRAND_CODE "-daemon"
# define PIA_CLIENT_FILENAME PIA_PRODUCT_NAME
#elif defined(Q_OS_LINUX)
# define PIA_DAEMON_FILENAME BRAND_CODE "-daemon"
# define PIA_CLIENT_FILENAME BRAND_CODE "-client"
#endif

#ifdef Q_OS_WIN
#include <shlobj_core.h>
#pragma comment(lib, "shell32.lib")
Path Path::getProgramsFolder()
{
    wchar_t path[MAX_PATH];
    if (S_OK == SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, path))
        return QDir::fromNativeSeparators(QString::fromWCharArray(path));
    return QString();
}
static inline bool isPathSeparator(QChar ch) { return ch == '/' || ch == '\\'; }
#else
#define isPathSeparator(ch) ((ch) == '/')
#endif

Path::Path()
{

}

Path::Path(const QString& path)
    : _path(path)
{

}

static Path getBaseDir()
{
    Path appPath = QCoreApplication::applicationDirPath();
#if defined(UNIT_TEST)
    // For unit tests, use install-root - unit tests are outside this directory,
    // but we need this to load OpenSSL dynamically.
    appPath = appPath / "../install-root";
    #if defined(Q_OS_MACOS)
        appPath = appPath / BRAND_NAME ".app";
    #endif
#elif defined(Q_OS_MACOS)
    appPath = appPath / "../..";
#elif defined(Q_OS_LINUX)
    appPath = appPath / "../";
#endif
    return appPath;
}

void Path::initializePreApp()
{
    // QStandardPaths depends on these; they can be set before the
    // QCoreApplication is created.
#if defined(Q_OS_WIN)
    QCoreApplication::setApplicationName(QStringLiteral(PIA_PRODUCT_NAME));
#elif defined(Q_OS_MACOS)
    QCoreApplication::setApplicationName(QStringLiteral(BRAND_IDENTIFIER));
#elif defined(Q_OS_LINUX)
    QCoreApplication::setApplicationName(QStringLiteral(BRAND_LINUX_APP_NAME));
#endif
    QCoreApplication::setApplicationVersion(QString::fromStdString(Version::semanticVersion()));

    // Only ClientSettingsDir is needed before QCoreApplication right now, but
    // other paths can be moved here as needed if they do not depend on
    // QCoreApplication.
    ClientSettingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

void Path::initializePostApp()
{
#if defined(Q_OS_WIN)
    InstallationDir = getProgramsFolder() / PIA_PRODUCT_NAME;
    BaseDir = getBaseDir();
    ExecutableDir = BaseDir;
    LibraryDir = BaseDir;
    ResourceDir = BaseDir;
    InstallationExecutableDir = InstallationDir;
    DaemonSettingsDir = DaemonDataDir = InstallationDir / "data";
    DaemonExecutable = ExecutableDir / BRAND_CODE "-service.exe";
    ClientExecutable = ExecutableDir / BRAND_CODE "-client.exe";
#elif defined(Q_OS_MACOS)
    InstallationDir = QStringLiteral("/Applications/" PIA_PRODUCT_NAME ".app");
    BaseDir = getBaseDir();
    ExecutableDir = BaseDir / "Contents/MacOS";
    // Bare .dylibs go into Contents/Frameworks.  Although they are not
    // actually frameworks, this seems to be the norm for bare .dylibs, and in
    // particular Qt searches this directory specifically for libssl and
    // libcrypto.
    LibraryDir = BaseDir / "Contents/Frameworks";
    ResourceDir = BaseDir / "Contents/Resources";
    InstallationExecutableDir = InstallationDir / "Contents/MacOS";
    DaemonDataDir = QStringLiteral("/Library/Application Support/" BRAND_IDENTIFIER);
    DaemonSettingsDir = QStringLiteral("/Library/Preferences/" BRAND_IDENTIFIER);
    DaemonExecutable = ExecutableDir / BRAND_CODE "-daemon";
    ClientExecutable = ExecutableDir / PIA_PRODUCT_NAME;
#elif defined(Q_OS_LINUX)
    InstallationDir = QStringLiteral("/opt/" BRAND_CODE "vpn");
    BaseDir = getBaseDir();
    ExecutableDir = BaseDir / "bin";
    LibraryDir = BaseDir / "lib";
    ResourceDir = InstallationDir / "share";
    InstallationExecutableDir = InstallationDir / "bin";
    DaemonDataDir = InstallationDir / "var";
    DaemonSettingsDir = InstallationDir / "etc";
    DaemonExecutable = ExecutableDir / BRAND_CODE "-daemon";
    ClientExecutable = ExecutableDir / BRAND_CODE "-client";
#endif
    DaemonUpdateDir = DaemonDataDir / "update";

#if defined(UNIT_TEST)
    auto unitTestLibDir = qEnvironmentVariable("UNIT_TEST_LIB");
    if(!unitTestLibDir.isEmpty())
        LibraryDir = unitTestLibDir;
#endif

    ClientDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ClientLogFile = ClientDataDir / "client.log";
    CliLogFile = ClientDataDir / "cli.log";
#ifdef Q_OS_MAC
    ClientUpdateDir = ClientDataDir / "update";
    ClientLaunchAgentPlist = Path{QStandardPaths::writableLocation(QStandardPaths::HomeLocation)} / "Library/LaunchAgents/" BRAND_IDENTIFIER ".client.plist";
#endif

#ifdef Q_OS_LINUX
    ClientAutoStartFile = Path { QStandardPaths::writableLocation(QStandardPaths::HomeLocation) } / ".config" / "autostart" / BRAND_CODE "-client.desktop";
#endif // Q_OS_LINUX
    ClientCrashReportDir = ClientDataDir / "crashes";
    DaemonCrashReportDir = DaemonDataDir / "crashes";

    OpenVPNWorkingDir = ExecutableDir;

#ifdef Q_OS_WIN
    OpenVPNExecutable = OpenVPNWorkingDir / BRAND_CODE "-openvpn.exe";
    SupportToolExecutable = ExecutableDir / BRAND_CODE "-support-tool.exe";
    HnsdExecutable = ExecutableDir / BRAND_CODE "-hnsd.exe";
    SsLocalExecutable = ExecutableDir / BRAND_CODE "-ss-local.exe";
    UnboundExecutable = ExecutableDir / BRAND_CODE "-unbound.exe";
    DaemonLocalSocket = QStringLiteral("\\\\.\\pipe\\" BRAND_WINDOWS_SERVICE_NAME);
    DaemonHelperIpcSocket = QStringLiteral("\\\\.\\pipe\\" BRAND_WINDOWS_SERVICE_NAME "HelperIpc");
#else
    OpenVPNExecutable = OpenVPNWorkingDir / BRAND_CODE "-openvpn";
    HnsdExecutable = ExecutableDir / BRAND_CODE "-hnsd";
    SsLocalExecutable = ExecutableDir / BRAND_CODE "-ss-local";
    UnboundExecutable = ExecutableDir / BRAND_CODE "-unbound";
    WireguardGoExecutable = ExecutableDir / Files::wireguardGoBasename;
#ifdef Q_OS_LINUX
    SupportToolExecutable = ExecutableDir / "support-tool-launcher";
#else
    SupportToolExecutable = ExecutableDir / BRAND_CODE "-support-tool";
#endif

    DaemonLocalSocket = DaemonDataDir / "daemon.sock";
    DaemonHelperIpcSocket = DaemonDataDir / "helper_ipc.sock";
#endif

    ModernShadowsocksOverride = DaemonSettingsDir / "modern_shadowsocks_override.json";
    ModernRegionOverride = DaemonSettingsDir / "modern_regions_override.json";
    ModernRegionMetaOverride = DaemonSettingsDir / "modern_region_meta_override.json";
    ModernShadowsocksBundle = ResourceDir / "modern_shadowsocks.json";
    ModernRegionBundle = ResourceDir / "modern_servers.json";
    ModernRegionMetaBundle = ResourceDir / "modern_region_meta.json";

#ifdef Q_OS_MAC
    // launch support tool from the bundle
    SupportToolExecutable = ExecutableDir / "../Resources/" BRAND_CODE "-support-tool.app/Contents/MacOS/" BRAND_CODE "-support-tool";
#endif

    UnboundConfigFile = DaemonDataDir / "unbound.conf";
    UnboundDnsStubConfigFile = DaemonDataDir / "unbound_stub.conf";
    WireguardInterfaceFile = DaemonDataDir / "wg" BRAND_CODE "0-tun";

    DaemonLogFile = DaemonDataDir / "daemon.log";
    ConfigLogFile = DaemonDataDir / "config.log";
    UpdownLogFile = DaemonDataDir / "updown.log";
    DaemonDiagnosticsDir = DaemonDataDir / "diagnostics";
    DebugFile = DaemonDataDir / "debug.txt";
    OpenVPNConfigFile = DaemonDataDir / "pia.ovpn";

#if defined(Q_OS_MACOS)
    OpenVPNUpDownScript = ExecutableDir / BRAND_CODE "-openvpn-helper";
#elif defined(Q_OS_LINUX)
    OpenVPNUpDownScript = ExecutableDir / "openvpn-updown.sh";
#elif defined(Q_OS_WIN)
    OpenVPNUpDownScript = ExecutableDir / "openvpn_updown.bat";
#endif

#ifdef Q_OS_WIN
    TapDriverDir = BaseDir / "tap";
    WfpCalloutDriverDir = BaseDir / "wfp_callout";
    WireguardServiceExecutable = BaseDir / BRAND_CODE "-wgservice.exe";
    WireguardConfigFile = DaemonDataDir / "wg" BRAND_CODE "0.conf";
#elif defined(Q_OS_MAC)
    SplitTunnelKextPath = ExecutableDir / "../Resources/PiaKext.kext";
    PcapDir = DaemonDataDir + "/pia_pcap";
#endif
#ifdef Q_OS_LINUX
    // Currently only tested on Ubuntu
    VpnExclusionsFile = Path { "/sys/fs/cgroup/net_cls/" BRAND_CODE "vpnexclusions/cgroup.procs" };
    VpnOnlyFile = Path { "/sys/fs/cgroup/net_cls/" BRAND_CODE "vpnonly/cgroup.procs" };
    ParentVpnExclusionsFile = Path { "/sys/fs/cgroup/net_cls/cgroup.procs" };
#endif

    ClientExecutable = QCoreApplication::applicationFilePath();
}

Path Path::operator/(const QString& child) const &
{
    return Path(_path) / child;
}

Path& Path::operator/(const QString& child) &&
{
    return appendPath(child.begin(), child.end());
}

Path Path::operator+(const QString& suffix) const &
{
    return Path(_path + suffix);
}

Path& Path::operator+(const QString& suffix) &&
{
    _path += suffix;
    return *this;
}

Path Path::operator/(const QStringView& child) const &
{
    return Path(_path) / child;
}

Path& Path::operator/(const QStringView& child) &&
{
    return appendPath(child.begin(), child.end());
}

Path Path::operator /(const QLatin1String& child) const &
{
    return Path(_path) / child;
}

Path& Path::operator/(const QLatin1String& child) &&
{
    return appendPath(child.begin(), child.end());
}

Path Path::operator+(const QLatin1String& suffix) const &
{
    return Path(_path + suffix);
}

Path& Path::operator+(const QLatin1String& suffix) &&
{
    _path += suffix;
    return *this;
}

Path Path::operator /(const char* child) const &
{
    return Path(_path) / QLatin1String(child);
}

Path& Path::operator/(const char* child) &&
{
    QLatin1String str(child);
    return appendPath(str.begin(), str.end());
}

Path Path::operator+(const char* suffix) const &
{
    return Path(_path + QLatin1String(suffix));
}

Path& Path::operator+(const char* suffix) &&
{
    _path += QLatin1String(suffix);
    return *this;
}

Path Path::parent() const
{
    return Path(*this).up();
}

Path &Path::mkpath()
{
    const Path &self = *this;
    self.mkpath();  // Implement with const overload
    return *this;
}

const Path& Path::mkpath() const
{
    if (!QDir(_path).mkpath("."))
        qCritical() << "Unable to create path" << _path;
    return *this;
}

Path &Path::mkparent()
{
    (*this / "..").mkpath();
    return *this;
}

Path& Path::touch()
{
    (*this / "..").mkpath();
    if (!QFile(_path).open(QFile::WriteOnly))
        qCritical() << "Failed to create file" << _path;
    return *this;
}

void Path::cleanDirFiles(int keepCount)
{
    QDir dirObj{*this};

    // If the directory doesn't exist, isn't readable, etc., entryList() returns
    // an empty list and we do nothing.
    const auto &fileList = dirObj.entryList(QDir::Filter::Files, QDir::SortFlag::Time);

    // If there are fewer than keepCount files, i>fileList.count() initially and
    // we do nothing
    for(int i=keepCount; i<fileList.count(); ++i)
    {
        const auto &cleanFilePath = *this / fileList[i];
        QFile cleanFile{cleanFilePath};
        if(!cleanFile.remove())
        {
            qWarning() << "Unable to clean" << cleanFilePath << "- error:"
                << cleanFile.error() << cleanFile.errorString();
        }
    }
}

template<typename Iterator>
Path& Path::appendPath(Iterator begin, Iterator end)
{
    if (begin == end)
        return *this;
    Iterator p = begin;
    forever
    {
        // Skip past any extra path separators
        while (isPathSeparator(*p))
            if (++p == end)
                return *this;

        Iterator segment = p;
        // Filter out . and .. segments
        if (*p == '.')
        {
            if (++p == end)
                return *this;
            if (*p == '.')
            {
                if (++p == end)
                    return up();
                else if (isPathSeparator(*p))
                {
                    up();
                    if (++p == end)
                        return *this;
                    else
                        continue;
                }
            }
        }
        while (!isPathSeparator(*p))
        {
            if (++p == end)
                return appendSegment(segment, p);
        }
        appendSegment(segment, p);
        if (++p == end)
            return *this;
    }
}

Path& Path::appendSegment(const QChar* first, const QChar* last)
{
    if (!_path.isEmpty() && !isPathSeparator(_path[_path.length() - 1]))
        _path += '/';
    _path.append(first, (int)(last - first));
    return *this;
}

Path& Path::appendSegment(const char* first, const char* last)
{
    if (!_path.isEmpty() && !isPathSeparator(_path[_path.length() - 1]))
        _path += '/';
    _path.append(QLatin1String(first, last));
    return *this;
}

// Chop off the last segment of this path; does not modify the path
// if it's already representing a top-level path
Path& Path::up()
{
    QString::const_reverse_iterator begin = _path.crbegin(), end = _path.crend();
    QString::const_reverse_iterator p = begin;
    if (p == end)
        return *this;
    // Chop off any trailing slashes
    while (isPathSeparator(*p))
    {
        if (++p == end)
            return *this;
    }
    // Chop off the last segment
    do
    {
        if (++p == end)
            return *this;
    } while (!isPathSeparator(*p));
    // Chop off trailing slashes
    do
    {
        if (++p == end)
        {
            // Leave one slash, or the string would become empty
            _path.chop((int)(p - begin - 1));
            return *this;
        }
    } while (isPathSeparator(*p));
    _path.chop((int)(p - begin));
    return *this;
}

std::ostream &operator<<(std::ostream &os, const Path &path)
{
    const QString &pathStr{path};
    return os << pathStr;
}
