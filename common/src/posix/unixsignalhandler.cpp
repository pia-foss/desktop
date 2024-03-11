// Copyright (c) 2024 Private Internet Access, Inc.
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

#include "../common.h"
#include "unixsignalhandler.h"
#line SOURCE_FILE("unixsignalhandler.cpp")
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    // These signal handlers are for all the signals that cause us to abort.
    // We want to capture what the signal was in case this happens in the field.
    //
    // Actually doing this is a bit tricky:
    // - Most of these signals indicate that it's not safe to return to the
    //   regular application code (SIGSEGV, SIGFPE, SIGILL, SIGSYS), or that
    //   we'll probably be SIGKILL'ed if we do (SIGXCPU).
    // - The logger is not reentrant, and the application code could have been
    //   in the logger.
    // - We don't capture the heap in core dumps.
    //
    // So we need to get this information in the stack captured by the dump.
    // The easiest thing to see are function names, so if each signal calls a
    // unique function, we can identify the signal that way.  (Here the template
    // parameter becomes part of the function name, rather than writing a bunch
    // of specifically-named functions.)
    //
    // In a stack trace from the dump, you can see the signal here:
    // 3  pia-daemon!void (anonymous namespace)::abortSignalHandler<10>(int, __siginfo*, void*) + 0x9
    //                                                              ^^ signal number
    //
    // The sending PID isn't captured.  There may be some way to get it into a
    // stack variable so it'd be in the dump, but some initial attempts with
    // clang were unsuccessful (even passing a volatile int to a non-inlined
    // function seemed not to actually write the PID to memory).
    template<int Signal>
    void abortSignalHandler(int, siginfo_t *, void*)
    {
        // Just abort.  The signal number is apparent from the name of the
        // function, and this can't be inlined since it was called via a
        // function pointer.
        ::abort();
    }

}

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
    // These signals are ignored, but we still attach a handler to trace them
    setAction(SIGHUP, action);
    setAction(SIGPIPE, action);
    setAction(SIGALRM, action);
    setAction(SIGPROF, action);
    setAction(SIGUSR2, action);
    setAction(SIGVTALRM, action);
    // These signals still cause us to abort, but we'll capture a dump, and the
    // mechanism above gets the signal and process PID in the stack captured by
    // the dump.
    // (Most of these signals would have terminated with no dump by default.)
    setAbortAction<SIGBUS>();
    setAbortAction<SIGFPE>();
    setAbortAction<SIGILL>();
    setAbortAction<SIGQUIT>();
    setAbortAction<SIGSEGV>();
    setAbortAction<SIGSYS>();
    setAbortAction<SIGTRAP>();
    setAbortAction<SIGXCPU>();
    setAbortAction<SIGXFSZ>();
}

UnixSignalHandler::~UnixSignalHandler()
{
    // Ignore signals that we were handling, UnixSignalHandler is being
    // destroyed so our signal handler would no longer work
    ::signal(SIGUSR1, SIG_IGN);
    ::signal(SIGTERM, SIG_IGN);
    ::signal(SIGINT, SIG_IGN);
    ::signal(SIGHUP, SIG_IGN);
    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGALRM, SIG_IGN);
    ::signal(SIGPROF, SIG_IGN);
    ::signal(SIGUSR2, SIG_IGN);
    ::signal(SIGVTALRM, SIG_IGN);
    // The 'abort' signals are left alone, these can still be handled, they
    // don't directly involve UnixSignalHandler
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
template<int Signal>
void UnixSignalHandler::setAbortAction()
{
    struct sigaction action{};
    action.sa_sigaction = &abortSignalHandler<Signal>;
    action.sa_flags = SA_RESTART | SA_SIGINFO;
    setAction(Signal, action);
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
    default:
        // Ignored
        break;
    }
}
