// Copyright (c) 2026 Private Internet Access, Inc.
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
#include "wg_service_cli_task.h"
#include "wg_service_json.h"
#include <QTimer>
#include <chrono>
#include <common/src/builtin/path.h>

namespace
{
// Timeout for wgservice.exe execution
constexpr std::chrono::seconds CommandTimeout{2};
}

WgServiceCliTask::WgServiceCliTask(QString interfaceName)
    : _interfaceName{std::move(interfaceName)}
{
    executeCommand();
}

WgServiceCliTask::~WgServiceCliTask()
{
    if(_process)
    {
        _process->disconnect(); // Disconnect all signals first
        if(_process->state() != QProcess::NotRunning)
        {
            _process->kill();
            _process->waitForFinished(1000);
        }
        _process.reset(); // Explicit cleanup via unique_ptr
    }
}

void WgServiceCliTask::executeCommand()
{
    auto keepAlive = sharedFromThis();

    _process = std::make_unique<QProcess>();

    // Set up process finished handler
    connect(_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, keepAlive](int exitCode, QProcess::ExitStatus exitStatus) {
                if(!isPending())
                {
                    return; // Already resolved/rejected
                }

                if(exitStatus != QProcess::NormalExit)
                {
                    qWarning() << "wgservice.exe crashed for interface" << _interfaceName;
                    reject({HERE, Error::Code::WireguardNotResponding});
                    return;
                }

                if(exitCode != 0)
                {
                    QString stdErr = QString::fromUtf8(_process->readAllStandardError());
                    qWarning() << "wgservice.exe failed with exit code" << exitCode
                               << "for interface" << _interfaceName << "- stderr:" << stdErr;
                    reject({HERE, Error::Code::WireguardNotResponding});
                    return;
                }

                // Read and parse JSON output
                QByteArray jsonOutput = _process->readAllStandardOutput();
                if(jsonOutput.isEmpty())
                {
                    qWarning() << "wgservice.exe returned empty output for interface"
                               << _interfaceName;
                    reject({HERE, Error::Code::WireguardNotResponding});
                    return;
                }

                qDebug() << "wgservice.exe output:" << jsonOutput.data();

                // Parse JSON using the WgServiceJson namespace utilities
                auto pStatus = WgServiceJson::parseWgServiceJson(jsonOutput);
                if(!pStatus)
                {
                    qWarning() << "Failed to parse JSON from wgservice.exe";
                    reject({HERE, Error::Code::WireguardNotResponding});
                    return;
                }

                resolve(std::move(pStatus));
            });

    // Set up error handler
    connect(_process.get(), &QProcess::errorOccurred, this,
            [this, keepAlive](QProcess::ProcessError error) {
                if(!isPending())
                {
                    return;
                }

                QString errorStr;
                switch(error)
                {
                    case QProcess::FailedToStart:
                        errorStr = "failed to start (wgservice.exe not found?)";
                        break;
                    case QProcess::Timedout: errorStr = "timed out"; break;
                    default: errorStr = QString("error %1").arg(static_cast<int>(error)); break;
                }

                qWarning() << "wgservice.exe" << errorStr << "for interface" << _interfaceName;
                reject({HERE, Error::Code::WireguardNotResponding});
            });

    // Set up timeout timer
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, keepAlive]() {
        if(!isPending())
        {
            return;
        }

        qWarning() << "wgservice.exe timed out after" << CommandTimeout.count() << "seconds";
        _process->kill();
        reject({HERE, Error::Code::WireguardNotResponding});
    });

    // Build command: wgservice.exe /getconfig <interfaceName>
    QString program = Path::WireguardServiceExecutable;
    QStringList arguments;
    arguments << QStringLiteral("/getconfig") << _interfaceName;

    qInfo() << "Executing:" << program << arguments.join(' ');

    // Start process asynchronously
    _process->start(program, arguments);

    // Start timeout timer
    using namespace std::chrono;
    timeoutTimer->start(duration_cast<milliseconds>(CommandTimeout).count());
}
