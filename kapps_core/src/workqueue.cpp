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

#include "workqueue.h"
#include "workfunc.h"
#include <cassert>

namespace kapps { namespace core {

void WorkQueue::enqueue(Any item)
{
    // Lock _itemsMutex only while modifying _items
    {
        std::lock_guard<std::mutex> lock{_itemsMutex};
        _items.push(std::move(item));
    }
    _haveItems.notify_one();
}

Any WorkQueue::dequeue()
{
    std::unique_lock<std::mutex> lock{_itemsMutex};
    // Until an item is available, release the lock and wait to be notified.
    _haveItems.wait(lock, [&]{return !_items.empty();});
    // Take the front work item
    Any item{std::move(_items.front())};
    _items.pop();
    return item;
}

WorkThread::WorkThread(std::function<void(Any)> workFunc)
    : _workThread{[this, workFunc=std::move(workFunc)]{workThreadProc(workFunc);}}
{
}

WorkThread::~WorkThread()
{
    // Tell the work thread to terminate
    enqueue(Terminate{});
    assert(_workThread.joinable()); // Class invariant
    // Wait for it to exit
    _workThread.join();
}

void WorkThread::workThreadProc(const std::function<void(Any)> &workFunc)
{
    while(true)
    {
        auto item = _queue.dequeue();
        // If it's a Terminate, we're done
        if(item.containsType<Terminate>())
            return;
        // If it's a WorkFunc<void>, invoke it automatically, thread work functions
        // don't have to handle this individually.  (Check with containsType()
        // first so we don't pass these to _workFunc; there's no way to know
        // if the type matched a handle() or not.)
        if(item.containsType<WorkFunc<>>())
            item.handle<WorkFunc<>>([](WorkFunc<> &func){func.invoke();});
        else
            workFunc(std::move(item));
    }
}

void WorkThread::enqueue(Any item)
{
    _queue.enqueue(std::move(item));
}

void WorkThread::queueInvoke(std::function<void()> func)
{
    enqueue(WorkFunc<>{std::move(func)});
}

void WorkThread::syncInvoke(std::function<void()> func)
{
    SyncWorkFunc syncWork;
    enqueue(syncWork.work<>(std::move(func)));
    syncWork.wait();
}

}}
