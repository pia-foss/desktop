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

#include <common/src/common.h>
#line SOURCE_FILE("win_appscanner.cpp")

#include "win_appscanner.h"
#include "win_objects.h"
#include <kapps_core/src/win/win_linkreader.h>
#include <common/src/win/win_util.h>
#include <common/src/win/win_winrtloader.h>
#include <common/src/builtin/path.h>
#include "brand.h"
#include "../client.h"
#include <QtWin>
#include <QDirIterator>
#include <QMutex>
#include <array>
#include <variant>
#include <cstddef>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <VersionHelpers.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")

namespace
{
    // Path to the WWA host for WWA apps
    const QString wwaHostPath = []() -> QString
    {
        std::array<wchar_t, MAX_PATH> sysPath;
        if(!::GetSystemDirectoryW(sysPath.data(), static_cast<unsigned>(sysPath.size())))
            return {};
        if(FAILED(::PathAppend(sysPath.data(), L"WWAHost.exe")))
            return {};
        return QString::fromWCharArray(sysPath.data());
    }();
    const QString wwaHostDisplayName = QStringLiteral("Windows Web Applications");

    // Get the localized name of a file.  Usually this is a link, but it can
    // actually be any file - localized names are provided by the shell for any
    // type of file (they're in desktop.ini).
    QString getLocalizedName(const QString &filePath)
    {
        std::wstring linkName;

        try
        {
            std::array<wchar_t, MAX_PATH> nameResUnexpPath{};
            int nameResId{0};
            HRESULT nameErr = ::SHGetLocalizedName(qstringWBuf(filePath),
                                                   nameResUnexpPath.data(),
                                                   nameResUnexpPath.size(),
                                                   &nameResId);
            if(FAILED(nameErr))
            {
                // Don't bother tracing anything.  This happens for apps that
                // don't have localized names (very common), the error returned
                // by Shellapi is usually completely bogus, and the worst impact
                // is that we might not localize an app name.
                return {};
            }

            // If the ID is out of range, don't bother with the rest
            if(nameResId < 0)
            {
                qInfo() << "Resource ID for localized name is invalid -"
                    << nameResId << "-" << filePath;
                throw Error{HERE, Error::Code::Unknown};
            }

            std::wstring nameResPath{kapps::core::expandEnvString(nameResUnexpPath.data())};
            if(nameResPath.empty())
            {
                qInfo() << "Couldn't expand path" << filePath;
                throw Error{HERE, Error::Code::Unknown};
            }

            // Try to load that module
            WinResourceModule module{nameResPath.c_str()};
            // Try to load the string identified
            linkName = module.loadString(static_cast<unsigned>(nameResId));
        }
        catch(const Error &ex)
        {
            qInfo() << "Couldn't get localized name for" << filePath << "-" << ex;
        }

        return QString::fromStdWString(linkName);
    }

    // Get the display name for a link.  This returns the localized name if
    // possible, otherwise it is the file's name _without_ the .lnk extension.
    QString getLinkDisplayName(const QFileInfo &link, const QString &linkNativePath)
    {
        QString localizedName = getLocalizedName(linkNativePath);
        // If we didn't find a localized name (or for whatever reason the
        // resource was empty), use the link file's base name
        if(localizedName.isEmpty())
            return link.completeBaseName();
        return localizedName;
    }

    // Get the display name for a non-link file.  This returns the localized
    // name if possible, otherwise it's the complete file name including
    // extension.
    QString getFileDisplayName(const QFileInfo &file, const QString &fileNativePath)
    {
        QString localizedName = getLocalizedName(fileNativePath);
        if(localizedName.isEmpty())
            return file.fileName();
        return localizedName;
    }

