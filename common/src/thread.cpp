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
#line SOURCE_FILE("thread.cpp")

#include "thread.h"

RunningWorkerThread::RunningWorkerThread()
{
    _thread.start();
    // Create pThreadObject and move it to the worker thread.
    pThreadObject = new QObject{};
    pThreadObject->moveToThread(&_thread);
}

RunningWorkerThread::~RunningWorkerThread()
{
    // Delete pThreadObject and terminate the worker thread.
    // It's not clear from the doc whether QObject::deleteLater() is
    // thread-safe.  Even if it is, it's very unclear how that would interact
    // with quit() and whether the object's destruction would be guaranteed if
    // we do not wait for it on this thread.
    // This shouldn't add a significant amount of time anyway since we wait for
    // the thread to quit either way.
    invokeOnThread([this](){
        pThreadObject->deleteLater();
        pThreadObject = nullptr;
    });
    _thread.quit();
    _thread.wait();
}

QObject &RunningWorkerThread::objectOwner()
{
    Q_ASSERT(pThreadObject);    // Class invariant
    return *pThreadObject;
}
