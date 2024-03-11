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

#ifndef ASYNC_H
#define ASYNC_H
#pragma once

#include "common.h"
#include "nodelist.h"

#include <QObject>
#include <QSharedPointer>
#include <QThread>
#include <QPointer>
#include <QTimer>

#include <atomic>

/*


=== Asynchonous task management ===

The classes in this file provide a way to represent operations that complete
asynchronously in a fashion similar to promises. The main go-to type for this
is Async<T>, representing an operation that eventually finishes with a return
value of T (can be void for operations that finish without a return value).

Async<T> can be used as a function return value, a function parameter, or as
any other by-value variable. Behind the scenes, it is a shared pointer that
refers to a Task<T> instance.

Tasks finish by either resolving (with a return value for non-void tasks) or
by rejecting with an Error (Error::TaskRejected by default). Once a task
finishes, it emits the finished signal, after which you can query for its
result with isResolved() and result() or isRejected() and error(). Note that
while the error() method is always valid, the result() method can only be
safely used if the task has been resolved.


--- Chaining ---

Rather than manually connecting signals and inspecting results though, the
more convenient way to use tasks is by chaining them. Every task supports
the following chaining methods (where T is the type of the task):

  Async<R> then([context,] R func(const T&))
  Async<T> except([context,] T func(const Error&))
  Async<R> next([context,] R func(const Error&, const T&))

  void notify([recipient,] void func(const Error&, const T&))

The 'then' function attaches a function that gets called with the result
of the current task (if it resolves), and in turn represents the result
of that function with a new returned task. For void tasks, no result
parameter is passed.

The 'except' function similarly attaches a function that gets called with
the error of the current task (if it rejects), and represents the result
of that function with a new returned task. The supplied function return
value must be compatible with the original task type.

The 'next' function is a combination of 'then' and 'except' and attaches
a function that gets called with either the result or error of the current
task depending on whether it is resolved or rejected. If the task is
resolved, the function gets called with an empty Error (with the code
Error::Success) and the result; if the task is rejected, the function is
called with the rejected Error and a default-constructed result. For void
tasks, no result parameter is passed. The result of the supplied function
is represented in a new task returned by the 'then' function.

Lastly, the 'notify' function merely attaches a callback function with the
same signature as for the 'next' function, but doesn't return any new task.
This is used internally by the other chaining functions, but is also useful
as a last step in any task chain, delivering the result to a final callback.
The task is kept alive until it either finishes, or until the recipient is
destroyed (if you passed a recipient).

In addition, there are two aggregate tasks, 'race' and 'all', exposed as
static functions on Async<T> and AutoAsync. These take a collection of tasks
as input and return a combined result. With 'race', the returned task takes
on the result or error of whichever input task finishes first. With 'all',
the returned task resolves to a list of the results of all input tasks once
they all resolve, or rejects with the error of the first rejected task.

Notes on chaining tasks:

- If you throw an exception in any chained callback (except 'notify') it
  will be caught and used to reject the chained task.

- There is no way to unchain two tasks. This is especially true for tasks
  chained via 'chain', as the chained callback belongs to the task itself.

- You are not supposed to resolve or reject tasks yourself in the middle
  of a chain.


--- Handling tasks ---

It is an error not to use a created/returned Task; the compiler will raise
an error if it detects this, otherwise you'll get runtime warnings. This is
because neglecting to keep a task alive until it finishes is almost always
a mistake. For every task, you should do one of the following:

- Chain it to a new task (e.g. via then/except/next/race/all), in which
  case you're then responsible for the resulting task instead.

- Instruct the task to deliver its result or error to a callback via the
  'notify' function.

- If you actually want to abandon a task and not care about its result or
  whether it finishes or not, call the 'abandon' function.


--- Lifetime management ---

As a general rule, tasks hold strong references to their dependencies and
weak references to their dependents. Qt's signal system is used to invoke
callbacks in the dependent's context, while holding a strong reference to
the parent task as a lambda capture. Once a task finishes, it disconnects
all its listeners, which, if the listener was the only thing holding a
reference to the parent, properly disposes of the parent when it is no
longer needed.


--- Running tasks without keeping a reference ---

Sometimes there is a need to keep running a task to its completion without
having to keep a reference to it. There are two ways to achieve this:

You can use the 'notify' function to run a task chain and deliver its result
to a recipient QObject and slot/callback.

If you don't care about the actual result and just want the task to run
until completion, you can use the 'runUntilFinished' function, which behaves
like 'notify' but with an empty callback.

In both cases a signal connection between the task and a recipient object
is what keeps the task alive, so if the recipient is destroyed the task will
be abandoned as well. While the recipient/context argument is optional, you
should always pass it so the task doesn't outlive its recipient.

Lastly, there is a nuclear option provided via the 'rejectAllTasks' function
which will reject all currently living tasks, but this should only ever be
used in very specific circumstances such as during shutdown.


--- Custom tasks ---

While many asynchronous tasks can be expressed relatively elegantly with
chained tasks, sometimes a task is too complicated (e.g. recursion) and
would be better implemented differently. For these cases, you can instead
subclass Task<T> and attach whatever additional state and logic you want,
calling resolve or reject yourself once finished, with your class being
consumable by others as a normal task.

Whenever you implement tasks, try to follow the same lifetime rules with
dependences holding strong references to dependencies, and having tasks
be cleanly abandonable/disposable "from below" if possible. One way to
accomplish this is by using lambdas as signal handlers and capturing
strong or weak references to dependencies and dependents. Note that while
"downward" signals are done using weak pointers, those pointers should
always be promoted to strong references while calling task functions,
or the task can expire unexpectedly. (The 'notify' function does this for
you if the recipient is a Task object.)


*/


class COMMON_EXPORT BaseTask;
template<typename Result> class Task;
template<typename Type, typename Result, class Class> class Async;
template<typename Result> class AsyncFunctions;

namespace impl {

