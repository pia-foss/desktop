// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("win_objects.cpp")

#include "win_objects.h"
#include "win/win_util.h"

#include <QPainter>
#include <QPixmap>
#include <QtWin>

#pragma comment(lib, "User32.lib")

WinPopupMenu::WinPopupMenu()
{
}

WinPopupMenu::~WinPopupMenu()
{
}

HMENU WinPopupMenu::createMenu(const NativeMenuItem::List& items)
{
    HMENU menu = CreatePopupMenu();
    MENUINFO info {};
    info.cbSize = sizeof(info);
    info.fMask = MIM_STYLE;
    info.dwStyle = MNS_CHECKORBMP;
    CHECK_IF_FALSE(SetMenuInfo(menu, &info));
    for (auto& item : items)
    {
        if (item->separator())
        {
            CHECK_IF_FALSE(AppendMenuW(menu, MF_SEPARATOR, 0, nullptr));
            continue;
        }
        UINT id = ++_lastId;
        UINT flags = MF_STRING;
        flags |= item->enabled() ? (MF_ENABLED) : (MF_DISABLED | MF_GRAYED);
        CHECK_IF_FALSE(AppendMenuW(menu, flags, id, qUtf16Printable(item->text())));
        _menuCodes.insert(id, item->code());
        if (item->icon().isEmpty() && item->children().isEmpty() && !item->checked())
            continue;
        MENUITEMINFOW info {};
        info.cbSize = sizeof(info);
        if (item->checked() == true)
        {
            info.fMask |= MIIM_STATE;
            info.fState = MFS_CHECKED | (item->enabled() ? (MFS_ENABLED) : (MFS_DISABLED | MFS_GRAYED));
        }
        if (!item->icon().isEmpty())
        {
            auto it = _icons.find(item->icon());
            if (it != _icons.end())
                info.hbmpItem = it->get();
            else
            {
                QString file = item->icon();
                if (file.startsWith("qrc:/")) file.remove(0, 3);
                auto pixmap = QPixmap(file);
                if (!pixmap.isNull())
                {
                    QPixmap icon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
                    icon.fill(Qt::transparent);
                    {
                        double scaleX = static_cast<double>(icon.width()) / static_cast<double>(pixmap.width());
                        double scaleY = static_cast<double>(icon.height()) / static_cast<double>(pixmap.height());
                        double scale = scaleX < scaleY ? scaleX : scaleY;
                        QPainter painter(&icon);
                        painter.setRenderHint(QPainter::SmoothPixmapTransform);
                        painter.drawPixmap(QRectF { (icon.width() - scale * pixmap.width()) / 2.0, (icon.height() - scale * pixmap.height()) / 2.0, scale * pixmap.width(), scale * pixmap.height() }, pixmap, pixmap.rect());
                    }
                    info.hbmpItem = QtWin::toHBITMAP(icon, QtWin::HBitmapPremultipliedAlpha);
                    if (info.hbmpItem)
                        _icons.insert(item->icon(), { info.hbmpItem, DeleteObject });
                }
            }
            if (info.hbmpItem)
                info.fMask |= MIIM_BITMAP;
        }
        if (!item->children().isEmpty())
        {
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = createMenu(item->children());
        }
        if (info.fMask)
        {
            CHECK_IF_FALSE(SetMenuItemInfoW(menu, id, FALSE, &info));
        }
    }
    return menu;
}

void WinPopupMenu::setItems(const NativeMenuItem::List& items)
{
    _menuCodes.clear();
    _lastId = 0;
    _menu.reset(createMenu(items), DestroyMenu);
}

QString WinPopupMenu::showMenu(HWND parentWnd, const QPoint &pos)
{
    // Grab light (copy-on-write) copies of the menu resources
    auto menu = _menu;
    auto menuCodes = _menuCodes;

    // Per doc, we have to set this window to the foreground window before
    // calling TrackPopupMenu().
    ::SetForegroundWindow(parentWnd);

    // Per doc, we have to get the menu aligment manually
    int alignFlag = ::GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;

    // TrackPopupMenu() returns BOOL, but this is really an int, and with
    // TPM_RETURNCMD it's really the menu identifier
    auto result = static_cast<UINT_PTR>(
                ::TrackPopupMenu(menu.get(),
                                 alignFlag|TPM_BOTTOMALIGN|TPM_NONOTIFY|TPM_RETURNCMD|TPM_RIGHTBUTTON,
                                 pos.x(), pos.y(), 0, parentWnd, nullptr));

    return menuCodes.value(result);
}

IconResource::IconResource(WORD resId, Size size)
    : _icon{nullptr}
{
    // The doc suggests to use ::LoadIconMetric() to load these icons, but that
    // is in comctl32 6.0, which requires a manifest entry, etc.  Go directly to
    // ::LoadImage(), which is the general full-blown image-loading API, and get
    // the system metric size manually.
    int cx, cy;
    if(size == Size::Small)
    {
        cx = ::GetSystemMetrics(SM_CXSMICON);
        cy = ::GetSystemMetrics(SM_CYSMICON);
    }
    else
    {
        cx = ::GetSystemMetrics(SM_CXICON);
        cy = ::GetSystemMetrics(SM_CYICON);
    }
    _icon = reinterpret_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr),
                                    MAKEINTRESOURCE(resId), IMAGE_ICON, cx, cy,
                                    LR_DEFAULTCOLOR));
    if(!_icon)
    {
        CHECK_THROW("Loading Windows icon resource");
    }
}

