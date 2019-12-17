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
#include "unixsignalhandler.h"
#line SOURCE_FILE("unixsignalhandler.cpp")
#include<signal.h>
#include<sys/socket.h>
#include <unistd.h>
#include <QDebug>
int UnixSignalHandler::sigUsr1Fd[2];

UnixSignalHandler::UnixSignalHandler(QObject *parent) : QObject (parent)
{
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigUsr1Fd)) {
     qCritical() << "Couldn't create SIGUSR1 socketpair";
     return;
  }
  _snUsr1 = new QSocketNotifier(sigUsr1Fd[1], QSocketNotifier::Read, this);
  connect(_snUsr1, &QSocketNotifier::activated, this, &UnixSignalHandler::handleSignal);
}

void UnixSignalHandler::_signalHandler(int signal)
{
  Q_ASSERT(signal <= std::numeric_limits<unsigned char>::max());

  // Preserve errno; write() may overwrite it.
  auto restoreErrno = raii_sentinel([savedErrno = errno](){errno = savedErrno;});

  unsigned char a = static_cast<unsigned char>(signal);
  // ::write() could theoretically fail, but there's nothing we could do even if
  // we checked it, we can't even log because the logger is not reentrant.
  ::write(sigUsr1Fd[0], &a, sizeof(a));
}

void UnixSignalHandler::setAction(int signal, const struct sigaction &action)
{
  // The signal number is written over the socket as a byte; this is fine
  // because all handled signal IDs fit in a byte.
  Q_ASSERT(signal <= std::numeric_limits<unsigned char>::max());

  if (sigaction(signal, &action, nullptr)) {
    qDebug () << "Unable to init handler for" << signal;
  }
}

void UnixSignalHandler::init()
{
  struct sigaction action{};

  action.sa_handler = UnixSignalHandler::_signalHandler;
  action.sa_flags = SA_RESTART;

  setAction(SIGUSR1, action);
  setAction(SIGTERM, action);
  setAction(SIGINT, action);
  // SIGHUP is ignored, but we still attach a handler to trace it
  setAction(SIGHUP, action);
}

int UnixSignalHandler::sendSignalUsr1(qint64 pid)
{
  qDebug () << "Sending signal SIGUSR1 to " << pid;
  return kill(static_cast<int>(pid),SIGUSR1);
}

void UnixSignalHandler::handleSignal(int socket)
{
  Q_UNUSED(socket);

  // There's no need to loop ::read() here because Qt polls the socket in its
  // event loop; if there is another signal buffered already it will activate
  // the socket again.
  unsigned char signal;
  ssize_t readResult = ::read(sigUsr1Fd[1], &signal, sizeof(signal));
  if(readResult < static_cast<ssize_t>(sizeof(signal)))
  {
    qInfo() << "Signal socket woke up but returned" << readResult;
    return;
  }

  qInfo() << "Received signal" << signal;
  switch(signal)
  {
  case SIGUSR1:
    emit sigUsr1();
    break;
  case SIGINT:
    emit sigInt();
    break;
  case SIGTERM:
    emit sigTerm();
    break;
  case SIGHUP:
    // Ignored
    break;
  }
}