    // Get a list of display names for the folders between a base path and an
    // enumerated link.
    QStringList getFolderNames(const QString &basePath, const QString &linkPath)
    {
        // Make a relative path from the base to the directory containing the
        // link.
        std::array<wchar_t, MAX_PATH> linkDir{};
        if(!::PathRelativePathToW(linkDir.data(), qstringWBuf(basePath),
                                  FILE_ATTRIBUTE_DIRECTORY,
                                  qstringWBuf(linkPath), 0))
        {
            qWarning() << "Can't construct relative path from" << basePath
                << "to" << linkPath;
            return {};
        }

        // Remove the file name from the relative path
        if(!::PathRemoveFileSpecW(linkDir.data()))
            return {};

        const wchar_t *pDirStart = linkDir.data();
        // For some reason PathRelativePathToW() gives us a leading '.'
        if(pDirStart[0] == '.')
        {
            if(pDirStart[1] == '\0')
                return {};  // Just ".", no folders
            if(pDirStart[1] == '\\')
                pDirStart += 2; // Skip ".\\"
        }

        Path curDir{basePath};
        QStringList folderNames;
        while(pDirStart && *pDirStart)
        {
            // Find the end of this directory
            const wchar_t *pNextDirStart = ::PathFindNextComponentW(pDirStart);
            if(!pNextDirStart)
                break;
            const wchar_t *pDirEnd = pNextDirStart;
            while(pDirEnd > pDirStart && *pDirEnd == '\\')
                --pDirEnd;

            // Add the next directory to curDir
            curDir = curDir / QStringView{pDirStart, pDirEnd};
            // Get the display name for this folder
            folderNames.push_back(getFileDisplayName(QFileInfo{curDir},
                                                     QDir::toNativeSeparators(curDir)));
            // Advance to the next directory
            pDirStart = pNextDirStart;
        }

        return folderNames;
    }

    class LinkScanner
    {
    public:
        // Entry in the _apps map.
        struct ScannedApp
        {
            // The length of the arguments is stored - for multiple shortcuts to
            // the same app, we prefer the one with the shortest arguments.
            std::size_t _argsLength;
            // Base directory from which this app was enumerated
            QString _basePath;
            QFileInfo _link;
        };

    public:
        LinkScanner();

    private:
        std::wstring canonicalizePath(const wchar_t *pPath);
        void readLink(const QString &baseFolderPath, const QFileInfo &link);

    public:
        void scanDirectory(REFKNOWNFOLDERID folderId);
        // After scanning folders, build the JSON array of apps.
        QJsonArray buildAppsArray() const;

    private:
        kapps::core::WinLinkReader _reader;
        // Canonicalized app installation directory (used to exclude PIA itself)
        std::wstring _piaBasePath;
        // Map of found apps by the target name.  Keys are the _canonicalize_
        // target paths.
        std::unordered_map<std::wstring, ScannedApp> _apps;
    };

    LinkScanner::LinkScanner()
    {
        // Native separators to match canonicalized paths
        QString instDirNative = QDir::toNativeSeparators(Path::InstallationDir);
        _piaBasePath = canonicalizePath(qstringWBuf(instDirNative));
        // Failure tolerated, won't be able to ignore PIA itself
    }

    std::wstring LinkScanner::canonicalizePath(const wchar_t *pPath)
    {
        // Canonicalize the target path
        std::wstring canonicalPath;
        canonicalPath.resize(MAX_PATH);
        // Canonicalize using GetLongPathNameW() - this resolves long/short
        // file names, case, and ../. references.  (The "path canonicalization"
        // Win32 functions actually do not canonicalize paths.)  It's still
        // possible for ambiguity to exist with network drives, UNC paths,
        // subst drives, etc., but this should work pretty well for this
        // purpose.
        DWORD canonLen = ::GetLongPathNameW(pPath, canonicalPath.data(),
                                            static_cast<DWORD>(canonicalPath.size()));
        if(canonLen == 0 || canonLen > canonicalPath.size())
        {
            qWarning() << "Can't canonicalize path" << QStringView{pPath} << "-"
                << canonLen << "-" << SystemError{HERE};
            return {};
        }
        canonicalPath.resize(canonLen);
        // Use native separators to ensure this can match _piaBasePath.
        // GetLongPathNameW() doesn't normalize separators.
        for(auto &c : canonicalPath)
        {
            if(c == '/')
                c = '\\';
        }
        return canonicalPath;
    }