    template<class Class, typename Func, typename A1, typename A2, typename... Extra>
    inline auto invoke(Class* ptr, Func Class::* fn, A1&& a1, A2&& a2, Extra&&...) -> decltype((ptr->*fn)(std::forward<A1>(a1), std::forward<A2>(a2)))
    {
        return (ptr->*fn)(std::forward<A1>(a1), std::forward<A2>(a2));
    }
    template<class Class, typename Func, typename A1, typename... Extra>
    inline auto invoke(Class* ptr, Func Class::* fn, A1&& a1, Extra&&...) -> decltype((ptr->*fn)(std::forward<A1>(a1)))
    {
        return (ptr->*fn)(std::forward<A1>(a1));
    }
    template<class Class, typename Func, typename... Extra>
    inline auto invoke(Class* ptr, Func Class::* fn, Extra&&...) -> decltype((ptr->*fn)())
    {
        return (ptr->*fn)();
    }
    template<typename Func, typename A1, typename A2, typename... Extra>
    inline auto invoke(QObject*, Func& fn, A1&& a1, A2&& a2, Extra&&...) -> decltype(fn(std::forward<A1>(a1), std::forward<A2>(a2)))
    {
        return fn(std::forward<A1>(a1), std::forward<A2>(a2));
    }
    template<typename Func, typename A1, typename... Extra>
    inline auto invoke(QObject*, Func& fn, A1&& a1, Extra&&...) -> decltype(fn(std::forward<A1>(a1)))
    {
        return fn(std::forward<A1>(a1));
    }
    template<typename Func, typename... Extra>
    inline auto invoke(QObject*, Func& fn, Extra&&...) -> decltype(fn())
    {
        return fn();
    }

    template<typename T> struct QVectorOrVoid { typedef QVector<T> type; };
    template<> struct QVectorOrVoid<void> { typedef void type; };

    template<class...> using void_t = void;

    template<typename, typename... Args>
    struct CommonTypeOrVoid { typedef void type; };
    template<typename... Args> struct CommonTypeOrVoid<void_t<std::common_type_t<const Args&...>>, Args...> { typedef std::common_type_t<const Args&...> type; };

    template<typename T> struct AsyncResultType {
        template<typename U> static auto call(Task<U>*) -> U;
        static T call(...);
        typedef decltype(call(std::declval<T*>())) type;
    };
    template<typename T, typename R, class C> struct AsyncResultType<Async<T, R, C>> { typedef R type; };

    template<typename T> struct AsyncClassType {
        template<typename U> static auto call(Task<U>*) -> T;
        static Task<T> call(...);
        typedef decltype(call(std::declval<T*>())) type;
    };
    template<typename T, typename R, class C> struct AsyncClassType<Async<T, R, C>> { typedef C type; };

    template<typename T> struct IsAsync : public std::false_type {};
    template<typename T, typename R, class C> struct IsAsync<Async<T, R, C>> : public std::true_type {};

    template<class Receiver, typename... InvokeArgs, typename ReturnType = decltype(impl::invoke(std::declval<InvokeArgs>()...))>
    inline std::enable_if_t<std::is_void<typename Receiver::ResultType>::value && !IsAsync<ReturnType>::value> resolveWith(Receiver* task, InvokeArgs&&... args) { invoke(std::forward<InvokeArgs>(args)...); task->resolve(); }
    template<class Receiver, typename... InvokeArgs, typename ReturnType = decltype(impl::invoke(std::declval<InvokeArgs>()...))>
    inline std::enable_if_t<!std::is_void<typename Receiver::ResultType>::value || IsAsync<ReturnType>::value> resolveWith(Receiver* task, InvokeArgs&&... args) { task->resolve(invoke(std::forward<InvokeArgs>(args)...)); }

    template<typename T> inline auto KeepAliveIfNeeded(T* ptr) -> decltype(ptr->sharedFromThis()) { return ptr->sharedFromThis(); }
    template<typename T> inline bool KeepAliveIfNeeded(const QSharedPointer<T>&) { return false; } // No need to keep alive
    inline bool KeepAliveIfNeeded(QObject*) { return false; } // Not possible to keep alive

    template<typename T> auto QPointerIfNeeded(T* ptr) -> std::enable_if_t< std::is_base_of<BaseTask, T>::value, T*> { return ptr; }
    template<typename T> auto QPointerIfNeeded(T* ptr) -> std::enable_if_t<!std::is_base_of<BaseTask, T>::value, QPointer<T>> { return ptr; }

    template<typename T> T* getNakedPointer(const QSharedPointer<T>& ptr) { return ptr.get(); }
    template<typename T> T* getNakedPointer(const QPointer<T>& ptr) { return ptr.data(); }
    template<typename T> T* getNakedPointer(T* ptr) { return ptr; }

}

// T -> QVector<T> but void -> void
template<typename T> using QVectorOrVoid = typename impl::QVectorOrVoid<T>::type;
// T... -> U if U can be deduced so that every T is convertible to U, otherwise T... -> void
template<typename... Args> using CommonTypeOrVoid = typename impl::CommonTypeOrVoid<void, Args...>::type;
// T -> T::ResultType if T is a Task, otherwise T -> T
template<typename T> using AsyncResultType = typename impl::AsyncResultType<std::decay_t<T>>::type;
// T -> T if T is a Task, otherwise T -> Task<T>
template<typename T> using AsyncClassType = typename impl::AsyncClassType<std::decay_t<T>>::type;



template<typename Type, typename Result = AsyncResultType<Type>, class Class = AsyncClassType<Type>> class Async;


// Implements a Promise-like notion, representing some asynchronous task
// that runs somewhere until it completes (resolves) or fails (rejects).
//
// The BaseTask is merely a base class for implementing common functionality
// in a QObject. The class that should actually be used is Task<T> via its
// wrapper, Async<T>. Use Async<void> if you do not need a result.
//
// NOTE: While a lot of prep work has been done to make tasks reasonably
// thread safe, this is not currently guaranteed and will likely come with
// some caveats in the end.
//
class COMMON_EXPORT BaseTask : public QObject, public QEnableSharedFromThis<BaseTask>, private Node<BaseTask>
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("async")

    template<typename Result> friend class Task;
    friend class Node<BaseTask>;
    friend class NodeList<BaseTask>;

public:
    enum State : uint
    {
        // Task has neither resolved nor rejected yet
        Pending   = 0x00000000u,
        // Task has resolved and result() is valid
        Resolved  = 0x40000000u,
        // Task has rejected
        Rejected  = 0x80000000u,

        // For internal use
        StateMask = 0xc0000000u,
        Connected = 0x20000000u,
        IndexMask = 0x0fffffffu,
    };
    Q_ENUM(State)

