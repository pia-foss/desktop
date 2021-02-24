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
#include "unixsignalhandler.h"
#line SOURCE_FILE("unixsignalhandler.cpp")
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <QDebug>

UnixSignalHandler::UnixSignalHandler(QObject *parent)
    : QObject{parent}, _rxBytes{0}
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, _sigFd)) {
        qCritical() << "Couldn't create SIGUSR1 socketpair";
        return;
    }
    _snUsr1 = new QSocketNotifier(_sigFd[1], QSocketNotifier::Read, this);
    connect(_snUsr1, &QSocketNotifier::activated, this, &UnixSignalHandler::handleSignal);

    struct sigaction action{};

    action.sa_sigaction = &UnixSignalHandler::_signalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    setAction(SIGUSR1, action);
    setAction(SIGTERM, action);
    setAction(SIGINT, action);
    // SIGHUP is ignored, but we still attach a handler to trace it
    setAction(SIGHUP, action);
}

UnixSignalHandler::~UnixSignalHandler()
{
    ::signal(SIGUSR1, SIG_IGN);
    ::signal(SIGTERM, SIG_IGN);
    ::signal(SIGINT, SIG_IGN);
    ::signal(SIGHUP, SIG_IGN);
    ::close(_sigFd[0]);
    ::close(_sigFd[1]);
}

void UnixSignalHandler::_signalHandler(int, siginfo_t *info, void *)
{
    Q_ASSERT(info);

    // Preserve errno; write() may overwrite it.
    auto restoreErrno = raii_sentinel([savedErrno = errno](){errno = savedErrno;});

    // ::write() could theoretically fail, but there's nothing we could do even if
    // we checked it, we can't even log because the logger is not reentrant.
    auto pThis = instance();
    if(pThis)
        ::write(pThis->_sigFd[0], info, sizeof(siginfo_t));
}

void UnixSignalHandler::setAction(int signal, const struct sigaction &action)
{
    if (sigaction(signal, &action, nullptr))
    {
        qDebug () << "Unable to init handler for" << signal;
    }
}

int UnixSignalHandler::sendSignalUsr1(qint64 pid)
{
  qDebug () << "Sending signal SIGUSR1 to " << pid;
  return kill(static_cast<int>(pid),SIGUSR1);
}

void UnixSignalHandler::handleSignal(int socket)
{
    Q_UNUSED(socket);

    unsigned char *pWritePos = reinterpret_cast<unsigned char*>(&_rxInfo);
    pWritePos += _rxBytes;
    // There's no need to loop ::read() here because Qt polls the socket in its
    // event loop; if there is another signal buffered already it will activate
    // the socket again.
    ssize_t readResult = ::read(_sigFd[1], pWritePos, sizeof(_rxInfo)-_rxBytes);
    if(readResult < 0)
    {
        qWarning() << "Signal socket read failed:" << readResult << "-" << errno;
        return;
    }

    _rxBytes += readResult;
    if(_rxBytes < sizeof(_rxInfo))
    {
        qInfo() << "Received incomplete signal info -" << _rxBytes << "/"
            << sizeof(_rxInfo) << "- waiting for complete signal info";
        return;
    }

    // Reset _rxBytes so we start from the beginning of the siginfo next time
    _rxBytes = 0;

    qInfo() << "Received signal" << _rxInfo.si_signo << "from pid"
        << _rxInfo.si_pid;

    emit signal(_rxInfo.si_signo);
    switch(_rxInfo.si_signo)
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
