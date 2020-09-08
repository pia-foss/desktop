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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QObject>
#include <QPalette>
#include <QCommandLineParser>
#include <QQuickWindow>
#include <QStandardPaths>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#endif
#include "reporthelper.h"
#include "payloadbuilder.h"
#include "common.h"
#include "appsingleton.h"
#include "path.h"
#include "version.h"

static const QtMessageHandler QT_DEFAULT_MESSAGE_HANDLER = qInstallMessageHandler(0);


void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toUtf8();
    QFile logFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/pia-reporter.log");
    logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    logFile.write(localMsg);
    logFile.write("\n");
    logFile.close();

    (*QT_DEFAULT_MESSAGE_HANDLER)(type, context, msg);
}


int main(int argc, char *argv[])
{
    // The support tool doesn't currently use QTextStream/QTextCodec/
    // QString::toLocal8Bit(), but this fix is here anyway because it's too easy
    // to forget to test non-Latin locales on Windows, this could easily come
    // back to bite us otherwise.
    setUtf8LocaleCodec();

    qDebug () << "Started PIA Reporter Tool";
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // On any platform except Mac, always use software rendering in the crash
    // reporter.  Software rendering always works; hardware rendering does not
    // work on all machines since we don't deploy ANGLE.
    // Don't do this on Mac though, it doesn't work with multiple monitors (the
    // window displays garbage when dragged to a non-primary monitor, might also
    // have to do with different resolution between the Retina display and my
    // 1080p LGs though.
#ifndef Q_OS_MAC
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
#endif

    QCommandLineParser parser;

    QCommandLineOption logsOption = QCommandLineOption("log", "Add a log file to the payload", "logfile");
    parser.addOption(logsOption);
    QCommandLineOption fileOption = QCommandLineOption("file", "Add a misc file to include in the payload", "file");
    parser.addOption(fileOption);
    QCommandLineOption modeOption = QCommandLineOption("mode", "Mode to run the application in", "modename");
    parser.addOption(modeOption);
    QCommandLineOption clientSettingsOption = QCommandLineOption("client-settings", "Path to client settings file", "clientsettingspath");
    parser.addOption(clientSettingsOption);
    QCommandLineOption clientCrashesOption = QCommandLineOption("client-crashes", "Path to client crashes", "crashpath");
    parser.addOption(clientCrashesOption);
    QCommandLineOption daemonCrashesOption = QCommandLineOption("daemon-crashes", "Path to daemon crashes", "crashpath");
    parser.addOption(daemonCrashesOption);
    QCommandLineOption invokePipeOption{"invoke-pipe", {}, "pipefd"};
    invokePipeOption.setFlags(QCommandLineOption::Flag::HiddenFromHelp);
    parser.addOption(invokePipeOption);

    Path::initializePreApp();

    QGuiApplication app(argc, argv);
    Path::initializePostApp();
    parser.process(app);

    AppSingleton runGuard(Path::SupportToolExecutable);
    if(runGuard.isAnotherInstanceRunning() > 0) {
        qWarning () << "Exiting because another instance appears to be running";
        app.quit();
        return -1;
    }

    bool restartInvoked = checkAutoRestart(parser.value(clientSettingsOption), parser.value(clientCrashesOption));

    if(restartInvoked) {
        // Start an application main loop so the app can have enough time to restart
        return app.exec();
    }

    bool invokePipeOk = false;
    int invokePipeArg = parser.value(invokePipeOption).toInt(&invokePipeOk);
    if(!invokePipeOk)
        invokePipeArg = -1;
#ifdef Q_OS_LINUX
    // Don't inherit the pipe fd.  Normally we don't exec anything directly if
    // the pipe fd is set, but on the off chance the launcher is killed, we
    // still don't want the client to inherit this.
    if(invokePipeArg >= 0)
        fcntl(invokePipeArg, F_SETFD, FD_CLOEXEC);
#endif

    qmlRegisterSingletonType<ReportHelper>("PIA.ReportHelper", 1, 0, "ReportHelper",
        [](auto, auto) -> QObject* {return new ReportHelper();});
    qmlRegisterSingletonType<PayloadBuilder>("PIA.PayloadBuilder", 1, 0, "PayloadBuilder",
        [](auto, auto) -> QObject* {return new PayloadBuilder();});

#if !defined(NDEBUG)
    QPalette appPalette = QGuiApplication::palette();
    // Dump the system palette - this was used to dump the light theme palette
    // stored in ReportHelper.  If the color roles change, this can be used to
    // re-dump the system light palette.
    qInfo() << "System palette:";
    for(int group = QPalette::Active; group <= QPalette::Inactive; ++group)
    {
        for(int role = QPalette::WindowText; role <= QPalette::ToolTipText; ++role)
        {
            const char *groupName = QMetaEnum::fromType<QPalette::ColorGroup>().valueToKey(group);
            const char *roleName = QMetaEnum::fromType<QPalette::ColorRole>().valueToKey(role);
            const auto &color = appPalette.color(static_cast<QPalette::ColorGroup>(group),
                                                 static_cast<QPalette::ColorRole>(role));
            qInfo().nospace().noquote() << "appPalette.setColor(QPalette::"
                << groupName << ", QPalette::" << roleName << ", {"
                << color.red() << ", " << color.green() << ", " << color.blue()
                << ", " << color.alpha() << "});";
        }
    }
#endif

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/components/support-tool-main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    // To set the configuration parameters into QML, we get the "params" object
    // and set the values directly on it.
    QObject* params = engine.rootObjects().first()->findChild<QObject *>("params");
    if(params) {
        params->setProperty("mode", parser.value(modeOption));
        params->setProperty("logs", ReportHelper::ensureFilesExist(parser.values(logsOption))); // A list of log files
        params->setProperty("files", ReportHelper::ensureFilesExist(parser.values(fileOption))); // A list of files
        params->setProperty("client_crash", parser.value(clientCrashesOption));
        params->setProperty("daemon_crash", parser.value(daemonCrashesOption));
        params->setProperty("version", QStringLiteral(PIA_VERSION));
        params->setProperty("invoke_pipe", invokePipeArg);

        ReportHelper::setUIParams(params);
    }

    return app.exec();
}