protected:
    Error _error;
    // We keep the state as atomic since the 'Connected' flag can be applied
    // from possibly arbitrary threads (whenever something is connected).
    std::atomic<uint> _state;

private:
    // Constructor is private so only our friend subclass Tasks can construct.
    explicit BaseTask(QObject* parent = nullptr);
    // A check called in derived destructors, so we can do generic destructor
    // checks while the subclass and its virtual functions are still valid.
    void destructorCheck();
    // Disconnects any listeners on the finished signal. This will release
    // the references those listeners are holding, which in turn can delete
    // ourselves, so this is only called last in any function.
    void disconnectDependents();

public:
    virtual ~BaseTask() override;

    // Mainly for testing: resets the counter for "naming" tasks.
    static void resetTaskIndex();
    // Get the number of live tasks.
    static int getTaskCount();
    // Reject all unfinished tasks with a given error (e.g. during shutdown).
    static void rejectAllTasks(const Error& error, bool synchronous = false);
    static void rejectAllTasks(bool synchronous = false);

    State state() const { return static_cast<State>(_state & StateMask); }
    bool isPending() const { return _state < Resolved; }
    bool isResolved() const { return _state & Resolved; }
    bool isRejected() const { return _state & Rejected; }
    bool isFinished() const { return _state >= Resolved; }

    // Rejects the task with a custom Error
    void reject(Error error);
    // Rejects the task with a default TaskRejected error
    void reject();

    // Get the error for the task. This is valid in any state; if the task has
    // resolved the error is Error::Success, while if the task is still running
    // the error is Error::TaskStillPending.
    const Error& error() const { return _error; }

    // Ensures the task finishes, even if no one is listening for the result.
    // This holds a reference to the task, so use with caution.
    void runUntilFinished(QObject* context);
    void runUntilFinished();

    // Signal that you're intentionally abandoning this task,
    // and don't want a warning when it's destroyed.
    void abandon();

signals:
    void finished();

protected:
    // Notify when rejected (so Task<T> can default-construct its result)
    virtual void notifyRejected() {}
    // Try to figure out the name of the result type if possible.
    virtual const char* typeName() const;
    // Pretty print the task when debugging.
    friend std::ostream COMMON_EXPORT &operator<<(std::ostream &os, const BaseTask* task);

    virtual void connectNotify(const QMetaMethod &signal) override;

    bool setConnected();
    bool setFinished(State state);
};

// Templated task implementation that can resolve to a particular value,
// which upon completion can be accessed with the result() function. If the
// task ends with rejection or an exception, the error is instead accessible
// via the error() function. Rather than create instances of this directly,
// you will usually want to use Async<T> and Async<T>::create().
//
template<typename Result>
class Task : public BaseTask
{
    static_assert(!std::is_base_of<BaseTask, Result>::value, "Result must not be a Task");

    template<typename T, typename R, class C> friend class Async;
    template<typename T> friend class AsyncFunctions;
    template<typename T> friend class Task;

    Q_DECL_ALIGN(Result) char _result[sizeof(Result)];

protected:
    Result& writableResult() { return *reinterpret_cast<Result*>(_result); }

protected:
    explicit Task(QObject* parent = nullptr) : BaseTask(parent) {}

public:
    inline virtual ~Task() override;

    typedef Result ResultType;
    typedef Async<Result> AsyncType;

    inline QSharedPointer<Task<Result>> sharedFromThis() { return qSharedPointerCast<Task<Result>>(BaseTask::sharedFromThis()); }

    // Resolve the task, constructing the result value with the supplied
    // constructor arguments.
    template<typename... Args> inline void resolve(Args&&... args);
    // Overloads to construct the result as a parameter (needed to be able
    // to use initializer syntax).
    inline void resolve(const Result& result) { resolve<const Result&>(result); }
    inline void resolve(Result&& result) { resolve<Result&&>(std::move(result)); }
    // Resolve the task with another Task; the result of the other task will,
    // once it is available, be used to actually resolve this task.
    template<typename T, typename R, class C> inline void resolve(Async<T, R, C> task);