    void LinkScanner::readLink(const QString &baseFolderPath, const QFileInfo &link)
    {
        const auto &linkFilePath{link.filePath().toStdWString()};
        if(!_reader.loadLink(linkFilePath))
            return;

        std::wstring targetPath = _reader.getLinkTarget(linkFilePath);
        if(targetPath.empty())
            return;

        // Canonicalize the target path
        std::wstring canonicalTarget{canonicalizePath(targetPath.c_str())};
        if(canonicalTarget.empty())
            return; // Traced by canonicalizePath()

        // QStringView provides endsWith()
        QStringView canonicalQstr{canonicalTarget.c_str(), static_cast<qsizetype>(canonicalTarget.size())};
        // If it's not an executable, we can't do anything with this, ignore it.
        // If it's PIA itself, ignore it.
        if(!canonicalQstr.endsWith(QStringLiteral(".exe"), Qt::CaseSensitivity::CaseInsensitive) ||
           canonicalQstr.startsWith(_piaBasePath.c_str(), Qt::CaseSensitivity::CaseInsensitive))
        {
            return; // Not an executable, can't do anything with this.
        }

        // Get the argument length - if it fails that's fine, just use the
        // default max value
        std::size_t argsLength = _reader.getArgsLength(linkFilePath);

        // Do we already have an app for this target?
        auto itExistingApp = _apps.find(canonicalTarget);
        if(itExistingApp != _apps.end())
        {
            // Yes, are the args shorter now?
            if(argsLength < itExistingApp->second._argsLength)
            {
                itExistingApp->second._argsLength = argsLength;
                itExistingApp->second._basePath = baseFolderPath;
                itExistingApp->second._link = link;
            }
            // Otherwise, keep the one we already had
        }
        else
        {
            // New target
            _apps.emplace(canonicalTarget, ScannedApp{argsLength, baseFolderPath, link});
        }
    }

    void LinkScanner::scanDirectory(REFKNOWNFOLDERID folderId)
    {
        PWSTR folderPathRaw{nullptr};

        // KF_FLAG_DONT_VERIFY - we have to be prepared for failure opening the
        // directory anyway, don't bother checking an extra time.
        HRESULT pathErr = ::SHGetKnownFolderPath(folderId, KF_FLAG_DONT_VERIFY,
            nullptr, &folderPathRaw);
        QString folderPath = QString::fromWCharArray(folderPathRaw);
        ::CoTaskMemFree(folderPathRaw);
        folderPathRaw = nullptr;

        if(FAILED(pathErr) || folderPath.isEmpty())
        {
            qWarning() << "Unable to get folder path -" << pathErr
                << QStringView{folderPath};
            return;
        }

        QDirIterator dirIter{folderPath, {QStringLiteral("*.lnk")},
                             QDir::Filter::Files|QDir::Filter::System,
                             QDirIterator::IteratorFlag::Subdirectories};
        while(dirIter.hasNext())
        {
            dirIter.next();
            readLink(folderPath, dirIter.fileInfo());
        }
    }

    QJsonArray LinkScanner::buildAppsArray() const
    {
        QJsonArray appsArray;
        for(const auto &app : _apps)
        {
            // SHGetLocalizedName() is very picky about slashes apparently
            // (returns E_INVALIDARG if we give it a path with slashes instead
            // of backslashes).  ::PathRelativePathToW() also fails.
            QString linkPath = QDir::toNativeSeparators(app.second._link.filePath());

            QString displayName{getLinkDisplayName(app.second._link, linkPath)};
            // Windows apps are frequently cluttered with shortcuts to "help",
            // "uninstall", etc. that don't make much sense if they're sorted
            // away from the app they correspond to.  We can't reliably filter
            // these out, but sort apps using folder names to keep them
            // together in the list.
            // In the future, we might display these folder names in some way.
            QStringList folders{getFolderNames(app.second._basePath, linkPath)};
            appsArray.append(SystemApplication{linkPath, std::move(displayName),
                                               std::move(folders)}.toJsonObject());
        }
        return appsArray;
    }

    // App icon provider for Windows.
    class WinAppIconProvider : public QQuickImageProvider
    {
    public:
        WinAppIconProvider()
            : QQuickImageProvider{QQuickImageProvider::Pixmap}
        {
        }

    private:
        QPixmap loadIconFromFile(const std::wstring &path,
                                 const QSize &requestedSize);
        QPixmap loadIconFromModule(const std::wstring &path, int index,
                                   const QSize &requestedSize);
        QPixmap loadShell32Icon(int index, const QSize &requestedSize);
        QPixmap loadDefaultIcon(const QSize &requestedSize);
        QPixmap loadLinkPixmap(const QString &id, const QSize &requestedSize);
        QPixmap loadUwpPixmap(const QString &family, const QSize &requestedSize);
        QPixmap loadWwaPixmap(const QSize &requestedSize);

