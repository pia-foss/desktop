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

#include <common/src/common.h>
#line SOURCE_FILE("mac_dynstore.mm")

#include "mac_dynstore.h"
#include <QCoreApplication>
#include <QThread>

void MacDynamicStore::dynStoreCallback(SCDynamicStoreRef, CFArrayRef changedKeys,
                                       void *info)
{
    Q_ASSERT(info); // Valid info always passed to SCDynamicStoreCreate()
    MacDynamicStore *pThis = reinterpret_cast<MacDynamicStore*>(info);

    // We're currently on the worker thread; post an event to the main thread.
    // Retain a reference to changedKeys to keep it alive throughout the
    // dispatched call.
    QMetaObject::invokeMethod(pThis, [pThis, changedKeyArray = MacArray{{true, changedKeys}}]()
    {
        pThis->keysChanged(std::move(changedKeyArray));
    }, Qt::QueuedConnection);
}

MacDynamicStore::MacDynamicStore()
{
    SCDynamicStoreContext callbackCtx{};
    callbackCtx.info = this;

    _dynStore.reset(::SCDynamicStoreCreate(nullptr, CFSTR("Private Internet Access"),
                                           &dynStoreCallback, &callbackCtx));

    // Create a run loop source so notifications can be received.
    CFHandle<CFRunLoopSourceRef> dynStoreSource{::SCDynamicStoreCreateRunLoopSource(nullptr,
                                                                                    _dynStore.get(),
                                                                                    0)};

    // Create an event thread to service the run loop source
    _eventThread.emplace(dynStoreSource);
}

MacString MacDynamicStore::ssidFromInterface(CFStringRef interfaceName) const
{
    MacString key{SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, interfaceName, kSCEntNetAirPort)};
    qInfo() << "Looking up SSID from dynamic store using" << key;

    // Get the value associated with the key from the dynamic store.
    // This will return a dictionary with many elements, including the SSID_STR (which we're interested in)
    const auto info = copyValue(key.get());

    // Store the dictionary in a MacDict to manage its life-time and release
    MacDict props{info.as<CFDictionaryRef>()};
    // The SSID_STR field contains the SSID of the interface
    return {props.getValueObj(CFSTR("SSID_STR")).as<CFStringRef>()};
}

bool MacDynamicStore::setNotificationKeys(CFArrayRef keys, CFArrayRef patterns)
{
    return ::SCDynamicStoreSetNotificationKeys(_dynStore.get(), keys, patterns);
}

CFHandle<CFPropertyListRef> MacDynamicStore::copyValue(CFStringRef key) const
{
    return CFHandle<CFPropertyListRef>{::SCDynamicStoreCopyValue(_dynStore.get(), key)};
}

MacDict MacDynamicStore::copyMultiple(CFArrayRef keys, CFArrayRef patterns) const
{
    return MacDict{::SCDynamicStoreCopyMultiple(_dynStore.get(), keys, patterns)};
}

MacArray MacDynamicStore::copyKeyList(CFStringRef pattern) const
{
    return CFHandle<CFArrayRef>{::SCDynamicStoreCopyKeyList(_dynStore.get(), pattern)};
}

void MacDynamicStore::keysChangedCallback(MacArray changedKeys)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());

    qInfo() << "Notification contains" << changedKeys.getCount() << "keys";

    for(MacString key : changedKeys.view<CFStringRef>())
        qInfo() << "Key changed:" << key;

    emit keysChanged(changedKeys);
}