    // Get the result for this task. This is only valid once the task has
    // finished. For rejected tasks, this is a default-constructed value.
    const Result& result() const { return *reinterpret_cast<const Result*>(_result); }

public:
    // Chain a callback which is called when the current task finishes (or
    // immediately if the task is already finished). If the task is resolved,
    // the callback is called with a 'Success' error and the result of the task,
    // whereas if the task is rejected the callback is called with the rejection
    // error and a default-constructed result. This function returns an Async<T>
    // representing the return value of the passed callback. If the callback
    // throws an exception, the returned Async<T> rejects with that error.
    //
    template<class Context, typename Func>
    inline auto next(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, error(), result()))>>;
    template<typename Func>
    inline auto next(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return next(this, std::forward<Func>(fn), type); }

    // Chain a callback which is called when the current task resolves (or
    // immediately if the task is already resolved). The callback is called
    // with the result as a parameter. This function returns an Async<T>
    // representing the return value of the passed callback. If the callback
    // throws an exception, the returned Async<T> rejects with that error.
    //
    template<class Context, typename Func>
    inline auto then(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, result()))>>;
    template<typename Func>
    inline auto then(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return then(this, std::forward<Func>(fn), type); }

    // Chain a callback which is called if the current task is rejected (or
    // immediately if the task is already rejected). The callback is called
    // with the error as a parameter. This function returns an Async<Result>
    // representing the return value of the passed callback, or the original
    // value if the task is not rejected. If the callback throws an exception,
    // the returned Async<T> rejects with that error.
    //
    template<class Context, typename Func>
    inline auto except(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> std::enable_if_t<!std::is_function<Context>::value, Async<Result>>;
    template<typename Func>
    inline auto except(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return except(this, std::forward<Func>(fn), type); }

    // Call the given callback once this task is finished. If the task is
    // already finished, the callback can be called synchronously, otherwise
    // the task is kept alive until the callback has been called. If the
    // recipient implements QEnableSharedFromThis (such as a Task) a strong
    // reference will be held during the callback, keeping the recipient
    // alive throughout the call. The callback is called with error() and
    // result() as arguments. If the recipient is destroyed, the callback
    // does not get called.
    template<class Recipient, typename Func>
    inline auto notify(Recipient* recipient, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> std::enable_if_t<!std::is_function<Recipient>::value>;
    template<typename Func>
    inline void notify(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { notify(this, std::forward<Func>(fn), type); }

protected:
    // Override to default-construct our result.
    virtual inline void notifyRejected() override;
    // When Result is a known Qt type, we can get its name at runtime.
    virtual inline const char* typeName() const override;
};

// Task<T> specialization for void; no result is stored, and resolve() does
// not take any parameters.
//
template<>
class COMMON_EXPORT Task<void> : public BaseTask
{
    template<typename T, typename R, class C> friend class Async;
    template<typename T> friend class AsyncFunctions;
    template<typename T> friend class Task;

public:

protected:
    explicit Task(QObject* parent = nullptr) : BaseTask(parent) {}

public:
    virtual ~Task() override;

    typedef void ResultType;
    typedef Async<void> AsyncType;

    inline QSharedPointer<Task<void>> sharedFromThis() { return qSharedPointerCast<Task<void>>(BaseTask::sharedFromThis()); }

    // Resolve the task.
    void resolve();
    // Resolve the task with another Task; once the other task is resolved,
    // this task will be resolved as well.
    template<typename T, typename R, class C> inline void resolve(Async<T, R, C> task);

    // Just for type detection consistency
    void result() const {}

public:
    // Chain a callback which is called when the current task finishes (or
    // immediately if the task is already finished). If the task is resolved,
    // the callback is called with a 'Success' error, whereas if the task is
    // rejected the callback is called with the rejection error. This function
    // returns an Async<T> representing the return value of the passed
    // callback. If the callback throws an exception, the returned Async<T>
    // rejects with that error.
    //
    template<class Context, typename Func>
    inline auto next(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, error()))>>;
    template<typename Func>
    inline auto next(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return next(this, std::forward<Func>(fn), type); }

    // Chain a callback which is called when the current task resolves (or
    // immediately if the task is already resolved). The callback is called
    // with no parameters. This function returns an Async<T> representing
    // the return value of the passed callback. If the callback throws an
    // exception, the returned Async<T> rejects with that error.
    //
    template<class Context, typename Func>
    inline auto then(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> Async<AsyncResultType<decltype(impl::invoke(context, fn))>>;
    template<typename Func>
    inline auto then(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return then(this, std::forward<Func>(fn), type); }

    // Chain a callback which is called if the current task is rejected (or
    // immediately if the task is already rejected). The callback is called
    // with the error as a parameter. This function returns an Async<void>
    // which resolves upon the return from the callback. If the callback
    // throws an exception, the returned Async<void> rejects with that error.
    //
    template<class Context, typename Func>
    inline auto except(Context* context, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> std::enable_if_t<!std::is_function<Context>::value, Async<void>>;
    template<typename Func>
    inline auto except(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { return except(this, std::forward<Func>(fn), type); }

    // Call the given callback once this task is finished. If the task is
    // already finished, the callback can be called synchronously, otherwise
    // the task is kept alive until the callback has been called. If the
    // recipient implements QEnableSharedFromThis (such as a Task) a strong
    // reference will be held during the callback, keeping the recipient
    // alive throughout the call. The callback is called with error() as an
    // argument. If the recipient is destroyed, the callback does not get
    // called.
    template<class Recipient, typename Func>
    inline auto notify(Recipient* recipient, Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) -> std::enable_if_t<!std::is_function<Recipient>::value>;
    template<typename Func>
    inline void notify(Func&& fn, Qt::ConnectionType type = Qt::AutoConnection) { notify(this, std::forward<Func>(fn), type); }

protected:
    // We know void is "void".
    virtual const char* typeName() const override;
};

// Abortable task - wraps an inner task with another task that can be aborted.
// If the outer task is aborted, the inner task is abandoned and the task chain
// is freed.
//
// Used to implement Async::abortable().
template<typename Result>
class AbortableTask : public Task<Result>
{
public:
    explicit AbortableTask(QSharedPointer<Task<Result>> pInnerTask)
    {
        Q_ASSERT(pInnerTask);  // Ensured by caller
        // Use next() (not notify()) so we can abandon the task.  This returns a
        // new task, hang onto that one and drop the reference if the task is
        // aborted.
        _pInnerTask = pInnerTask->next(this, [this](const Error &err, const Result &result)
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                if(err)
                    this->reject(err);
                else
                    this->resolve(result);
            }
            else
            {
                qInfo() << "Task" << this << "/"
                    << (_pInnerTask ? _pInnerTask.get() : nullptr)
                    << "- inner task completed after being aborted";
            }
        });
    }

public:
    void abort(Error err)
    {
        auto keepAlive = this->sharedFromThis();
        if(this->isPending())
        {
            qInfo() << "Aborting task" << this << "/"
                << (_pInnerTask ? _pInnerTask.get() : nullptr)
                << "with error" << err;
            this->reject(std::move(err));
            if(_pInnerTask)
            {
                _pInnerTask->abandon();
                _pInnerTask.reset();
            }
        }
        else
        {
            qInfo() << "Task" << this << "/"
                << (_pInnerTask ? _pInnerTask.get() : nullptr)
                << "has already finished, can't abort";
        }
    }

private:
    QSharedPointer<Task<Result>> _pInnerTask;
};

// Timeout task - wraps an inner task with a timeout.  If the timeout elapses,
// the task is rejected with Error::Code::TaskTimedOut, and the inner task is
// abandoned.
//
// Used to implement Async::timeout().
//
// Note that TimeoutTask<void> is explicitly specialized later.
template<typename Result>
class TimeoutTask : public Task<Result>
{
public:
    explicit TimeoutTask(Async<Result> pInnerTask,
                         std::chrono::milliseconds timeout)
        : _pInnerTask{std::move(pInnerTask)}
    {
        Q_ASSERT(_pInnerTask);  // Ensured by caller
        // Set the timer first - the inner task could already be resolved; this
        // ensures the notify callback can stop the timer
        QObject::connect(&_timer, &QTimer::timeout, this, [this]()
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                this->reject({HERE, Error::Code::TaskTimedOut});
                _pInnerTask.abandon();
            }
        });

        _timer.setSingleShot(true);
        _timer.start(msec(timeout));
        _pInnerTask->notify(this, [this](const Error &err, const Result &result)
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                _timer.stop();
                if(err)
                    this->reject(err);
                else
                    this->resolve(result);
            }
        });
    }

