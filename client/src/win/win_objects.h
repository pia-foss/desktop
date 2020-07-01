// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("win/win_objects.h")

#ifndef WIN_OBJECTS_H
#define WIN_OBJECTS_H

#include "nativetray.h"

#include <QHash>
#include <QSharedPointer>
#include <variant>
#include <Windows.h>

template<typename Handle> using SharedHandle = QSharedPointer<std::remove_pointer_t<Handle>>;

// Wrapper for a popup menu (owns the HMENU).
class WinPopupMenu
{
public:
    WinPopupMenu();
    ~WinPopupMenu();

private:
    // Not copiable (would have to duplicate the menu)
    WinPopupMenu(const WinPopupMenu&) = delete;
    WinPopupMenu &operator=(const WinPopupMenu&) = delete;

    HMENU createMenu(const NativeMenuItem::List& items);

public:
    // (Re)create the menu with the given items.
    void setItems(const NativeMenuItem::List& items);

    // Show the menu.  If the user makes a selection, returns the ID of the menu
    // item that was selected.  Returns undefined if nothing was selected.
    // The window passed won't receive any messages, but it will be made the
    // foreground window.
    QString showMenu(HWND parentWnd, const QPoint &pos);

private:
    SharedHandle<HMENU> _menu;
    QHash<UINT_PTR, QString> _menuCodes;
    QHash<QString, SharedHandle<HBITMAP>> _icons;
    UINT _lastId = 0;
};

// Icon loader (owns the HICON)
class IconResource
{
public:
    // We can load either small icons (SM_C[XY]SMICON, the tray icon itself) or
    // large icons (SM_C[XY]ICON, the notification balloon icons)
    enum class Size
    {
        Small,
        Large
    };
public:
    // Load an icon from this module with a standard size
    IconResource(WORD resId, Size size);
    // Load an icon from an arbitrary module.  traceModulePath is used only for
    // tracing if the icon can't be loaded.
    IconResource(HMODULE module, LPCWSTR pName, const QSize &size,
                 const QStringView &traceModulePath);
    // Load an icon from an ICO file.
    IconResource(LPCWSTR pPath, const QSize &size);
    ~IconResource();
private:
    IconResource(const IconResource &) = delete;
    IconResource &operator=(const IconResource &) = delete;

private:
    HICON loadModuleIcon(HMODULE module, UINT resId, int cx, int cy,
                         const QString &sizeTrace);
    HICON loadStockIcon(const wchar_t *pRes, int cx, int cy,
                        const QString &sizeTrace);

public:
    HICON getHandle() const {return _icon;}
private:
    HICON _icon;
};

// A resource ID - can be a string for a named resource or an integer for a
// numbered resource.  (When holding a string, this could also be a
// stringified number, like "#1000".)
class WinResId
{
public:
    bool empty() const;
    LPCWSTR getResName() const;
    void setResName(LPCWSTR pName);
private:
    std::variant<std::wstring, WORD> _id;
};

// Resource module loader - loads a module as a resource-only DLL
class WinResourceModule
{
private:
    // Data for findIconRes()
    struct FindIconResData
    {
        int idxRemaining;
        std::pair<WinResId, WinResId> icons;
    };
    // Callback used in findIconRes()
    static BOOL CALLBACK enumResourceNameProc(HMODULE, LPCWSTR, LPWSTR pName,
                                              LONG_PTR lParam);
public:
    WinResourceModule(LPCWSTR pModulePath);
    ~WinResourceModule();
private:
    WinResourceModule(const WinResourceModule &) = delete;
    WinResourceModule &operator=(const WinResourceModule &) = delete;

public:
    HMODULE getHandle() const {return _module;}
    // Find icon resources in this module.  Returns the first icon resource
    // (first) and the icon resource at the position 'index' (second).
    std::pair<WinResId, WinResId> findIconRes(int index,
                                              const QStringView &traceModulePath) const;
    // Load a string resource by ID
    std::wstring loadString(unsigned id) const;

private:
    HMODULE _module;
};

#endif
