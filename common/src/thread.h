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

#include "common.h"
#line HEADER_FILE("thread.h")

#ifndef THREAD_H
#define THREAD_H

#include <QObject>
#include <QThread>

// RunningWorkerThread is a thread with a Qt event loop that is started and
// stopped automatically. The caller can invoke a functor on the thread
// synchronously by calling invokeOnThread(), or asynchronously by calling
// queueOnThread().
class COMMON_EXPORT RunningWorkerThread : public QObject
{
    Q_OBJECT

public:
    RunningWorkerThread();
    ~RunningWorkerThread();

public:
    // Invoke a functor synchronously on the worker thread.
    template<class Func>
    void invokeOnThread(Func f)
    {
        Q_ASSERT(pThreadObject);    // Class invariant
        QMetaObject::invokeMethod(pThreadObject, std::move(f),
                                  Qt::ConnectionType::BlockingQueuedConnection);
        // Note that pThreadObject is deleted in the destructor using
        // invokeOnThread(), so it may not be valid at this point.
    }

    // Queue a functor to be invoked asynchronously on the worker thread.
    // Be careful with the state captured by the functor
    // - it must remain valid until either the functor completes or the thread
    //   is shut down (there is no way to "cancel" the functor)
    // - if any other thread might also access that state, synchronization is
    //   the caller's responsibiility
    template<class Func>
    void queueOnThread(Func f)
    {
        Q_ASSERT(pThreadObject);    // Class invariant
        QMetaObject::invokeMethod(pThreadObject, std::move(f),
                                  Qt::ConnectionType::QueuedConnection);
    }

    // Get a QObject that can be used to own objects created on the worker
    // thread.
    // If objects are created on the worker thread, they must be destroyed
    // before that thread exits.  By parenting them to objectOwner(), which
    // lives on the worker thread, they will be destroyed when this object is
    // destroyed just before the thread exits.
    QObject &objectOwner();

private:
    // This is the actual worker thread
    QThread _thread;
    // This QObject lives on the worker thread.  Objects created in calls to
    // invokeOnThread() can be parented to it, so they'll be destroyed before
    // the thread exits.
    // We can't use an owning pointer for this, because we can't directly delete
    // the object from RunningWorkerThread's thread, it has to be deleted from
    // the actual worker thread.
    QObject *pThreadObject;
};

#endif
