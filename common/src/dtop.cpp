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

#include "dtop.h"
#include "common.h"
#include <kapps_core/src/eventloop.h>
#include <kapps_core/src/timer.h>
#include <unordered_map>
#include <QSocketNotifier>

namespace
{
    // Implement EventLoop using the Qt event loop
    class EventLoopQt : public QObject, public kapps::core::EventLoop
    {
    private:
        struct RWNotifiers
        {
        public:
            RWNotifiers(int fd)
                : _r{fd, QSocketNotifier::Type::Read},
                  _w{fd, QSocketNotifier::Type::Write}
            {
                _r.setEnabled(false);
                _w.setEnabled(false);
            }

            QSocketNotifier &forType(WatchType t)
            {
                return (t == WatchType::Write) ? _w : _r;
            }

        public:
            QSocketNotifier _r, _w;
        };

    private:
        // Watch tokens indicate the file descriptor number and whether it's the
        // read or write watch (low bit)
        TokenT makeFdToken(int fd, WatchType type) const
        {
            TokenT token{fd};
            token <<= 1;
            if(type == WatchType::Write)
                token |= 1;
            return token;
        }
        int breakFdToken(TokenT token, WatchType &type) const
        {
            type = (token & 1) ? WatchType::Write : WatchType::Read;
            return token >> 1;
        }

        // Implementation of EventLoop
        virtual TokenT setTimer(std::chrono::milliseconds interval, bool single) override;
        virtual void cancelTimer(TokenT token) override;
#if defined(Q_OS_UNIX)
        virtual TokenT watchFd(int fd, WatchType type) override;
        virtual void cancelFdWatch(TokenT token) override;
#endif

        // QObject overrides
        virtual void timerEvent(QTimerEvent *pEvent) override;

        // Connected to QSocketNotifier signals
        void socketActivated(QSocketDescriptor socket, QSocketNotifier::Type type);

    private:
        // Keep track of which timers are ours (though in theory there probably
        // shouldn't be any other timers scheduled on this QObject), and whether
        // they are single-shot so we know to cancel them automatically.
        std::unordered_map<TokenT, bool> _timers;
#if defined(Q_OS_UNIX)
        // Each watched file descriptor has up to two QSocketNotifiers - one for
        // read events and one for write.  If only one type of events is
        // watched, only one is activated.
        std::unordered_map<int, RWNotifiers> _fds;
#endif
    };

    auto EventLoopQt::setTimer(std::chrono::milliseconds interval, bool single)
        -> TokenT
    {
        int id = startTimer(msec(interval));
        // QObject::startTimer() uses 0 for failure
        if(id <= 0)
        {
            KAPPS_CORE_WARNING() << "QObject::startTimer() failed to set timer with interval"
                << traceMsec(interval) << "and single=" << single;
            return InvalidToken;
        }

        _timers[id] = single;
        return id;
    }

    void EventLoopQt::cancelTimer(TokenT token)
    {
        auto itExistingTimer = _timers.find(token);
        // This shouldn't happen; would indicate a bug in kapps::core::Timer
        if(itExistingTimer == _timers.end())
        {
            KAPPS_CORE_WARNING() << "cancelTimer() called for timer" << token
                << "that was not ours";
            return;
        }

        _timers.erase(itExistingTimer);
        killTimer(token);
    }

#if defined(KAPPS_CORE_OS_POSIX)
    auto EventLoopQt::watchFd(int fd, WatchType type) -> TokenT
    {
        auto itFd = _fds.find(fd);
        if(itFd == _fds.end())
        {
            // Didn't have this file descriptor, add it and activate this
            // notifier
            auto &notifiers = _fds.emplace(fd, fd).first->second;
            connect(&notifiers._r, &QSocketNotifier::activated,
                this, &EventLoopQt::socketActivated);
            connect(&notifiers._w, &QSocketNotifier::activated,
                this, &EventLoopQt::socketActivated);
            notifiers.forType(type).setEnabled(true);
            return makeFdToken(fd, type);
        }

        // Otherwise, we do have at least one notifier for this type.  Is the
        // watch type requested already active?
        auto &notifier{itFd->second.forType(type)};
        if(notifier.isEnabled())
        {
            qWarning() << "Already have active watch for fd" << fd << "and type"
                << type;
            return InvalidToken;
        }

        // Just activate the watch
        notifier.setEnabled(true);
        return makeFdToken(fd, type);
    }

    void EventLoopQt::cancelFdWatch(int token)
    {
        WatchType type;
        int fd{breakFdToken(token, type)};

        auto itFd = _fds.find(fd);
        if(itFd == _fds.end())
        {
            qWarning() << "Tried to cancel watch for fd" << fd
                << "which is not active";
            return;
        }

        auto &notifier{itFd->second.forType(type)};
        if(!notifier.isEnabled())
        {
            qWarning() << "Tried to cancel watch for fd" << fd
                << "of type" << type << "which is not active";
            return;
        }

        // Disable it
        notifier.setEnabled(false);

        // If both are disabled, discard this entry
        if(!itFd->second._r.isEnabled() && !itFd->second._w.isEnabled())
            _fds.erase(itFd);
    }
#endif

    void EventLoopQt::timerEvent(QTimerEvent *pEvent)
    {
        if(!pEvent)
        {
            QObject::timerEvent(pEvent);
            return;
        }

        TokenT token = pEvent->timerId();
        auto itExistingTimer = _timers.find(token);
        // This probably won't happen, but it could happen if for some reason
        // QObject scheduled its own timer, etc.
        if(itExistingTimer == _timers.end())
        {
            QObject::timerEvent(pEvent);
            return;
        }

        // If it's a single-shot timer, cancel it.  This is essentially what
        // QTimer would do.
        if(itExistingTimer->second)
        {
            cancelTimer(token);
            // This invalidates itExistingTimer
        }

        timerElapsed(token);
    }

#if defined(KAPPS_CORE_OS_POSIX)
    void EventLoopQt::socketActivated(QSocketDescriptor socket, QSocketNotifier::Type type)
    {
        WatchType watchType{(type == QSocketNotifier::Type::Read) ? WatchType::Read : WatchType::Write};
        fdWatchTriggered(makeFdToken(socket, watchType));
    }
#endif
}

void initKApps()
{
    kapps::core::EventLoop::setThreadEventLoop(
        std::unique_ptr<kapps::core::EventLoop>{new EventLoopQt{}});
}
