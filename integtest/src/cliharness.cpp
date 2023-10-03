// Copyright (c) 2023 Private Internet Access, Inc.
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
#line SOURCE_FILE("cliharness.cpp")

#include "cliharness.h"
#include "integtestcase.h"
#include "brand.h"
#include <common/src/builtin/path.h>
#include <common/src/settings/daemonsettings.h>
#include <QProcess>
#include <QTest>
#include <QJsonDocument>

namespace
{
    const QString &piactlPath()
    {
        // Function local static for initialization on first use (depends on
        // Path initialization)
        static const QString path = Path::InstallationExecutableDir / QStringLiteral(BRAND_CODE "ctl");
        return path;
    }
}

namespace CliHarness
{
    namespace
    {
        QByteArray execute(const QStringList &args)
        {
            QProcess ctlProc;
            ctlProc.setProgram(piactlPath());
            ctlProc.setArguments(args);
            ctlProc.start();
            ctlProc.waitForFinished();

            if(ctlProc.exitStatus() != QProcess::NormalExit || ctlProc.exitCode() != 0)
            {
                qWarning() << "CLI command failed - " << args << "- status:"
                    << ctlProc.exitStatus() << "- exit code:" << ctlProc.exitCode();
                VERIFY_CONTINUE(false);
            }

            return ctlProc.readAllStandardOutput();
        }

        QString processOutput(const QStringList &args, QByteArray output)
        {
            if(output.endsWith('\n'))
            {
                output.chop(1);

                // If the line ending was CRLF, trim the CR too.  LF line endings are
                // accepted even on Windows though (just means that CR can't be used in
                // the content, which is fine).
                if(output.endsWith('\r'))
                    output.chop(1);
            }
            else
            {
                // It should always end with a line break
                qWarning() << "output for" << args << "didn't end in line break:" << output.data();
            }

            return QString::fromLocal8Bit(output);
        }

        QString executeWithOutput(const QStringList &args)
        {
            return processOutput(args, execute(args));
        }
    }

    QString getVersion()
    {
        return executeWithOutput({"--version"});
    }

    void connectVpn()
    {
        execute({"connect"});
    }

    void disconnectVpn()
    {
        execute({"disconnect"});
    }

    QString get(const QString &type)
    {
        return executeWithOutput({"get", type});
    }

    void set(const QString &type, const QString &value)
    {
        execute({"set", type, value});
    }

    void applySetting(const QString &name, const QJsonValue &value)
    {
        QJsonObject settings;
        settings.insert(name, value);
        applySettings(settings);
    }

    void applySettings(const QJsonObject &settings)
    {
        execute({"--unstable", "applysettings",
            QJsonDocument{settings}.toJson(QJsonDocument::JsonFormat::Compact)});
    }

    void resetSettings()
    {
        // Although there is a 'resetsettings' CLI command that uses the
        // resetSettings RPC, here we need to exclude a few additional settings.
        QJsonObject defaultsJson{DaemonSettings{}.toJsonObject()};
        // Remove settings that are normally excluded
        for (const auto &excludedSetting : DaemonSettings::settingsExcludedFromReset())
            defaultsJson.remove(excludedSetting);

        // Also exclude settings that were specifically set up by initSettings()
        defaultsJson.remove(QStringLiteral("automaticTransport"));
        // Debug logging is already excluded by default in settingsExcludedFromReset()

        // Also exclude the update channels.  The GA update channel must
        // sometimes be overridden during tests to run the test with feature
        // flags that aren't yet published.  (The beta update channel doesn't
        // impact feature flags, but it'd strange to reset one and not the
        // other.)
        defaultsJson.remove(QStringLiteral("updateChannel"));
        defaultsJson.remove(QStringLiteral("betaUpdateChannel"));

        // Exclude the service quality events setting - if this has been turned
        // on, keep it on.  Currently, we're testing events with integ tests.
        // In the future this may stay on since we typically run integ tests
        // against builds using the "staging" (prerelease) product ID, although
        // we should avoid sending production events from integ tests.
        defaultsJson.remove(QStringLiteral("sendServiceQualityEvents"));

        applySettings(defaultsJson);
    }

    bool waitForPredicate(const QString &monitorType, std::function<bool(const QString &)> predicate,
                          std::chrono::milliseconds timeout) 
    {
        CliMonitor monitor{monitorType};
        auto success = QTest::qWaitFor([&]() -> bool {return predicate(monitor.value());}, msec(timeout));
        if(!success)
        {
            qWarning() << "Failed to wait for" << monitorType
                       << "to match the predicate after " << traceMsec(timeout) << "ms";
        }
        VERIFY_CONTINUE(success);
        return success;
    }

    bool waitFor(const QString &monitorType, const QString &value,
                 std::chrono::milliseconds timeout) 
    {
        return waitForPredicate(monitorType, [&](auto currentValue) -> bool { return currentValue == value;});
    }

    bool waitForNotEqual(const QString &monitorType, const QString &value,
                         std::chrono::milliseconds timeout)
    {
        return waitForPredicate(monitorType, [&](auto currentValue) -> bool { return currentValue.size() > 0 && currentValue != value;});
    }

    bool disconnectAndWait(std::chrono::milliseconds timeout)
    {
        disconnectVpn();
        return waitFor(QStringLiteral("connectionstate"), QStringLiteral("Disconnected"), timeout);
    }
}

CliMonitor::CliMonitor(const QString &type)
{
    connect(&_piactlStdout, &LineBuffer::lineComplete, this, [this](const QByteArray &line)
    {
        if (line.size() == 0) return;
        _value = QString::fromLocal8Bit(line);
        // Ignore empty strings
        qInfo() << "monitor updated with value" << _value;
        emit valueChanged(_value);
    });

    connect(&_piactlStderr, &LineBuffer::lineComplete, this, [](const QByteArray &line)
    {
        qInfo() << "monitor stderr:" << line.data();
    });

    connect(&_piactl, &QProcess::readyReadStandardOutput, this, [this]()
    {
        _piactlStdout.append(_piactl.readAllStandardOutput());
    });
    connect(&_piactl, &QProcess::readyReadStandardError, this, [this]()
    {
        _piactlStderr.append(_piactl.readAllStandardError());
    });

    _piactl.setProgram(piactlPath());
    _piactl.setArguments({QStringLiteral("monitor"), type});
    _piactl.start();

    // There's no handling of exit status or exit code - this is used by tests,
    // we expect the type to be valid, and piactl monitor never exits if the
    // type is valid.
}

CliMonitor::~CliMonitor()
{
    _piactl.kill();
    _piactl.waitForFinished();
}
