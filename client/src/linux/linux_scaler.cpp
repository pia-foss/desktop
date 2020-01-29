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
#line SOURCE_FILE("linux_scaler.cpp")

#include "linux_scaler.h"
#include <QProcess>
#include <QStringList>
#include <QFile>
#include <QRegularExpression>

QString LinuxWindowScaler::_qtScaleEnv;
double LinuxWindowScaler::_scaleFactor = 0;
StaticSignal LinuxWindowScaler::_scaleFactorChange;

double LinuxWindowScaler::checkScaleEnvVar(const QProcessEnvironment &env,
                                           const QString &envVarName)
{
    return parseScaleEnvVar(env.value(envVarName), envVarName);
}

double LinuxWindowScaler::parseScaleEnvVar(const QString &scaleFactorStr,
                                           const QString &envVarName)
{
    if(scaleFactorStr.isEmpty())
        return 0;   //Don't log anything,  not set at all

    bool scaleParseSuccess = false;
    double envScaleFactor = scaleFactorStr.toDouble(&scaleParseSuccess);
    if(!scaleParseSuccess || envScaleFactor <= 0)
    {
        qWarning() << "Invalid value of " << envVarName << "env variable: " << scaleFactorStr;
        return 0;
    }

    qInfo().nospace() << "Scale from " << envVarName << "=" << scaleFactorStr
        << " -> " << envScaleFactor;
    return envScaleFactor;
}

QString LinuxWindowScaler::runCommand(const QString &command,
                                      const QStringList &args)
{
    if (!QFile::exists(command))
        return {};

    QProcess cmdProc;
    cmdProc.setProgram(command);
    cmdProc.setArguments(args);
    cmdProc.start();
    cmdProc.waitForFinished();

    if (cmdProc.exitStatus() != QProcess::NormalExit && cmdProc.exitCode() == 0)
    {
        qWarning() << "Command failed -" << command << "had status"
            << cmdProc.exitStatus() << "and code" << cmdProc.exitCode();
        return {};
    }

    return QString::fromUtf8(cmdProc.readAllStandardOutput()).trimmed();
}

double LinuxWindowScaler::getPiaScale(const QProcessEnvironment &env)
{
    // Check for the env value `PIA_SCALE`. This has the highest priority
    // and overrides all other guesses
    return checkScaleEnvVar(env, QStringLiteral("PIA_SCALE"));
}

double LinuxWindowScaler::getGsettingsScale()
{
    // Check if a GNOME scaling factor has been set.
    // 'gsettings' exists in many desktop environments, but many like KDE don't
    // use these GDK properties, so we still fall back to others if this isn't
    // set.
    QString gsettingsOutput = runCommand(QStringLiteral("/usr/bin/gsettings"),
                                         {QStringLiteral("get"),
                                          QStringLiteral("org.gnome.desktop.interface"),
                                          QStringLiteral("scaling-factor")});
    if (!gsettingsOutput.isEmpty())
    {
        // parse the stdout which would normally look like:
        // "uint32 2" for a scale factor of 2
        // The default is 0, and should be ignored
        qDebug () << "gsettings stdout: " << gsettingsOutput;
        gsettingsOutput.remove(0, 7);
        bool parseSuccess = false;
        unsigned parsedValue = gsettingsOutput.toUInt(&parseSuccess);
        if (parseSuccess)
        {
            // If the scale factor found is nonzero, it sets the scale (even
            // if it's 1)
            if (parsedValue >= 1)
            {
                qInfo() << "Scale from gsettings:" << gsettingsOutput << "->" << parsedValue;
                return parsedValue;
            }
        }
        else
            qWarning () << "Unable to parse gsettings output." << gsettingsOutput;
    }

    return 0;
}

double LinuxWindowScaler::getQtScale(const QProcessEnvironment &env)
{
    // Check if a Qt scaling factor has been applied to the whole virtual
    // desktop.  This handles some Qt-based environments like Deepin.
    return parseScaleEnvVar(_qtScaleEnv, QStringLiteral("QT_SCALE_FACTOR"));
}

