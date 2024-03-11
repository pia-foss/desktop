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
#line HEADER_FILE("clientqmlcontext.h")

#ifndef CLIENTQMLCONTEXT_H
#define CLIENTQMLCONTEXT_H

#include <QObject>
#include <QJSValue>
#include <QJSEngine>

// This class defines the object that we set as global QML context for our QML
// It defines some globals that are essential enough for all QML to have access
// to them (currently, uiTr()).
class ClientQmlContext : public QObject
{
    Q_OBJECT

public:
    ClientQmlContext(QJSEngine &jsEngine);

public:
    // This property defines the global uiTr() function that we use to
    // translate strings in the client QML.
    // This is nearly identical to qsTr(), except that it introduces a property
    // dependency where it's called that we can then signal to re-evaluate
    // translations.
    // See retranslate()'s implementation for the long-winded explanation why
    // this is done this way.
    Q_PROPERTY(QJSValue uiTr READ getUiTr NOTIFY uiTrChanged)
    // Similarly, this is an equivalent to qsTranslate, which can be used when
    // the context needs to be set manually.
    // It shares a notify signal with uiTr.
    Q_PROPERTY(QJSValue uiTranslate READ getUiTranslate NOTIFY uiTrChanged)
    // This property defines a uiBrand() function that just applies branding
    // substitutions to a piece of UI text.  Most code uses uiTr()/uiTranslate()
    // instead of calling this directly, but the Changelog uses this (it is
    // branded but not translated).
    Q_PROPERTY(QJSValue uiBrand READ getUiBrand CONSTANT)

private:
    const QJSValue &getUiTr() const;
    const QJSValue &getUiTranslate() const;
    const QJSValue &getUiBrand() const;

public:
    // Emit a signal that causes all bindings dependent on uiTr() to be
    // re-evaluated.
    void retranslate();

signals:
    void uiTrChanged();

private:
    QJSValue _uiTrFunc, _uiTranslateFunc, _uiBrandFunc;
};

#endif