    public:
        QPixmap requestPixmap(const QString &id, QSize *pSize,
                              const QSize &requestedSize) override;

    private:
        // Mutex protecting the default image data members.
        // QQuickImageProvider says that requestPixmap() could be called from
        // multiple threads, although this hasn't been observed in practice so
        // far.
        mutable QMutex _mutex;
        // The default image is cached in the last size that was requested.
        // This is a "null" QPixmap if no image has been loaded yet or the last
        // load failed.
        QPixmap _defaultImg;
        // The last requested size - meaningful when _defaultImg is valid.
        // Could be {0, 0} if no particular size was requested when we loaded
        // _defaultImg (which is different from _defaultImg.size() in that case)
        QSize _defaultImgSize;
    };

    QPixmap WinAppIconProvider::loadIconFromFile(const std::wstring &path,
                                                 const QSize &requestedSize)
    {
        IconResource icon{path.c_str(), requestedSize};
        return QtWin::fromHICON(icon.getHandle());
    }

    QPixmap WinAppIconProvider::loadIconFromModule(const std::wstring &path,
                                                   int index,
                                                   const QSize &requestedSize)
    {
        // Try to load the module
        WinResourceModule module{path.c_str()};

        // If the index is 0, that's just the first icon, so find the first
        // icon only (don't bother loading the same icon twice if it fails).
        // Index 0 is very common.
        if(index == 0)
            index = -1;

        // Find the icon ID at the specified index and/or the first icon ID
        auto iconIds = module.findIconRes(index, path);

        // If an icon was found for the index given, try that first, but
        // allow failure to fall back to the first icon
        if(!iconIds.second.empty())
        {
            try
            {
                IconResource icon{module.getHandle(),
                                  iconIds.second.getResName(),
                                  requestedSize, path};
                return QtWin::fromHICON(icon.getHandle());
            }
            catch(const Error &ex)
            {
                qWarning() << "Icon resource" << index << "doesn't exist in"
                    << path << "-" << ex;
                // Eat error and try to load the first icon instead
            }
        }

        // Load the first icon as a fallback.  If this fails too, we're
        // done.
        IconResource icon{module.getHandle(), iconIds.first.getResName(),
                          requestedSize, path};
        return QtWin::fromHICON(icon.getHandle());
    }

    QPixmap WinAppIconProvider::loadShell32Icon(int index, const QSize &requestedSize)
    {
        const LPCWSTR shell32Path = L"shell32.dll";
        WinResourceModule shell32{shell32Path};
        auto iconIds = shell32.findIconRes(index, shell32Path);
        IconResource icon{shell32.getHandle(), iconIds.second.getResName(),
                          requestedSize, shell32Path};
        return QtWin::fromHICON(icon.getHandle());
    }

    QPixmap WinAppIconProvider::loadDefaultIcon(const QSize &requestedSize)
    {
        QMutexLocker lock{&_mutex};

        // Return from cache if this is the same size we already loaded
        if(!_defaultImg.isNull() && requestedSize == _defaultImgSize)
            return _defaultImg;

        qInfo() << "Load default icon with size" << requestedSize;

        // Load the default icon from shell32.dll - index 0 is a generic file
        // placeholder
        _defaultImg = loadShell32Icon(0, requestedSize);
        _defaultImgSize = requestedSize;

        return _defaultImg;
    }

    QPixmap WinAppIconProvider::loadLinkPixmap(const QString &id,
                                               const QSize &requestedSize)
    {
        kapps::core::WinLinkReader linkReader;
        const auto &linkFilePath{id.toStdWString()};

        if(!linkReader.loadLink(linkFilePath))
            return {};

        auto location = linkReader.getLinkIconLocation(linkFilePath);
        // If no icon location was given, fall back to the link target
        if(location.first.empty())
            location = {linkReader.getLinkTarget(linkFilePath), 0};

        // Is it an ICO file or a DLL/EXE?
        // (QStringView has a case-insensitive endsWith)
        QStringView filePath{location.first.c_str(),
                             static_cast<qsizetype>(location.first.size())};
        if(filePath.endsWith(QStringView{L".ico"}, Qt::CaseSensitivity::CaseInsensitive))
        {
            return loadIconFromFile(location.first, requestedSize);
        }
        if(!location.first.empty())
        {
            return loadIconFromModule(location.first, location.second,
                                      requestedSize);
        }
        // Otherwise, no icon is specified, return a default icon
        return {};
    }

