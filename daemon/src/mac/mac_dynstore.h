// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("mac_dynstore.h")

#ifndef MAC_DYNSTORE_H
#define MAC_DYNSTORE_H

#include "mac_objects.h"
#include "mac_thread.h"
#include <SystemConfiguration/SystemConfiguration.h>

// SCDynamicStore wrapper.  Wraps up a handle to SCDynamicStore and provides
// methods to wrap various function calls.
//
// Also starts a CFRunLoop-based worker thread to service notification events
// (which are dispatched back to the main thread).
class MacDynamicStore : public QObject
{
    Q_OBJECT

private:
    // Static callback function used to receive key-change notifications.  Posts
    // a ChangeEvent to the main thread for MacDynamicStore.
    static void dynStoreCallback(SCDynamicStoreRef, CFArrayRef changedKeys,
                                 void *info);

public:
    MacDynamicStore();

public:
    // Set the notification keys and/or regex patterns.
    bool setNotificationKeys(CFArrayRef keys, CFArrayRef patterns);

    // Get a value from the dynamic store.
    CFHandle<CFPropertyListRef> copyValue(CFStringRef key) const;

    // Get a dictionary containing any number of matching keys from the dynamic
    // store.  Literal keys and regular expressions can be passed.
    MacDict copyMultiple(CFArrayRef keys, CFArrayRef patterns) const;

    // Get a list of keys matching a regex that are currently in the store.
    MacArray copyKeyList(CFStringRef pattern) const;

private:
    // Callback used when monitored keys changed; called by dynStoreCallback()
    // on main thread.
    void keysChangedCallback(MacArray changedKeys);

signals:
    // Monitored keys have changed - the changed keys are provided
    void keysChanged(const MacArray &changedKeys);

private:
    CFHandle<SCDynamicStoreRef> _dynStore;
    nullable_t<MacRunLoopThread> _eventThread;
};

#endif
