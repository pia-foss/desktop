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

#include "testlog.h"
#include <kapps_core/src/logger.h>
#include <QMessageLogger>
#include <string>
#include <iostream>

// Logging sink for unit tests.
//
// In the regular product code, Logger hooks up to both the Qt logging sink and
// the kapps::core logging sink, and sends messages from both sources to the
// logs.
//
// In unit tests, we use the default Qt logging sink, but forward messages from
// the kapps::core logger to the Qt logger.  This entails a lot of UTF-8/16
// conversion, but this shouldn't really matter in unit tests, and it allows us
// to continue using QTest::ignoreMessage() normally.
namespace testlog {

KAPPS_CORE_LOG_MODULE(tests, "src/testlog.cpp")

class TestLogCallback final : public kapps::core::LogCallback
{
public:
    virtual void write(kapps::core::LogMessage msg) override
    {
        // Qt needs a null-terminated file and category names, copy them
        std::string file{msg.loc().file().begin(), msg.loc().file().end()};
        std::string category{msg.category().name().begin(), msg.category().name().end()};
        QMessageLogger logger{file.c_str(), msg.loc().line(),
                              category.c_str()};
        switch(msg.level())
        {
            default:
            case kapps::core::LogMessage::Level::Fatal:
                logger.fatal("%s", msg.message().c_str());
                break;
            case kapps::core::LogMessage::Level::Error:
                logger.critical("%s", msg.message().c_str());
                break;
            case kapps::core::LogMessage::Level::Warning:
                logger.warning("%s", msg.message().c_str());
                break;
            case kapps::core::LogMessage::Level::Info:
                logger.info("%s", msg.message().c_str());
                break;
            case kapps::core::LogMessage::Level::Debug:
                logger.debug("%s", msg.message().c_str());
                break;
        }
    }
};

void initialize()
{
    std::cerr << "Test logger initializing" << std::endl;
    kapps::core::log::enableLogging(true);
    kapps::core::log::init(std::make_shared<TestLogCallback>());
}

// Initialize the logger automatically so every unit test doesn't have to do it.
// (Each unit test has its own main(), and many use the default main() from
// QTest.)
class Initializer
{
public:
    Initializer() {initialize();}
} init;

// Relying on the static initializer above to initialize the logging sink is
// unfortunately tricky due to unit tests using a static library.
//
// Ideally we might use a dynamic library instead, but this is tricky on
// Windows since exports normally have to be annotated explicitly.  (CMake has a
// hack to generate a module definition file automatically by inspecting the
// object files before linking, we might be able to do this.)
//
// So we need to link in a static library and ensure that this object is
// referenced.  With gcc/clang we could pass --whole-archive to the linker, but
// on Windows, link.exe consumes outrageous amounts of memory trying to link
// in this static library with /WHOLEARCHIVE (it often runs out of heap space
// and fails entirely).
//
// So instead, we can tell the linker to assume a particular symbol is undefined
// in order to force inclusion of this object file.  This also works on
// clang/gcc, so we can do the same thing there.  We use a C-linkage symbol to
// dodge name mangling (although MSVC still slightly mangles C names on x86).
extern "C"
{
    void forceLinkTestlogCpp(){}
}

}
