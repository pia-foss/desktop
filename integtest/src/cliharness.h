// Copyright (c) 2021 Private Internet Access, Inc.
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
#line HEADER_FILE("cliharness.h")

#ifndef CLIHARNESS_H
#define CLIHARNESS_H

#include "linebuffer.h"
#include <QProcess>

// Test harness based on the CLI interface - use this to invoke piactl commands
// from integration tests.  If the CLI interface fails in any unexpected way,
// an exception is thrown.
//
// These should only be used in test functions (a few functions use QTest APIs,
// like QTest::qWaitFor()).
//
// CliHarness models "one shot" commands, as well as blocking for specific
// using CliMonitor.  It doesn't directly model "ongoing" commands such as
// "monitor"/"watch", use CliMonitor for that.
namespace CliHarness
{
    // Get the version of piactl
    QString getVersion();
    // Connect to the VPN
    void connectVpn();
    // Disconnect from the VPN
    void disconnectVpn();
    // Reset all settings to defaults.  By default, debug logging is enabled
    // after resetting (since this is for tests)
    void resetSettings(bool enableLogging = true);
    // Get a value.  (Trims the terminating line break.)
    QString get(const QString &type);
    // Change a single setting value
    void applySetting(const QString &name, const QJsonValue &value);

    // Wait for a specific monitored state value.  Success is checked using
    // VERIFY_CONTINUE.
    // Returns false if the value isn't reached in time - tests can abort if
    // necessary in this case.  Many tests can just ignore this.
    // Note that the default timeout is higher than qWaitFor() since VPN state
    // changes can take several seconds.
    bool waitFor(const QString &monitorType, const QString &value,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

    // Disconnect and wait for it to complete.  Shortcut for disconnectVpn()
    // followed by waitFor(), which is very common.
    bool disconnectAndWait(std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});
}

// Test harness for ongoing commands that monitor daemon state. Can be used to
// observe state changes, wait for a specific state, etc.
//
// CliMonitor monitors a specific value provided by the "piactl monitor"
// command.  The initial state and changes are notified with the valueChanged()
// signal.
//
// The current value can be obtained with value(), but keep in mind that changes
// are asynchronous, so the value could already be out-of-date.  value() returns
// an empty string until the first value is received.
//
// Because changes are asynchronous, the caller must start an event loop to
// receive any changes (in tests, this is often done with QSignalSpy).  This
// also means that value() is always empty after constructing CliMonitor until
// the caller starts an event loop to receive changes.
class CliMonitor : public QObject
{
    Q_OBJECT

public:
    // This constructor starts monitoring a specific value provided by the
    // "piactl monitor" command.
    explicit CliMonitor(const QString &type);
    ~CliMonitor();

private:
    CliMonitor(const CliMonitor &) = delete;
    CliMonitor &operator=(const CliMonitor &) = delete;

public:
    const QString &value() {return _value;}

signals:
    void valueChanged(const QString &value);

private:
    QProcess _piactl;
    LineBuffer _piactlStdout;
    LineBuffer _piactlStderr;   // Just for diagnostics
    QString _value;
};

#endif
