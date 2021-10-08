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
#line HEADER_FILE("linux_scaler.h")

#ifndef LINUX_SCALER_H
#define LINUX_SCALER_H

#include <QObject>
#include "windowmaxsize.h"
#include "nativewindowscaler.h"
#include "staticsignal.h"
#include <QProcessEnvironment>

class LinuxWindowScaler : public NativeWindowScaler
{
    CLASS_LOGGING_CATEGORY("linuxscaler")

private:
    // The QT_SCALE_FACTOR environment variable has to be wiped out before
    // creating the QGuiApplication, because Qt still brilliantly applies this
    // factor even though we've explicitly turned off DPI scaling.  The value is
    // stored here so it can be used when we calculate the scale factor later.
    static QString _qtScaleEnv;
    // Store the computed value of the scale factor. 0 if the scale factor
    // hasn't been found yet.
    static double _scaleFactor;
    // Change signal emitted when the scale factor is first computed.
    static StaticSignal _scaleFactorChange;

    // LinuxWindowMetrics connects to the _scaleFactorChange signal to observe
    // changes in the scale factor.
    friend class LinuxWindowMetrics;

private:
    static double checkScaleEnvVar(const QProcessEnvironment &env,
                                   const QString &envVarName);
    // Parse an environment variable that might contain a scale factor (as a
    // double).  Returns the value if it's found and parsed successfully, or
    // 0.0 otherwise.  (Used by detectScaleFactor(), prints diagnostics if the
    // scale factor will be set by this environment variable.)
    static double parseScaleEnvVar(const QString &scaleFactorStr,
                                   const QString &envVarName);
    // Run a command used to detect a scale factor.  Returns the command's
    // output (with whitespace trimmed), or "" if it didn't exist, failed, etc.
    // Prints diagnostics for various failure modes.
    static QString runCommand(const QString &command, const QStringList &args);
    // Detect scale factors using various methods
    static double getPiaScale(const QProcessEnvironment &env);
    static double getGsettingsScale();
    static double getQtScale(const QProcessEnvironment &env);
    static double getQtScreenScale(const QProcessEnvironment &env);
    static double getXftDpiScale();
    // Check various settings and environment variables to determine the scale
    // factor.
    static double detectScaleFactor();

public:
    // Do initialization that must occur before the QGuiApplication is created.
    // Called by LinuxEnv::preAppInit().
    static void preAppInit();
    // Initialize the scale factor if it hasn't been intialized yet.
    // We have to rely on the windows and tray icon to tell us when they really
    // need the scale factor.  We can't just detect visibility changes in the
    // window, because the window has already been positioned based on its size
    // at that point, which needs the correct scale factor to work correctly.
    // (Detecting changes in the X11 DPI setting correctly would eliminate this
    // workaround.)
    static void initScaleFactor();
    // Get the scale factor read from the environment
    static double getScaleFactor();

public:
    LinuxWindowScaler(QQuickWindow &window, const QSizeF &logicalSize);

private:
    void applyWindowSize();

public:
    virtual qreal applyInitialScale() override;
    virtual void updateLogicalSize(const QSizeF &logicalSize) override;

private:
    QSizeF _logicalSize;
};

class LinuxWindowMetrics: public NativeWindowMetrics
{
    Q_OBJECT

public:
    LinuxWindowMetrics();

    // NativeWindowMetrics interface
public:
    double calcScreenScaleFactor(const PlatformScreens::Screen &) const override;
    QMarginsF calcDecorationSize(const QWindow &window, double screenScale) const override;
};

#endif // LINUXWINDOWMETRICS_H