private:
    Async<Result> _pInnerTask;
    QTimer _timer;
};

// Helper to provide static functions for Async class.
template<typename Result>
class AsyncFunctions
{
public:
    // A shortcut to create an already resolved Async, constructed from the
    // provided constructor arguments.
    template<typename... Args> static inline Async<Result> resolve(Args&&... args);
    // A shortcut to create an Async resolved to another Async (equivalent
    // to constructing the Async directly but with the syntax of resolving).
    template<typename T, typename R, class C> static inline Async<Result> resolve(Async<T, R, C> result);

    // A shortcut to create an already rejected Async with a given Error.
    static inline Async<Result> reject(Error error);
    // A shortcut to create an already rejected Async with a default Error.
    static inline Async<Result> reject();

    // Create an aggregate Async, where whichever input task resolves or
    // rejects first becomes the result of the aggregate Async. Consider
    // using AutoAsync::race[Iterable] to avoid explicitly providing the
    // result type.
    template<typename Iterable> static inline auto raceIterable(const Iterable& iterable) { return raceInternal(iterable); }
    template<typename T, typename R, class C> static inline auto raceIterable(const std::initializer_list<Async<T>>& list) { return raceInternal<std::initializer_list<Async<T>>>(list); }
    template<typename... Tasks> static inline auto race(Tasks&&... tasks) { return raceInternal(std::initializer_list<Async<Result>> { tasks... }); }

    // Create an aggregate Async, where the result is either a vector of
    // all the results from the input tasks, or the error from the first
    // failed task. Consider using AutoAsync::all[Iterable] to avoid
    // explicitly providing the result type.
    template<typename Iterable> static inline Async<QVectorOrVoid<Result>> allIterable(const Iterable& iterable) { return allInternal(iterable); }
    template<typename T, typename R, class C> static inline Async<QVectorOrVoid<Result>> allIterable(const std::initializer_list<Async<T>>& list) { return allInternal<std::initializer_list<Async<T>>>(list); }
    template<typename... Tasks> static inline Async<QVectorOrVoid<Result>> all(Tasks&&... tasks) { return allInternal(std::initializer_list<Async<Result>> { tasks... }); }

private:
    template<typename Iterable>
    static inline Async<Result> raceInternal(const Iterable& iterable);
    template<typename Iterable>
    static inline Async<QVectorOrVoid<Result>> allInternal(const Iterable& iterable);
};


// Convenience wrapper for return values for "asynchronous" functions
// (i.e. functions that return a Task<T> the caller can then wait for).
//
// This is the class used for everything; as a return value from functions
// that return results asynchonously, and as a type safe and managed handle
// to refer to such asynchronous tasks in a Promise-like manner. Tasks can
// be chained together, upon which child tasks keep their parents alive
// until their result has been delivered. If a task is abandoned, so are
// its handles to its dependencies.
//
// The Result and Class template arguments are normally not specified and
// are instead deduced from the Type template argument. If Type is a class
// derived from Task<T>, Result is deduced to T and Class to Type. This is
// for using Async<T> to manage custom Tasks. Otherwise in the normal case
// Type is the Result type and Class is deduced to Task<Type>.
//
template<typename Type, typename Result, class Class>
class Q_REQUIRED_RESULT Async : public QSharedPointer<Class>, public AsyncFunctions<Result>
{
    static_assert(!std::is_base_of<BaseTask, Result>::value, "Result must not be a Task");
    static_assert(std::is_base_of<Task<Result>, Class>::value, "Class must be a Task<Result>");
    static_assert(std::is_same<typename Class::ResultType, Result>::value, "Class::ResultType must match Result");
    static_assert(std::is_same<Class, Task<Type>>::value || std::is_same<Class, Type>::value, "invalid Type");
    static_assert(std::is_same<Result, AsyncResultType<Result>>::value, "Result should be stable");
    static_assert(std::is_same<Class, AsyncClassType<Class>>::value, "Class should be stable");

    typedef QSharedPointer<Class> base;
    friend class Task<Result>;
public:
    typedef Result ResultType;
    typedef Class TaskType;
public:
    Async() {}
    explicit Async(Class* task) : base(task) {}
    Async(const base& copy) : base(copy) {}
    Async(base&& move) : base(std::move(move)) {}
    template<typename T, typename R, class C> inline Async(const Async<T, R, C>& task);
    template<typename C> inline Async(const QSharedPointer<C>& ptr) : base(ptr) {}
    template<typename C> inline Async(QSharedPointer<C>&& ptr) : base(std::move(ptr)) {}

    // The typical way to create a new Async for a future result. If using
    // a custom Task subclass, this method forwards its arguments to the
    // constructor of the custom class.
    template<typename... Args> static inline Async create(Args&&... args);

    // Abandon the task and reset the handle.
    inline void abandon();

    // Time out a task if it hasn't resolved by then.  The returned task
    // resolves or rejects with the nested task's result if it occurs before the
    // timeout, or rejects with Error::Code::TaskTimedOut when the timeout
    // elapses.
    Async<Result> timeout(std::chrono::milliseconds timeout) {return Async<TimeoutTask<Result>>::create(*this, timeout);}

    // Wrap a task in a AbortableTask.  The returned task's abort() method can
    // be used to abort the task prematurely.
    Async<AbortableTask<Result>> abortable() {return Async<AbortableTask<Result>>::create(*this);}
};