    QPixmap WinAppIconProvider::loadUwpPixmap(const QString &family,
                                              const QSize &requestedSize)
    {
        auto imgData = getWinRtLoader().loadAppIcon(family,
                                            static_cast<float>(requestedSize.width()),
                                            static_cast<float>(requestedSize.height()));
        QPixmap appImg;
        if(!imgData.empty())
        {
            appImg.loadFromData(imgData.data(),
                                static_cast<uint>(imgData.size()));
        }
        return appImg;
    }

    QPixmap WinAppIconProvider::loadWwaPixmap(const QSize &requestedSize)
    {
        // Index 13 is a 'world' icon
        return loadShell32Icon(13, requestedSize);
    }

    QPixmap WinAppIconProvider::requestPixmap(const QString &id, QSize *pSize,
                                              const QSize &requestedSize)
    {
        // Qt doesn't decode the id after extracting it from the URI
        QString path = QUrl::fromPercentEncoding(id.toUtf8());
        try
        {
            // If it's the WWA host, use the default Windows logo
            if(path == wwaHostPath)
            {
                QPixmap wwaHostImg = loadWwaPixmap(requestedSize);
                if(!wwaHostImg.isNull())
                    return wwaHostImg;
            }
            // If it's a UWP app, load the icon data using WinRT
            else if(path.startsWith(uwpPathPrefix))
            {
                const auto &family = path.mid(uwpPathPrefix.size());
                QPixmap uwpImg = loadUwpPixmap(family, requestedSize);
                if(!uwpImg.isNull())
                    return uwpImg;
            }
            // If it's a shortuct, load an icon from the shortcut metadata or
            // the link target
            else if(path.endsWith(QStringLiteral(".lnk"), Qt::CaseSensitivity::CaseInsensitive))
            {
                QPixmap linkImg = loadLinkPixmap(path, requestedSize);
                if(!linkImg.isNull())
                    return linkImg;
                // Otherwise, continue below to load a default image
            }
            else
            {
                // It's not a link - it should be an executable of some kind.
                // This happens if the user manually browses to an executable.
                // Try to load an icon from this file.
                return loadIconFromModule(path.toStdWString(), 0, requestedSize);
            }
        }
        catch(const Error &ex)
        {
            qWarning() << "Can't get app icon for" << path << "-" << ex;
        }

        // Return the default icon (if it can be loaded)
        try
        {
            return loadDefaultIcon(requestedSize);
        }
        catch(const Error &ex)
        {
            qWarning() << "Can't load default icon for" << path << "-" << ex;
        }

        // Failed, can't return anything
        return {};
    }
}

// Do the scan for scanApplications() on the worker thread.  This can't
// access any members of WinAppScanner, which is why it's static - a pointer
// to the WinAppScanner is provided just to queue the result back to the
// main thread.
void WinAppScanner::scanOnThread(WinAppScanner *pScanner)
{
    QJsonArray nativeApps;

    try
    {
        LinkScanner scanner;

        // Scan programs in the global start menu
        scanner.scanDirectory(FOLDERID_CommonPrograms);
        // Scan programs in this user's start menu
        scanner.scanDirectory(FOLDERID_Programs);
        nativeApps = scanner.buildAppsArray();
    }
    catch(const Error &ex)
    {
        qWarning() << "Unable to scan applications:" << ex;
    }

    auto uwpApps = getWinRtSupport().getUwpApps();

    // Finished scanning the applications, finalize and emit on main thread
    QMetaObject::invokeMethod(pScanner,
        [pScanner, nativeApps = std::move(nativeApps), uwpApps = std::move(uwpApps)]()
        {
            pScanner->completeScan(std::move(nativeApps), std::move(uwpApps));
        }, Qt::ConnectionType::QueuedConnection);
}

// A WinComInit wrapped in a QObject so it can be parented to another QObject
// on _workerThread
class WinComInitQObject : public QObject
{
public:
    using QObject::QObject;
private:
    WinComInit _comInit;
};

