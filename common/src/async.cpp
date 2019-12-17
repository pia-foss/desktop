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
#line SOURCE_FILE("async.cpp")

#include "async.h"

#include <QMutex>

// If we guarantee that all Tasks will be owned/managed by the main thread, this mutex is unnecessary
static QMutex g_taskMutex;
static NodeList<BaseTask> g_taskList;
static uint g_taskIndex = 0;

static inline const QMetaMethod& taskFinishedSignal()
{
    static QMetaMethod signal = QMetaMethod::fromSignal(&BaseTask::finished);
    return signal;
}

BaseTask::BaseTask(QObject *parent)
    : QObject(parent), _error(HERE, Error::TaskStillPending)
{
    QMutexLocker lock(&g_taskMutex);
    insertLast(&g_taskList);
    _state = ++g_taskIndex & IndexMask;
}

BaseTask::~BaseTask()
{
    // Unlink us from the task list while holding the task lock.
    QMutexLocker lock(&g_taskMutex);
    remove();
}

// Called from derived class destructor to do checks while
// virtual functions are still valid
void BaseTask::destructorCheck()
{
    uint state = _state;
    // Check if we're being destroyed before resolving or rejecting, in which
    // case we may want to write a warning to the log.
    if (state < Resolved)
    {
        if (!(state & Connected))
        {
            // If no one has ever connected to listen to the task's result,
            // this is likely a mistake; you should probably be holding a
            // reference to the final task in the chain, or you should call
            // runUntilFinished() or notify() on it.
            qWarning() << this << "abandoned";
        }
        // Qt < 5.12.1 has a bug where isSignalConnected keeps returning true
        // for low (index < 64) signals even after they've been disconnected,
        // due to a cache that's not updated.  The tracker indicates that this
        // is fixed in 5.12.1, but keep this workaround until we've actually
        // tested it.
        // https://bugreports.qt.io/browse/QTBUG-32340
        else if (isSignalConnected(taskFinishedSignal())
                 && receivers(SIGNAL(finished())) > 0)
        {
            // If someone is *still* listening for the task's result at this
            // point, there's an error in lifetime management somewhere.
            // If you are handling instances of Task<T> yourself, consider
            // refactoring to use chains of Async<T> instead.
            qError() << this << "destroyed while running";

            // Try to send a rejection error as there might still be time
            // to handle it if the child is in the same thread, but any
            // child listening for this is holding a non reference counted
            // pointer to us, which is dangerous and should be fixed.
            reject(Error(HERE, Error::TaskDestroyedWhilePending));
        }
    }
}

void BaseTask::disconnectDependents()
{
    disconnect(this, &BaseTask::finished, nullptr, nullptr);
    // Note: At this point, we may have been destroyed as a result of
    // the last reference being released. This is why it's important
    // that tasks are only manipulated while holding strong references.
}

void BaseTask::resetTaskIndex()
{
    QMutexLocker lock(&g_taskMutex);
    g_taskIndex = 0;
}

int BaseTask::getTaskCount()
{
    QMutexLocker lock(&g_taskMutex);
    return g_taskList.count();
}

void BaseTask::rejectAllTasks(const Error& error, bool synchronous)
{
    if (synchronous)
    {
        // Grab a copy of the list as it exists now with QSharedPointer
        // references to temporarily keep them alive
        QVector<QSharedPointer<BaseTask>> tasks;
        {
            QMutexLocker lock(&g_taskMutex);
            tasks.reserve(g_taskList.count());
            for (BaseTask* task = g_taskList.first(); task; task = task->next())
            {
                tasks.append(task->sharedFromThis());
            }
        }
        // Reject every item in the list
        for (const auto& task : tasks)
        {
            if (!task->isFinished())
                task->reject(error);
        }
    }
    else
    {
        // Schedule asynchonous reject calls for each task
        QMutexLocker lock(&g_taskMutex);
        for (BaseTask* task = g_taskList.first(); task; task = task->next())
        {
            QMetaObject::invokeMethod(task, [=] { auto keepalive = task->sharedFromThis(); if (task->isPending()) task->reject(error); }, Qt::QueuedConnection);
        }
    }
}

void BaseTask::rejectAllTasks(bool synchronous)
{
    rejectAllTasks(Error(HERE, Error::TaskRejected), synchronous);
}

void BaseTask::reject(Error error)
{
    if (setFinished(Rejected))
    {
        _error = std::move(error);
        notifyRejected();
        emit finished();
        // This must be called last in the function
        disconnectDependents();
    }
}

void BaseTask::reject()
{
    reject(Error(HERE, Error::TaskRejected));
}

void BaseTask::runUntilFinished(QObject* context)
{
    if (!isFinished())
    {
        // Add a listener holding a reference to ourselves. This will get
        // disconnected after finishing, and the reference will be released.
        connect(this, &BaseTask::finished, context, [self = sharedFromThis()] {});
    }
}

void BaseTask::runUntilFinished()
{
    runUntilFinished(this);
}

void BaseTask::abandon()
{
    // Setting the Connected flag suppresses the abandoned warning.
    setConnected();
}

const char* BaseTask::typeName() const
{
    return nullptr;
}

void BaseTask::connectNotify(const QMetaMethod& signal)
{
    if (signal == taskFinishedSignal())
    {
        setConnected();
    }
}

bool BaseTask::setConnected()
{
    // Compare-exchange with spinlock to set state flag.
    uint old = _state.load(), value;
    do
    {
        if (old & Connected)
            return false;
        value = old | Connected;
    } while (!_state.compare_exchange_weak(old, value));
    return true;
}

bool BaseTask::setFinished(BaseTask::State state)
{
    // Compare-exchange with spinlock to set state flag.
    uint old = _state.load(), value;
    do
    {
        if (old >= Resolved)
        {
            qWarning() << "Tried to" << (state == Resolved ? "resolve" : "reject") << "already" << (old & Resolved ? "resolved" : "rejected") << this;
            return false;
        }
        value = old | state;
    } while (!_state.compare_exchange_weak(old, value));
    return true;
}

QDebug operator<<(QDebug debug, const BaseTask* task)
{
    // Note: Extra debugging stuff like state and listener count can be added
    // here if debugging task lifetimes etc.
    QDebugStateSaver saver(debug);
    debug.nospace().noquote() << "Task";
    if (const char* type = task->typeName())
    {
        debug << '<' << type << '>';
    }
    debug << ' ' << '#' << (task->_state & BaseTask::IndexMask);
    //debug << ' ' << '(' << qEnumToString(task->_state) << ')';
    return debug;
}

Task<void>::~Task()
{
    destructorCheck();
}

void Task<void>::resolve()
{
    if (setFinished(Resolved))
    {
        _error = Error(HERE, Error::Success);
        emit finished();
        // This must be called last in the function
        disconnectDependents();
    }
}

const char* Task<void>::typeName() const
{
    return "void";
}