// Specialization of AbortableTask for void result
template<>
class AbortableTask<void> : public Task<void>
{
public:
    explicit AbortableTask(QSharedPointer<Task<void>> pInnerTask)
    {
        Q_ASSERT(pInnerTask);  // Ensured by caller
        // Use next() (not notify()) so we can abandon the task.  This returns a
        // new task, hang onto that one and drop the reference if the task is
        // aborted.
        _pInnerTask = pInnerTask->next(this, [this](const Error &err)
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                if(err)
                    this->reject(err);
                else
                    this->resolve();
            }
            else
            {
                qInfo() << "Task" << this << "/"
                    << (_pInnerTask ? _pInnerTask.get() : nullptr)
                    << "- inner task completed after being aborted";
            }
        });
    }

public:
    void abort(Error err)
    {
        auto keepAlive = this->sharedFromThis();
        if(this->isPending())
        {
            qInfo() << "Aborting task" << this << "/"
                << (_pInnerTask ? _pInnerTask.get() : nullptr)
                << "with error" << err;
            this->reject(std::move(err));
            if(_pInnerTask)
            {
                _pInnerTask->abandon();
                _pInnerTask.reset();
            }
        }
        else
        {
            qInfo() << "Task" << this << "/"
                << (_pInnerTask ? _pInnerTask.get() : nullptr)
                << "has already finished, can't abort";
        }
    }

private:
    QSharedPointer<Task<void>> _pInnerTask;
};

// Specialization of TimeoutTask for void result
template<>
class TimeoutTask<void> : public Task<void>
{
public:
    explicit TimeoutTask(Async<void> pInnerTask,
                         std::chrono::milliseconds timeout)
        : _pInnerTask{std::move(pInnerTask)}
    {
        Q_ASSERT(_pInnerTask);  // Ensured by caller
        // Set the timer first - the inner task could already be resolved; this
        // ensures the notify callback can stop the timer
        QObject::connect(&_timer, &QTimer::timeout, this, [this]()
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                this->reject({HERE, Error::Code::TaskTimedOut});
                _pInnerTask.abandon();
            }
        });

        _timer.setSingleShot(true);
        _timer.start(msec(timeout));
        _pInnerTask->notify(this, [this](const Error &err)
        {
            auto keepAlive = this->sharedFromThis();
            if(this->isPending())
            {
                _timer.stop();
                if(err)
                    this->reject(err);
                else
                    this->resolve();
            }
        });
    }

private:
    Async<void> _pInnerTask;
    QTimer _timer;
};

// Delay task - just resolves after the specified time has elapsed
class DelayTask : public Task<void>
{
public:
    explicit DelayTask(std::chrono::milliseconds timeout)
    {
        QObject::connect(&_timer, &QTimer::timeout, this, [this]()
        {
            auto keepAlive = sharedFromThis();
            resolve();
        });

        _timer.setSingleShot(true);
        _timer.start(msec(timeout));
    }

private:
    QTimer _timer;
};

// Just a wrapper for some of Async's static functions to be able to
// call them with deduced instead of explicit result types.
//
class COMMON_EXPORT AutoAsync
{
    // Prevent construction; this is just a namespacing class
    AutoAsync() {}

public:
    // Construct an already resolved Async using type deduction based on the
    // supplied parameters. Should be able to handle all trivial cases.
    template<typename Result>
    static inline auto resolve(Result&& result)
    {
        return Async<AsyncResultType<Result>>::resolve(std::forward<Result>(result));
    }

    // Construct an Async already resolved to another Async; this is the same
    // as merely taking the original Async.
    template<typename Result>
    static inline Async<Result> resolve(const Async<Result>& task)
    {
        return task;
    }
    template<typename Result>
    static inline Async<Result> resolve(Async<Result>&& task)
    {
        return std::move(task);
    }

    // Construct an Async already resolved to whatever the supplied smart
    // pointer is referring to (e.g. a derived subclass of Task).
    template<typename T>
    static inline Async<typename T::ResultType> resolve(const QSharedPointer<T>& task)
    {
        return task;
    }

    // Create an aggregate race task, deducing the type from the item type
    // of the supplied iterable.
    template<typename Iterable>
    static inline auto raceIterable(const Iterable& iterable)
    {
        return Async<AsyncResultType<decltype(*std::begin(iterable))>>::race(iterable);
    }
    // Special case to be able to pass initializer lists directly.
    template<typename Result>
    static inline auto raceIterable(const std::initializer_list<Async<Result>>& list)
    {
        return Async<Result>::template race<std::initializer_list<Async<Result>>>(list);
    }

    // Create an aggregate race task from the typed parameter tasks; the result
    // type is deduced from the common type of the input tasks, or void if the
    // input tasks have no type in common.
    template<typename... Tasks>
    static inline auto race(Tasks&&... tasks)
    {
        typedef CommonTypeOrVoid<typename std::decay_t<Tasks>::ResultType...> ResultType;
        return Async<ResultType>::raceIterable(std::initializer_list<Async<ResultType>> { std::forward<Tasks>(tasks)... });
    }
    // Special case for a race with no inputs; a void task that never resolves.
    static inline Async<void> race()
    {
        return Async<void>::create();
    }

    // Create an aggregate all task, with the same item type as the item
    // type of the supplied iterable.
    template<typename Iterable>
    static inline auto allIterable(const Iterable& iterable)
    {
        return Async<AsyncResultType<decltype(*std::begin(iterable))>>::all(iterable);
    }
    // Special case to be able to pass initializer lists directly.
    template<typename Result>
    static inline auto allIterable(const std::initializer_list<Async<Result>>& list)
    {
        return Async<Result>::template all<std::initializer_list<Async<Result>>>(list);
    }

    // Create an aggregate all task from the typed parameter tasks; the result
    // is a vector of the deduced common type of the input tasks, or void if
    // the input tasks have no type in common.
    template<typename... Tasks>
    static inline auto all(Tasks&&... tasks)
    {
        typedef CommonTypeOrVoid<typename std::decay_t<Tasks>::ResultType...> ResultType;
        return Async<ResultType>::allIterable(std::initializer_list<Async<ResultType>> { std::forward<Tasks>(tasks)... });
    }
    // Special case for an all with no inputs; an already resolved void task.
    static inline Async<void> all()
    {
        return Async<void>::resolve();
    }
};


// Avoid the QSharedPointer(...) decoration you normally get from QSharedPointer.
template<typename T>
inline std::ostream &operator<<(std::ostream &os, const QSharedPointer<Task<T>>& task)
{
    return os << task.get();
}