IconResource::IconResource(HMODULE module, LPCWSTR pName,
                           const QSize &size, const QStringView &traceModulePath)
    : _icon{nullptr}
{
    Q_ASSERT(module);  // Ensured by caller

    _icon = reinterpret_cast<HICON>(::LoadImageW(module, pName,
                                                 IMAGE_ICON, size.width(),
                                                 size.height(), 0));
    if(!_icon)
    {
        qWarning() << "Can't load icon from module"
            << traceModulePath;
        throw SystemError{HERE};
    }
}

IconResource::IconResource(LPCWSTR pPath, const QSize &size)
    : _icon{nullptr}
{
    _icon = reinterpret_cast<HICON>(::LoadImageW(nullptr, pPath, IMAGE_ICON,
                                                 size.width(), size.height(),
                                                 LR_LOADFROMFILE));
    if(!_icon)
    {
        qWarning() << "Can't load icon file" << QStringView{pPath};
        throw SystemError{HERE};
    }
}

IconResource::~IconResource()
{
    ::DestroyIcon(_icon);
}

bool WinResId::empty() const
{
    return (std::holds_alternative<std::wstring>(_id) &&
            std::get<std::wstring>(_id).empty()) ||
           _id.valueless_by_exception();
}

LPCWSTR WinResId::getResName() const
{
    if(std::holds_alternative<std::wstring>(_id))
        return std::get<std::wstring>(_id).c_str();
    else if(std::holds_alternative<WORD>(_id))
        return MAKEINTRESOURCE(std::get<WORD>(_id));
    return MAKEINTRESOURCE(0);
}

void WinResId::setResName(LPCWSTR pName)
{
    if(IS_INTRESOURCE(pName))
    {
        // It's an integer, store the integer ID directly.
        _id = reinterpret_cast<WORD>(pName);
    }
    else
    {
        Q_ASSERT(pName);    // Postcondition of false IS_INTRESOURCE()
        // It's a string (a name or stringified integer).  It's not clear
        // how pName was allocated during EnumResourceNamesExW(), so
        // copy the string to be on the safe side.
        _id = std::wstring{pName};
    }
}

BOOL CALLBACK WinResourceModule::enumResourceNameProc(HMODULE, LPCWSTR,
                                                      LPWSTR pName,
                                                      LONG_PTR lParam)
{
    auto pData = reinterpret_cast<FindIconResData*>(lParam);
    Q_ASSERT(pData);   // Ensured by findIconRes()

    // If we haven't found the first icon yet, store that.
    if(pData->icons.first.empty())
        pData->icons.first.setResName(pName);

    // If this is the desired index, store that.
    if(pData->idxRemaining == 0)
    {
        pData->icons.second.setResName(pName);
        return FALSE;    // We're done enumerating
    }
    else if(pData->idxRemaining > 0)
    {
        --pData->idxRemaining;
        return TRUE;   // Keep going to the index
    }

    // Otherwise, pData->idxRemaining is negative, no valid index was given.
    // We're done enumerating.
    return FALSE;
}

WinResourceModule::WinResourceModule(LPCWSTR pModulePath)
    : _module{nullptr}
{
    _module = ::LoadLibraryExW(pModulePath, nullptr,
                               LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if(!_module)
    {
        qWarning() << "Can't load" << QStringView{pModulePath} << "to read icons";
        throw SystemError{HERE};
    }
}

WinResourceModule::~WinResourceModule()
{
    ::FreeLibrary(_module);
}

std::pair<WinResId, WinResId> WinResourceModule::findIconRes(int index,
                                                             const QStringView &traceModulePath) const
{
    FindIconResData data;
    data.idxRemaining = index;
    // Enumerate resources of the given type, update *pResId if one is
    // lower.
    if(!::EnumResourceNamesExW(_module, RT_GROUP_ICON,
                               &WinResourceModule::enumResourceNameProc,
                               reinterpret_cast<LONG_PTR>(&data),
                               0, 0))
    {
        // This seems to return false with no error code when the module
        // doesn't contain any resources of this type.  Don't trace that.
        //
        // Ignore ERROR_RESOURCE_TYPE_NOT_FOUND - same thing.
        //
        // Also ignore ERROR_RESOURCE_ENUM_USER_STOP, which happens when we
        // grab the first icon resource (and the callback says to stop
        // enumerating).
        auto code = ::GetLastError();
        if(code != ERROR_SUCCESS && code != ERROR_RESOURCE_TYPE_NOT_FOUND &&
           code != ERROR_RESOURCE_ENUM_USER_STOP)
        {
            qWarning() << "Couldn't enumerate icons in" << traceModulePath
                << "-" << SystemError{HERE, code};
        }
    }

    return std::move(data.icons);
}

std::wstring WinResourceModule::loadString(unsigned id) const
{
    LPWSTR pStr{nullptr};
    int len = ::LoadStringW(_module, id, reinterpret_cast<LPWSTR>(&pStr), 0);
    if(len <= 0)
    {
        qInfo() << "Can't load string resource" << id;
        throw SystemError{HERE};
    }

    return {pStr, static_cast<std::size_t>(len)};
}
