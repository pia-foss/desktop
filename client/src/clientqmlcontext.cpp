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
#line SOURCE_FILE("clientqmlcontext.cpp")

#include "clientqmlcontext.h"
#include "brand.h"

// JS regexes to replace all occurrences of {{BRAND_CODE}}, {{BRAND}} and
// {{BRAND_SHORT}}; braces have to be escaped
#define BRAND_CODE_JS_REGEX R"(/\{\{BRAND_CODE\}\}/g)"
#define BRAND_TEMPLATE_JS_REGEX R"(/\{\{BRAND\}\}/g)"
#define BRAND_SHORT_TEMPLATE_JS_REGEX R"(/\{\{BRAND_SHORT\}\}/g)"

ClientQmlContext::ClientQmlContext(QJSEngine &jsEngine)
{
    // Create a __brandSubstitute() function that performs brand substitutions
    // on a piece of UI text.
    jsEngine.evaluate(QStringLiteral("function __brandSubstitute(val) {"
          "val = val.replace(" BRAND_CODE_JS_REGEX ", '" BRAND_CODE "');"
          "val = val.replace(" BRAND_TEMPLATE_JS_REGEX ", '" BRAND_NAME "');"
          "val = val.replace(/Private Internet Access/g, '" BRAND_NAME "');"
          "val = val.replace(" BRAND_SHORT_TEMPLATE_JS_REGEX ", '" BRAND_SHORT_NAME "');"
          "val = val.replace(/PIA/g, '" BRAND_SHORT_NAME "');"
          "return val;"
        "}"));

    // Create 2 new functions which replace {{BRAND}} with the appropriate brand name
    // and acts as a wrapper to qsTr
    jsEngine.evaluate(QStringLiteral("function __brandTr() {"
          "var val = qsTr.apply(null, arguments);"
          "return __brandSubstitute(val);"
        "}"));

    jsEngine.evaluate(QStringLiteral("function __brandTranslate() {"
          "var val = qsTranslate.apply(null, arguments);"
          "return __brandSubstitute(val);"
        "}"));

    _uiTrFunc = jsEngine.evaluate(QStringLiteral("__brandTr"));
    _uiTranslateFunc = jsEngine.evaluate(QStringLiteral("__brandTranslate"));
    _uiBrandFunc = jsEngine.evaluate(QStringLiteral("__brandSubstitute"));
    Q_ASSERT(_uiTrFunc.isCallable());  // Should return a function
    Q_ASSERT(_uiTranslateFunc.isCallable());
    Q_ASSERT(_uiBrandFunc.isCallable());
}

const QJSValue &ClientQmlContext::getUiTr() const
{
    return _uiTrFunc;
}

const QJSValue &ClientQmlContext::getUiTranslate() const
{
    return _uiTranslateFunc;
}

const QJSValue &ClientQmlContext::getUiBrand() const
{
    return _uiBrandFunc;
}

void ClientQmlContext::retranslate()
{
    // The goal here is to re-evaluate all property bindings that depend on
    // uiTr().  In order to do that, we have to cause an expression like
    // `uiTr("foo")` to introduce a dependency on a property that we can
    // signal.
    //
    // (QQmlEngine::retranslate() is not usable, because it re-evaluates _all_
    // property bindings in the entire app.  Window.window does not seem to work
    // in this blast, so this causes widespread breakage.)
    //
    // The trick here is to implement 'uiTr' as a property binding that returns
    // the usual qsTr function.  That way, any code calling uiTr() depends on
    // this property, and we can signal a change in that property to cause those
    // bindings to be re-evaluated.
    //
    // We don't want to add any complexity to the client QML if we can avoid it
    // - translation is used _extensively_.  Importing a specific JS library,
    // calling `util.tr("...")`, etc., would add lots of widespread boilerplate
    // throughout the client code.  'uiTr()' is the least possible overhead.
    //
    // Unfortunately, we can't just call our own property 'qsTr' - Qt defines
    // the original qsTr as a property of the global object, which takes
    // precedence over any context properties.
    qInfo() << "retranslate now";
    emit uiTrChanged();
}