// Convenience function to cast an Async return value to a different type.
template<typename Output, typename Input>
inline std::enable_if_t<!std::is_same<Input, Output>::value, Async<Output>> async_cast(const Async<Input>& input)
{
    return input->then([](const Input& value) { return static_cast<Output>(value); });
}
// Specialization when casting an Async return value to its own type.
template<typename Output, typename Input>
inline std::enable_if_t<std::is_same<Input, Output>::value, const Async<Output>&> async_cast(const Async<Input>& input)
{
    return input;
}
template<typename Output, typename Input>
inline std::enable_if_t<std::is_same<Input, Output>::value, Async<Output>&&> async_cast(Async<Input>&& input)
{
    return std::move(input);
}


// Inline function definitions

template<typename Result>
inline Task<Result>::~Task()
{
    destructorCheck();
    if (isResolved())
    {
        writableResult().~Result();
    }
}

template<typename Result>
template<typename... Args>
inline void Task<Result>::resolve(Args&&... args)
{
    if (setFinished(Resolved))
    {
        _error = Error(HERE, Error::Success);
        new(_result) Result(std::forward<Args>(args)...);
        // We have to keep the task alive, in case slots connected to finished()
        // drop their reference when the signal is emitted
        auto keepAlive = sharedFromThis();
        emit finished();
        // This must be called last in the function
        disconnectDependents();
    }
}

template<typename Result>
template<typename T, typename R, class C>
inline void Task<Result>::resolve(Async<T, R, C> task)
{
    static_assert(std::is_convertible<R, Result>::value, "chained task result must be convertible to target type");
    task->notify(this, [this](const Error& error, const R& result) {
        if (error)
            reject(error);
        else
            resolve(result);
    });
}

template<typename Result>
template<class Context, typename Func>
inline auto Task<Result>::next(Context* context, Func&& fn, Qt::ConnectionType type) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, error(), result()))>>
{
    auto task = Async<AsyncResultType<decltype(impl::invoke(context, fn, error(), result()))>>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error, const Result& result) {
        auto context = impl::getNakedPointer(ctx);
        if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn, error, result));
    }, type);
    return task;
}

template<typename Result>
template<class Context, typename Func>
inline auto Task<Result>::then(Context* context, Func&& fn, Qt::ConnectionType type) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, result()))>>
{
    auto task = Async<AsyncResultType<decltype(impl::invoke(context, fn, result()))>>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error, const Result& result) {
        auto context = impl::getNakedPointer(ctx);
        if (error)
            task->reject(error);
        else if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn, result));
    }, type);
    return task;
}

template<typename Result>
template<class Context, typename Func>
inline auto Task<Result>::except(Context* context, Func&& fn, Qt::ConnectionType type) -> std::enable_if_t<!std::is_function<Context>::value, Async<Result>>
{
    auto task = Async<Result>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error, const Result& result) {
        auto context = impl::getNakedPointer(ctx);
        if (!error)
            task->resolve(result);
        else if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn, error));
    }, type);
    return task;
}

template<typename Result>
template<class Recipient, typename Func>
inline auto Task<Result>::notify(Recipient* recipient, Func&& fn, Qt::ConnectionType type) -> std::enable_if_t<!std::is_function<Recipient>::value>
{
    const bool done = isFinished();
    const auto actualType = static_cast<Qt::ConnectionType>(type & ~Qt::UniqueConnection);
    if (done && (type == Qt::DirectConnection || (type == Qt::AutoConnection && recipient->thread() == QThread::currentThread())))
    {
        // The callback can be called synchronously.
        auto keepAlive = impl::KeepAliveIfNeeded(recipient);
        // Clang would warn that keepAlive is unused if no keep-alive reference
        // was needed
        Q_UNUSED(keepAlive);
        impl::invoke(recipient, fn, error(), result());
    }
    else
    {
        // We need to invoke asynchronously, so prepare a lambda (which also
        // keeps us alive until the call finishes).
        auto call = [self = sharedFromThis(), recipient, f = std::move(fn)]() mutable {
            auto keepAlive = impl::KeepAliveIfNeeded(recipient);
            Q_UNUSED(keepAlive);
            impl::invoke(recipient, f, self->error(), self->result());
        };
        if (done)
            QMetaObject::invokeMethod(recipient, std::move(call), actualType);
        else
            QObject::connect(this, &BaseTask::finished, recipient, std::move(call), type);
    }
}

template<typename Result>
inline void Task<Result>::notifyRejected()
{
    new(_result) Result();
}

template<typename Result>
inline const char* Task<Result>::typeName() const
{
    return qTypeName<Result>();
}

template<typename T, typename R, class C>
inline void Task<void>::resolve(Async<T, R, C> task)
{
    task->notify(this, [this](const Error& error) {
        if (error)
            reject(error);
        else
            resolve();
    });
}

template<class Context, typename Func>
inline auto Task<void>::next(Context* context, Func&& fn, Qt::ConnectionType type) -> Async<AsyncResultType<decltype(impl::invoke(context, fn, error()))>>
{
    auto task = Async<AsyncResultType<decltype(impl::invoke(context, fn, error()))>>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error) {
        auto context = impl::getNakedPointer(ctx);
        if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn, error));
    }, type);
    return task;
}

template<class Context, typename Func>
inline auto Task<void>::then(Context* context, Func&& fn, Qt::ConnectionType type) -> Async<AsyncResultType<decltype(impl::invoke(context, fn))>>
{
    auto task = Async<AsyncResultType<decltype(impl::invoke(context, fn))>>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error) {
        auto context = impl::getNakedPointer(ctx);
        if (error)
            task->reject(error);
        else if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn));
    }, type);
    return task;
}

template<class Context, typename Func>
inline auto Task<void>::except(Context* context, Func&& fn, Qt::ConnectionType type) -> std::enable_if_t<!std::is_function<Context>::value, Async<void>>
{
    auto task = Async<void>::create();
    task->moveToThread(context->thread());
    notify(task.get(), [task = task.get(), ctx = impl::QPointerIfNeeded(context), fn = std::move(fn)](const Error& error) {
        auto context = impl::getNakedPointer(ctx);
        if (!error)
            task->resolve();
        else if (!context)
            task->reject(Error(HERE, Error::TaskRecipientDestroyed));
        else
            GUARD_WITH(task->reject, impl::resolveWith(task, context, fn, error));
    }, type);
    return task;
}