double LinuxWindowScaler::getQtScreenScale(const QProcessEnvironment &env)
{
    // Check if Qt per-screen scale factors have been set.
    // KDE does this (at least in Kubuntu 18.04), although it does not actually
    // support per-screen scale factors (they're always set to the same value).
    //
    // We don't support per-screen scale factors on Linux currently - we'd have
    // to detect when the window crosses screen boundaries and change scale
    // factors then.  (It's possible, but no environment seems to allow it
    // anyway.)
    const auto &screenScaleFactorsStr = env.value(QStringLiteral("QT_SCREEN_SCALE_FACTORS"));
    if(!screenScaleFactorsStr.isEmpty())
    {
        // The list can be semicolon-delimited name=value pairs, or just
        // semicolon-delimited scale factors.
        // We're not quite that strict, since we're just picking a single factor
        // we allow each factor to be name=value or just value.
        const auto &factorsList = screenScaleFactorsStr.split(';', QString::SplitBehavior::SkipEmptyParts);

        double qtScreenScale = 0;
        for(const auto &factor : factorsList)
        {
            int equalPos = factor.indexOf('=');
            QString factorNumPart = factor;
            // If there was no equal sign, equalPos=-1, so equalPos+1 is 0 and
            // nothing happens.
            factorNumPart.remove(0, equalPos+1);
            // Use the maximum as the single value.  This is pretty
            // hypothetical, but if there were different factors here, the max
            // probably applies to the primary display), which is where we'll
            // probably appear by default.
            // An average wouldn't make much sense, that would probably look bad
            // on _all_ displays; at least a max will look good on at least one
            // display.
            qtScreenScale = std::max(factorNumPart.toDouble(), qtScreenScale);
        }

        // If we did find at least one nonzero scaling factor, use this value.
        if(qtScreenScale > 0)
        {
            qInfo().nospace() << "Scale set by QT_SCREEN_SCALE_FACTORS="
                << screenScaleFactorsStr << " -> " << qtScreenScale;
            return qtScreenScale;
        }
    }

    return 0;
}

double LinuxWindowScaler::getXftDpiScale()
{
    // Check if the Xft.dpi attribute is set.  Although this purports to be the
    // display's DPI, in reality it's a scaling factor since it scales programs
    // if it's not 96.
    //
    // Fedora sets this (it's present for both X11 and Wayland) and does not set
    // any other scale settings.  Arch GNOME is the same way.
    //
    // Ubuntu and Kubuntu set it when scale is not 1, but they also set other
    // settings that take precedence above.
    //
    // Arch allows the scale to be changed dynamically without a logout (so far
    // it seems to be the only distro that does this).  It seems that we're
    // supposed to receive XSETTINGS events to indicate the change:
    // - https://wiki.debian.org/MonitorDPI (mentions Xft/DPI key)
    // - https://specifications.freedesktop.org/xsettings-spec/xsettings-spec-0.5.html
    //   (spec for XSETTINGS)
    // This isn't currently implemented though.
    QString xrdbOutput = runCommand(QStringLiteral("/usr/bin/xrdb"),
                                    {QStringLiteral("-query")});
    if(!xrdbOutput.isEmpty())
    {
        // The regex specifically doesn't restrict the value to be numeric so
        // we'll get some diagnostic output if xrdb prints a value that we can't
        // understand.
        QRegularExpression dpiRegex{QStringLiteral("^Xft.dpi: *(.+)$"),
                                    QRegularExpression::PatternOption::MultilineOption};
        auto match = dpiRegex.match(xrdbOutput);
        if(match.hasMatch())
        {
            // If the capture group isn't numeric, this returns 0 and we print a
            // diagnostic.
            double x11dpi = match.captured(1).toDouble();
            if(x11dpi > 0)
            {
                double x11scale = x11dpi / 96.0;
                qInfo().nospace() << "DPI setting '" << match.captured(0)
                    << "' from xrdb results in scale -> " << x11scale;
                return x11scale;
            }
            else
            {
                qWarning() << "Value of Xft.dpi from xrdb couldn't be parsed:"
                    << match.captured(0);
            }
        }
    }

    return 0;
}

double LinuxWindowScaler::detectScaleFactor()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Attempt to guess the scale factor through various means.  Detect all of
    // them, even if an earlier guess overrides a later one, so diagnostics
    // indicate multiple or conflicting scale factors.
    double piaScale = getPiaScale(env);
    double gsettingsScale = getGsettingsScale();
    double qtScale = getQtScale(env);
    double qtScreenScale = getQtScreenScale(env);
    double xftDpiScale = getXftDpiScale();

    qInfo() << "Scale factors:";
    qInfo() << " - PIA:" << piaScale;
    qInfo() << " - gsettings:" << gsettingsScale;
    qInfo() << " - Qt:" << qtScale;
    qInfo() << " - Qt(screen):" << qtScreenScale;
    qInfo() << " - Xft.dpi:" << xftDpiScale;

    // PIA_SCALE overrides all other guesses; this is intended as a workaround
    // override if the guess is incorrect.
    // Note that scale factors < 1 are not allowed for any of these methods;
    // these have known issues with the overlay layer (QQuickPopup is buggy when
    // trying to position itself in the overlay layer; we can work around it for
    // scale >=1 but not scale <1).
    if(piaScale >= 1)
        return piaScale;
    if(gsettingsScale >= 1)
        return gsettingsScale;
    // Prefer the single-valued QT_SCALE_FACTOR over the per-screen values
    // because we cannot actually apply per-screen scale factors right now.  If
    // both were set, this would be the sensible value to apply globally.
    if(qtScale >= 1)
        return qtScale;
    if(qtScreenScale >= 1)
        return qtScreenScale;
    if(xftDpiScale >= 1)
        return xftDpiScale;

    // We didn't find anything.  Default to 1.0.
    return 1.0;
}

