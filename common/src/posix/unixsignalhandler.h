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
#ifndef UNIXSIGNALHANDLER_H
#define UNIXSIGNALHANDLER_H
#line HEADER_FILE("unixsignalhandler.h")

#include <QObject>
#include <QSocketNotifier>


class COMMON_EXPORT UnixSignalHandler: public QObject
{
    Q_OBJECT

public:
  explicit UnixSignalHandler(QObject *parent = nullptr);

private:
  // called by the system when a signal is received
  static void _signalHandler(int signal);

  static void setAction(int signal, const struct sigaction &action);

public:
  // set up system level callbacks. Call once in the startup function
  static void init();

  // Send the SIGUSR1 signal to the process
  static int sendSignalUsr1(qint64 pid);

signals:
  void sigUsr1();
  void sigInt();
  void sigTerm();

public slots:
  // callback from a socket. Used to push the event to qt-land
  void handleSignal(int socket);

private:
  QSocketNotifier *_snUsr1;
  static int sigUsr1Fd[2];
};

#endif // UNIXSIGNALHANDLER_H