template<class Recipient, typename Func>
inline auto Task<void>::notify(Recipient* recipient, Func&& fn, Qt::ConnectionType type) -> std::enable_if_t<!std::is_function<Recipient>::value>
{
    const bool done = isFinished();
    const auto actualType = static_cast<Qt::ConnectionType>(type & ~Qt::UniqueConnection);
    if (done && (type == Qt::DirectConnection || (type == Qt::AutoConnection && recipient->thread() == QThread::currentThread())))
    {
        // The callback can be called synchronously.
        auto keepAlive = impl::KeepAliveIfNeeded(recipient);
        Q_UNUSED(keepAlive);
        impl::invoke(recipient, fn, error());
    }
    else
    {
        // We need to invoke asynchronously, so prepare a lambda (which also
        // keeps us alive until the call finishes).
        auto call = [self = sharedFromThis(), recipient, f = std::move(fn)]() {
            auto keepAlive = impl::KeepAliveIfNeeded(recipient);
            Q_UNUSED(keepAlive);
            impl::invoke(recipient, f, self->error());
        };
        if (done)
            QMetaObject::invokeMethod(recipient, std::move(call), actualType);
        else
            QObject::connect(this, &BaseTask::finished, recipient, std::move(call), type);
    }
}


template<typename Type, typename Result, class Class>
template<typename T, typename R, class C>
inline Async<Type, Result, Class>::Async(const Async<T, R, C>& task)
{
    static_assert(std::is_convertible<R, Result>::value || std::is_void<Result>::value, "R must be convertible to Result");
    base::reset(new Class());
    base::get()->resolve(task);
}

template<typename Type, typename Result, class Class>
template<typename... Args>
inline Async<Type, Result, Class> Async<Type, Result, Class>::create(Args&&... args)
{
    return Async { new Class(std::forward<Args>(args)...) };
}

template<typename Type, typename Result, class Class>
inline void Async<Type, Result, Class>::abandon()
{
    auto ptr = base::get();
    if (ptr) ptr->abandon();
    base::reset();
}


template<typename Result>
template<typename... Args>
inline Async<Result> AsyncFunctions<Result>::resolve(Args&&... args)
{
    auto task = Async<Result>::create();
    task->resolve(std::forward<Args>(args)...);
    return task;
}

template<typename Result>
template<typename T, typename R, class C>
inline Async<Result> AsyncFunctions<Result>::resolve(Async<T, R, C> result)
{
    return std::move(result);
}

template<typename Result>
inline Async<Result> AsyncFunctions<Result>::reject(Error error)
{
    auto task = Async<Result>::create();
    task->reject(std::move(error));
    return task;
}

template<typename Result>
inline Async<Result> AsyncFunctions<Result>::reject()
{
    return reject(Error(HERE, Error::TaskRejected));
}

template<typename Result>
template<typename Iterable>
inline Async<Result> AsyncFunctions<Result>::raceInternal(const Iterable& iterable)
{
    auto task = Async<Result>::create();
    for (const auto& item : iterable)
    {
        item->notify(task.get(), [task = task.get()](const Error& error, const Result& result) {
            if (task->isFinished())
                return;
            if (error)
                task->reject(error);
            else
                task->resolve(result);
        });
    }
    return task;
}

template<>
template<typename Iterable>
inline Async<void> AsyncFunctions<void>::raceInternal(const Iterable& iterable)
{
    auto task = Async<void>::create();
    for (const auto& item : iterable)
    {
        item->notify(task.get(), [task = task.get()](const Error& error) {
            if (task->isFinished())
                return;
            if (error)
                task->reject(error);
            else
                task->resolve();
        });
    }
    return task;
}

namespace impl {

    // Custom task to hold references to all inputs and count how many
    // unfinished tasks are left. The reference is needed to keep the task
    // alive for long enough to get the result once all tasks are done.
    template<typename Result>
    class AllTask : public Task<QVector<Result>>
    {
    public:
        QVector<Async<Result>> results;
        int remaining;
        template<typename Iterator>
        AllTask(Iterator begin, const Iterator end)
        {
            remaining = end - begin;
            results.reserve(remaining);
            std::copy(begin, end, std::back_inserter(results));
        }
        void inputResolved()
        {
            if (--remaining == 0)
            {
                // We're done! Assemble the results into a vector and release our references.
                QVector<Result> result;
                result.reserve(results.length());
                for (const auto& item : results)
                    result.append(item->result());
                results.clear();
                this->resolve(std::move(result));
            }
        }
        void inputRejected(const Error& error)
        {
            results.clear();
            this->reject(error);
        }
    };
    // Specialization for void; no extra references needed.
    template<>
    class COMMON_EXPORT AllTask<void> : public Task<void>
    {
    public:
        int remaining;
        template<typename Iterator>
        AllTask(Iterator begin, const Iterator end)
        {
            remaining = end - begin;
        }
        void inputResolved()
        {
            if (--remaining == 0)
            {
                // We're done!
                this->resolve();
            }
        }
        void inputRejected(const Error& error)
        {
            this->reject(error);
        }
    };

}


template<typename Result>
template<typename Iterable>
inline Async<QVectorOrVoid<Result>> AsyncFunctions<Result>::allInternal(const Iterable& iterable)
{
    // First check if the input list is empty, and if so return an empty array.
    if (std::begin(iterable) == std::end(iterable))
        return Async<QVectorOrVoid<Result>>::resolve();

    // Create a custom task and copy over all the input tasks.
    auto allTask = new impl::AllTask<Result>(std::begin(iterable), std::end(iterable));
    Async<QVectorOrVoid<Result>> task(allTask);

    // Hook up each input task to deliver results to our custom task.
    for (const auto& item : iterable)
    {
        item->notify(task.get(), [allTask](const Error& error) {
            // Guard against multiple rejects etc.
            if (allTask->isFinished())
                return;
            if (error)
                allTask->inputRejected(error);
            else
                allTask->inputResolved();
        });
    }

    return task;
}

#endif // ASYNC_H
