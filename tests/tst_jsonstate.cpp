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

#include <common/src/jsonstate.h>
#include <kapps_core/src/logger.h>
#include <QtTest>
#include <deque>
#include <nlohmann/json.hpp>

// Small mock comparable to DaemonState
class MockState : public JsonState<nlohmann::json>
{
public:
    // Default-constructible (given initializers below) and
    // default-assignable
    MockState() = default;
    MockState &operator=(const MockState&) = default;
    // Copy construction must delegate to construction+assignment
    MockState(const MockState &other) : MockState{} {*this = other;}

public:
    // Similar to DaemonState::connectionState
    JsonProperty(std::string, connectionState, "Disconnected");
    // Similar to DaemonState::connectedServer, mocked as only the CN
    JsonProperty(std::string, connectedServer);
    // Similar to DaemonState::intervalMeasurements, mocked as only down speeds
    JsonProperty(std::deque<int>, intervalMeasurements);
};

class PropertyObserver
{
public:
    PropertyObserver(JsonState<nlohmann::json> &state)
    {
        state.propertyChanged = [this](kapps::core::StringSlice name)
        {
            _changes.push_back(name.to_string());
        };
    }

public:
    // Take the current set of changes for validation (returns the existing
    // changes and clears the internal state)
    std::deque<std::string> take()
    {
        std::deque<std::string> result{std::move(_changes)};
        _changes.clear();
        return result;
    }

public:
    std::deque<std::string> _changes;
};

class tst_jsonstate : public QObject
{
    Q_OBJECT

private:
    // Name for type of expected changes from PropertyObserver::take(), this is
    // used a lot throughout these tests.
    using Expected = std::deque<std::string>;

private slots:
    // Test building JSON for the whole object
    void testJsonObject()
    {
        MockState state;
        state.connectionState("Connected");
        state.connectedServer("newyork405");
        state.intervalMeasurements({1, 2, 3, 4, 9001});

        auto actual = state.getJsonObject();
        auto expected = nlohmann::json{
            {"connectionState", "Connected"},
            {"connectedServer", "newyork405"},
            {"intervalMeasurements", {1, 2, 3, 4, 9001}}
        };

        KAPPS_CORE_INFO() << "got:" << actual.dump(2);
        KAPPS_CORE_INFO() << "exp:" << expected.dump(2);

        QCOMPARE(actual, expected);
    }

    // Test observing property changes
    void testPropertyChanges()
    {
        MockState state;
        PropertyObserver obs{state};

        state.connectionState("Connected");
        // The change was signaled
        QCOMPARE(obs.take(), (Expected{"connectionState"}));

        state.connectionState("Connected");
        // No change, so no signal
        QCOMPARE(obs.take(), (Expected{}));

        // Multiple changes
        state.connectionState("Reconnecting");
        state.connectedServer("newyork405");
        state.connectionState("Connected");
        QCOMPARE(obs.take(), (Expected{"connectionState", "connectedServer",
            "connectionState"}));

        // Unchanged values are still detected even after a change and with
        // other property changes
        state.intervalMeasurements({1, 2, 3, 4});
        state.connectionState("Disconnected");
        state.intervalMeasurements({1, 2, 3, 4});   // No change
        state.connectionState("Connecting");
        state.intervalMeasurements({1, 2, 3, 4, 64});
        QCOMPARE(obs.take(), (Expected{"intervalMeasurements", "connectionState",
            "connectionState", "intervalMeasurements"}));

        // Verify the properties' individual signals too
        std::deque<std::string> individualChanges;
        state.connectionState.changed = [&]{individualChanges.push_back("connectionState");};
        state.connectedServer.changed = [&]{individualChanges.push_back("connectedServer");};
        state.connectionState("Reconnecting");
        state.connectedServer("chicago402");
        state.connectionState("Connected");
        QCOMPARE(obs.take(), (Expected{"connectionState", "connectedServer",
            "connectionState"}));
        QCOMPARE(individualChanges, (Expected{"connectionState", "connectedServer",
            "connectionState"}));
    }

    // Copying and assignment work
    void testCopy()
    {
        MockState original;
        original.connectionState("Connected");
        original.connectedServer("chicago421");
        original.intervalMeasurements({10, 20, 30, 40});

        PropertyObserver orgObs{original};

        MockState duplicate{original};
        PropertyObserver dupObs{duplicate};

        QCOMPARE(original.connectionState(), duplicate.connectionState());
        QCOMPARE(original.connectedServer(), duplicate.connectedServer());
        QCOMPARE(original.intervalMeasurements(), duplicate.intervalMeasurements());

        // There are no changes in either yet (we created orgObs later to skip
        // setup of original; we can't observe changes during copy construction
        // of duplicate
        QCOMPARE(orgObs.take(), (Expected{}));
        QCOMPARE(dupObs.take(), (Expected{}));

        // Make some changes to the original
        original.connectedServer("newyork405");
        original.intervalMeasurements({100, 200, 300, 400});
        original.connectionState("Disconnected");
        // We observed those changes
        QCOMPARE(orgObs.take(), (Expected{"connectedServer",
            "intervalMeasurements", "connectionState"}));

        // Assign to duplicate and verify both the values and changes are
        // observed
        duplicate = original;
        QCOMPARE(original.connectionState(), duplicate.connectionState());
        QCOMPARE(original.connectedServer(), duplicate.connectedServer());
        QCOMPARE(original.intervalMeasurements(), duplicate.intervalMeasurements());
        // The order of changes _is_ well-defined, it is the order in which the
        // properties are defined in MockState (not an unspecified order from
        // the internal map or a lexical order from a JSON object)
        QCOMPARE(dupObs.take(), (Expected{"connectionState", "connectedServer",
            "intervalMeasurements"}));

        // Assignment does not emit changes for fields that did not change
        MockState unchanged;
        PropertyObserver unchangedObs{unchanged};
        // duplicate.connectionState() has the default value now, so no change
        // is observed
        unchanged = duplicate;
        QCOMPARE(unchangedObs.take(), (Expected{"connectedServer", "intervalMeasurements"}));
    }
};

QTEST_GUILESS_MAIN(tst_jsonstate);
#include TEST_MOC
