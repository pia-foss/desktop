// Copyright (c) 2022 Private Internet Access, Inc.
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

#pragma once
#include <kapps_core/core.h>
#include "logger.h"
#include "util.h"
#include <future>
#include <functional>

namespace kapps { namespace core {

// TODO:
// WorkFunc and SyncWorkFunc both wrap std::function<>, but this leads to some
// fragility that would probably be better suited with a different wrapper:
//
// - We'd like to optionally pass parameters only if the functor requires them;
//   i.e. accept either void() or void(PollWorker&) depending on whether the
//   caller actually cares about the PollWorker& or not.  This could be
//   provided by ignoring parameters that aren't actually accepted by the
//   callable.
// - SyncWorkFunc's generated WorkFunc holds a reference back to the
//   SyncWorkFunc itself.  It can't hold the std::promise directly, because
//   std::function requires the functor to be copiable, and std::promise isn't.
//   A move-only analog to std::function would be better suited for this.
// - These functors should never be empty, but std::function has a 'null' state
//   that necessitates a lot of empty checks (which are often neglected).  A
//   non-nullable functor, or one that silently ignores calls to an empty
//   functor, would be less fragile.

// WorkFunc is used by WorkThread and PollThread to invoke a functor on the
// thread.  It's mostly a wrapper for std::function, the fact that it is a
// WorkFunc indicates to WorkThread/PollThread that we want it to invoke the
// function.  Callers usually do not use this directly; use the methods of
// WorkThread and PollThread.
//
// It does catch and trace exceptions from the work functor, since there is no
// useful place for these to be thrown on the work thread, and we can't
// return them to the calling thread (SyncWorkFunc can, though).
//
// WorkThread/PollThread will invoke the functor asynchronously, so of course
// don't capture any references that could become invalid.
template<class... ArgsT>
class WorkFunc
{
public:
    WorkFunc(std::function<void(ArgsT...)> func) : _func{std::move(func)} {}

public:
    template<class... CallArgsT>
    void invoke(CallArgsT&&... args)
    {
        try
        {
            _func(std::forward<CallArgsT>(args)...);
        }
        catch(const std::exception &ex)
        {
            // Just trace it, nothing else useful we can do with it
            KAPPS_CORE_WARNING() << "Work item threw an exception:" << ex.what();
        }
        catch(...)
        {
            KAPPS_CORE_WARNING() << "Work item threw unknown exception";
        }
    }

private:
    std::function<void(ArgsT...)> _func;
};

// Create a work function that will signal an associated future after it is
// invoked.  By queuing the WorkFunc and waiting on the future, a function can
// be invoked synchronously on the worker thread.
//
// WorkThread, PollThread, etc. use this to implement syncInvoke(), usually it
// doesn't need to be used directly.
class SyncWorkFunc
{
public:
    // Get the WorkFunc to enqueue.  This can be called once - subsequent calls
    // will throw a std::future_error.  Note that the resulting WorkFunc holds a
    // reference to SyncWorkFunc, so you must call wait() after enqueueing the
    // WorkFunc.
    template<class... ArgsT>
    WorkFunc<ArgsT...> work(std::function<void(ArgsT...)> userFunc)
    {
        assert(userFunc);   // Ensured by caller

        // Get the future that we can wait on - if work() has already been called,
        // this throws intentionally.
        _invokeFuture = _invokePromise.get_future();

        // Return a functor wrapping userFunc that will signal the promise after
        // completing.
        return WorkFunc<ArgsT...>{[this, userFunc = std::move(userFunc)](ArgsT... args)
        {
            try
            {
                userFunc(std::forward<ArgsT>(args)...);
                _invokePromise.set_value();
            }
            catch(...)
            {
                // Send any exceptions from userFunc() over to the waiting thread
                _invokePromise.set_exception(std::current_exception());
            }
        }};
    }

    // Wait for the WorkFunc to be executed.  Throws if work() hasn't been
    // called yet.
    void wait();

private:
    // The future signaled by the WorkFunc.  wait() waits on this.
    //
    // It's initialized when work() is called - that way, calling wait() before
    // work() throws, and calling work() more than once throws (both
    // intentional).
    std::future<void> _invokeFuture;
    // The promise for the work function's invocation.  The WorkFunc signals
    // this.
    //
    // We'd really prefer to hold this in the WorkFunc itself, but
    // std::function's functor has to be copiable, so instead it captures
    // 'this'.  Note that the WorkFunc will be invoked on another thread, so we
    // can't touch this after creating the WorkFunc.
    std::promise<void> _invokePromise;
};

}}
