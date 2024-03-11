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

#include <kapps_core/logger.h>
#include "logger.h"

namespace kapps { namespace core {

// Implementation of kapps::core::LogCallback using KACLogCallback
// Like PIA's LoggerCallback, final here silences a warning from clang that
// we're calling a destructor of a polymorphic type without a virtual
// destructor, which is fine here because ApiCallback is the concrete type.
class ApiCallback final : public LogCallback
{
public:
    ApiCallback(KACLogCallback cb) : _cb{cb} {}

public:
    virtual void write(LogMessage msg) override;

private:
    KACLogCallback _cb;
};

void ApiCallback::write(LogMessage msg)
{
    auto adaptSlice = [](const StringSlice &val) -> KACStringSlice
    {
        return {val.data(), val.size()};
    };

    auto adaptLevel = [](LogMessage::Level l) -> int
    {
        switch(l)
        {
            default:
            case LogMessage::Level::Fatal: return KAPPS_CORE_LOG_MESSAGE_LEVEL_FATAL;
            case LogMessage::Level::Error: return KAPPS_CORE_LOG_MESSAGE_LEVEL_ERROR;
            case LogMessage::Level::Warning: return KAPPS_CORE_LOG_MESSAGE_LEVEL_WARNING;
            case LogMessage::Level::Info: return KAPPS_CORE_LOG_MESSAGE_LEVEL_INFO;
            case LogMessage::Level::Debug: return KAPPS_CORE_LOG_MESSAGE_LEVEL_DEBUG;
        }
    };

    KACLogMessage apiMsg
    {
        adaptSlice(msg.category().module() ? msg.category().module()->name() : StringSlice{}),
        adaptSlice(msg.category().name()),
        adaptLevel(msg.level()),
        adaptSlice(msg.loc().file()),
        msg.loc().line(),
        msg.message().c_str()
    };

    (*_cb.pWriteFn)(_cb.pContext, &apiMsg);
}

}}

extern "C" {

void KACLogInit(KACLogCallback *pCallback)
{
    if(!pCallback || !pCallback->pWriteFn)
    {
        // No callback, reset
        kapps::core::log::init({});
        return;
    }

    kapps::core::log::init(std::make_shared<kapps::core::ApiCallback>(*pCallback));
}

void KACEnableLogging(int enable)
{
    kapps::core::log::enableLogging(enable);
}

int KACLoggingEnabled()
{
    return kapps::core::log::loggingEnabled();
}

}
