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
#line SOURCE_FILE("watchcommand.cpp")

#include "watchcommand.h"
#include "cliclient.h"
#include "output.h"
#include <unordered_set>

class JsonChangePrinter : public QObject
{
    Q_OBJECT

private:
    // A few huge (and largely uninteresting) properties are not printed
    static const std::unordered_set<QString> propertyBlacklist;

public:
    JsonChangePrinter(const NativeJsonObject &obj, const QString &name)
        : _obj{obj}, _name{name}
    {
        connect(&obj, &NativeJsonObject::propertyChanged, this,
                &JsonChangePrinter::printChange);
    }

public:
    void printChange(const QString &propName)
    {
        if(propertyBlacklist.count(propName))
            return;

        QJsonObject valueObj;
        valueObj.insert(propName, _obj.get(propName));
        QJsonDocument valueDoc{valueObj};

        outln() << _name << ": " << valueDoc.toJson(QJsonDocument::JsonFormat::Compact);
    }

private:
    const NativeJsonObject &_obj;
    QString _name;
};

const std::unordered_set<QString> JsonChangePrinter::propertyBlacklist{
    QStringLiteral("certificateAuthorities"),
    QStringLiteral("locations"),
    QStringLiteral("groupedLocations")
};

void WatchCommand::printHelp(const QString &name)
{
    outln() << "usage:" << name;
    outln() << "Monitors the PIA daemon for changes in settings, state, or data";
    outln() << "When a connection is established, prints the initial changes from the default state";
    outln() << "When a change is received, a line is printed of the form:";
    outln() << "  <group>: <change-object>";
    outln() << "  - group: Group where change occurred: settings, state, or data";
    outln() << "  - change-object: JSON object containing changed properties";
    outln() << "This command continues to run until terminated.";
}

int WatchCommand::exec(const QStringList &params, QCoreApplication &app)
{
    checkNoParams(params);

    CliClient client;

    QObject localConnState{};
    JsonChangePrinter data{client.connection().data, QStringLiteral("data")};
    JsonChangePrinter settings{client.connection().settings, QStringLiteral("settings")};
    JsonChangePrinter state{client.connection().state, QStringLiteral("state")};

    return app.exec();
}

#include "watchcommand.moc"
