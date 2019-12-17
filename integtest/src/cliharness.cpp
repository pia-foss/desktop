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
#line SOURCE_FILE("cliharness.cpp")

#include "cliharness.h"
#include "integtestcase.h"
#include "brand.h"
#include "path.h"
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
                qWarning() << "output for" << args << "didn't end in line break:" << output;
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

    void resetSettings(bool enableLogging)
    {
        execute({"resetsettings"});
        if(enableLogging)
            execute({"set", "debuglogging", "true"});
    }

    QString get(const QString &type)
    {
        return executeWithOutput({"get", type});
    }

    void applySetting(const QString &name, const QJsonValue &value)
    {
        QJsonObject settings;
        settings.insert(name, value);
        auto settingsJson = QJsonDocument{settings}.toJson(QJsonDocument::JsonFormat::Compact);
        execute({"--unstable", "applysettings", settingsJson});
    }

    bool waitFor(const QString &monitorType, const QString &value,
                             std::chrono::milliseconds timeout)
    {
        CliMonitor monitor{monitorType};
        auto success = QTest::qWaitFor([&]() -> bool {return monitor.value() == value;}, msec(timeout));
        if(!success)
        {
            qWarning() << "Failed to wait for" << monitorType << "to be" << value
                << "after" << traceMsec(timeout);
        }
        VERIFY_CONTINUE(success);
        return success;
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
        qInfo() << "monitor updated with value" << line;
        _value = QString::fromLocal8Bit(line);
        emit valueChanged(_value);
    });

    connect(&_piactlStderr, &LineBuffer::lineComplete, this, [](const QByteArray &line)
    {
        qInfo() << "monitor stderr:" << line;
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