WinAppScanner::WinAppScanner()
{
    // Initialize COM on the worker thread.
    _workerThread.invokeOnThread([this]()
    {
        // COM initializer is parented to the worker thread's object owner so
        // it's destroyed before the thread terminates.
        new WinComInitQObject{&_workerThread.objectOwner()};
        getWinRtSupport().initWinRt();
    });
}

void WinAppScanner::scanApplications()
{
    // Kick off the scan on the worker thread.  When it's complete, it'll queue
    // a call back to the main thread to emit applicationScanComplete().
    _workerThread.queueOnThread([this](){scanOnThread(this);});
}

void WinAppScanner::completeScan(QJsonArray nativeApps,
                                 std::vector<EnumeratedUwpApp> uwpApps)
{
    // Build a map of family IDs to display names.  This deduplicates family IDs
    // if they appear more than once, and it allows us to reassociate display
    // names after inspecting the family IDs.
    std::unordered_map<QString, QString> appDisplayNames;

    for(const auto &app : uwpApps)
        appDisplayNames[app.appPackageFamily] = app.displayName;

    // Build the (deduplicated) family ID array for the RPC call
    QJsonArray familyIds;
    for(const auto &app : appDisplayNames)
        familyIds.push_back(app.first);

    // Build the parameters array for the RPC call
    QJsonArray rpcParams;
    rpcParams.push_back(familyIds);

    // Do the RPC call
    g_daemonConnection->call(QStringLiteral("inspectUwpApps"), std::move(rpcParams))
        ->notify(this,
        [this, nativeApps = std::move(nativeApps),
         appDisplayNames = std::move(appDisplayNames)]
        (const Error &error, const QJsonValue &result) mutable
        {
            if(error)
            {
                qWarning() << "Couldn't inspect UWP apps:" << error;
                // Continue anyway, app result arrays are empty
            }

            const auto &resultObj = result.toObject();
            const auto &exeApps = resultObj.value(QStringLiteral("exe")).toArray();
            const auto &wwaApps = resultObj.value(QStringLiteral("wwa")).toArray();

            // Add the executable apps
            for(const auto &exeApp : exeApps)
            {
                const auto &familyId = exeApp.toString();
                auto itDisplayName = appDisplayNames.find(familyId);
                if(!familyId.isEmpty() && itDisplayName != appDisplayNames.end())
                {
                    nativeApps.push_back(SystemApplication{uwpPathPrefix + familyId,
                                                           itDisplayName->second,
                                                           {}}.toJsonObject());
                }
            }

            // Add an entry for WWA apps
            if(!wwaApps.isEmpty())
            {
                QStringList wwaAppNames;
                qInfo() << "WWA apps:";
                for(const auto &app : wwaApps)
                {
                    auto itDisplayName = appDisplayNames.find(app.toString());
                    if(itDisplayName != appDisplayNames.end())
                    {
                        qInfo() << " -" << itDisplayName->second << "->" << itDisplayName->first;
                        wwaAppNames.push_back(itDisplayName->second);
                    }
                }

                SystemApplication wwaAppsEntry{wwaHostPath, wwaHostDisplayName, {}};
                wwaAppsEntry.includedApps(wwaAppNames);
                nativeApps.push_back(wwaAppsEntry.toJsonObject());
            }

            emit applicationScanComplete(nativeApps);
        });
}

std::unique_ptr<QQuickImageProvider> createWinAppIconProvider()
{
    return std::unique_ptr<QQuickImageProvider>{new WinAppIconProvider};
}

QString getWinAppName(const QString &path)
{
    if(path.startsWith(uwpPathPrefix))
        return getWinRtLoader().loadAppDisplayName(path.mid(uwpPathPrefix.size()));

    // As in readLink(), ensure native separators
    QString pathNative{QDir::toNativeSeparators(path)};
    if(pathNative == wwaHostPath)
        return wwaHostDisplayName;
    if(pathNative.endsWith(QStringLiteral(".lnk"), Qt::CaseSensitivity::CaseInsensitive))
        return getLinkDisplayName(QFileInfo{pathNative}, pathNative);
    return getFileDisplayName(QFileInfo{pathNative}, pathNative);
}