void LinuxWindowScaler::preAppInit()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    _qtScaleEnv = env.value(QStringLiteral("QT_SCALE_FACTOR"));
    // Wipe out QT_SCALE_FACTOR so Qt itself won't apply it (it would otherwise
    // apply it even though we've turned off high-DPI scaling).
    qunsetenv("QT_SCALE_FACTOR");
}

void LinuxWindowScaler::initScaleFactor()
{
    // If the scale factor hasn't been found yet, find it now.
    if(_scaleFactor <= 0)
    {
        _scaleFactor = detectScaleFactor();
        // If we detected 1, there was no change from the default 1 used before
        // detection
        if(_scaleFactor != 1)
            emit _scaleFactorChange.signal();
    }
}

LinuxWindowScaler::LinuxWindowScaler(QQuickWindow &window, const QSizeF &logicalSize)
    : NativeWindowScaler{window, logicalSize}, _logicalSize{logicalSize}
{
    // We're not properly detecting DPI changes on Linux right now, so defer the
    // scale factor check until the first window is shown.
    // This is necessary to scale correctly when launched at login on Arch with
    // GNOME (and possibly others).  Arch allows the scale to change during the
    // session, and the client usually starts up before it has applied the
    // initial scaling factor.
    //
    // This doesn't handle later scale changes made by the user, but that's a
    // less frequent occurrence.
    QObject::connect(&window, &QQuickWindow::visibleChanged, this,
        [&window]()
        {
           if(window.isVisible())
               initScaleFactor();  // No effect if the scale factor was already found
        });

    // If the scale factor is found, apply it and emit that as a change in this
    // window's scale factor
    QObject::connect(&_scaleFactorChange, &StaticSignal::signal, this,
        [this]()
        {
            applyWindowSize();
            emit scaleChanged(getScaleFactor());
        });

    applyWindowSize();
}

double LinuxWindowScaler::getScaleFactor()
{
    // If the scale factor hasn't been found yet, act as if it's 1 until we
    // find it.
    if(_scaleFactor <= 0)
        return 1;
    return _scaleFactor;
}

void LinuxWindowScaler::applyWindowSize()
{
    QSize sizeInt = (_logicalSize * getScaleFactor()).toSize();

    // We also need to set the minimum and maximum size.  On Linux, the window
    // manager may understand this and give us a non-sizing border (and remove
    // buttons like maximize).  GNOME, KDE, and LXDE all do this.
    //
    // We have to set the limits first to ensure that we don't attempt to resize
    // outside of the limits we specified, even transiently.  On older Ubuntus
    // (16.04, 16.10), this causes sizing glitches since the resize isn't
    // applied correctly.  We don't need any more smarts than this though, it's
    // OK if the initial limit change causes the window to resize since we
    // resize it explicitly right after.
    //
    // TODO: This needs to be hooked up to a property (and then hooked up by
    // SecondaryWindow) so the Changelog window is resizeable on Linux.
    targetWindow().setMinimumSize(sizeInt);
    targetWindow().setMaximumSize(sizeInt);

    targetWindow().resize(sizeInt);
}

qreal LinuxWindowScaler::applyInitialScale()
{
    return LinuxWindowScaler::getScaleFactor();
}

void LinuxWindowScaler::updateLogicalSize(const QSizeF &logicalSize)
{
    if(logicalSize != _logicalSize)
    {
        _logicalSize = logicalSize;
        applyWindowSize();
    }
}

LinuxWindowMetrics::LinuxWindowMetrics()
{
    // A change in the global scale factor results in a change for the screen
    // scale factors returned by calcScreenScaleFactor().
    QObject::connect(&LinuxWindowScaler::_scaleFactorChange, &StaticSignal::signal,
                     this, &LinuxWindowMetrics::displayChanged);
}

double LinuxWindowMetrics::calcScreenScaleFactor(QScreen &screen) const
{
    return LinuxWindowScaler::getScaleFactor();
}

QMarginsF LinuxWindowMetrics::calcDecorationSize(const QWindow &window, double screenScale) const
{
    // This function needs to return the size of the _screen_ which is occupied
    // by window decorations.  On Linux, this can vary greatly between window
    // managers. GNOME, KDE, XFCE and others use vastly different approaches
    //
    // So for now we will have to provide a hard-coded conservative estimate.
    // We can (to some extent) safely assume that the decorations will have not
    // be larger than 40px on the top and 20px on other sides.
    //
    // Note this is a logical size, so the screen scaling factor will be applied
    // to this size.  Usually this makes sense, since the window decoration
    // probably scales with this factor too.  It may not be ideal though if the
    // user overrides the scale factor with PIA_SCALE, or in general if the
    // window decoration doesn't use the same scale factor as the client.
    return QMarginsF{20, 40, 20, 20};
}
