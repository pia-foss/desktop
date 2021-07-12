# This project glues rake to CMake for use in Qt Creator.  This allows Qt
# Creator to build, launch, and debug targets.
#
# This is only intended as glue for Qt Creator.  To build manually, use rake
# directly.
#
# The CMake file models the basic aspects of the rake build:
# - include directories - for Qt Creator's code model
# - referenced Qt modules - for Qt Creator's code model
# - target executables - so they can be built, started, and debugged in Qt
#   Creator with no manual setup
#
# Qt Creator won't be able to locate QML files by default since they aren't
# indicated to CMake.  See README.md for instructions to enable the
# "Files in All Project Directories" locator filter by default.
#
# Qt Creator defaults to the "all" target, which builds everything (including
# tests, exports, installers, artifacts).  To build just the staged installation
# by default instead:
#  - Go to the Projects page
#  - Select "Build" under current kit"
#  - Under "Build Steps", expand the CMake build step
#  - Select a different target instead of "all":
#    - "stage" will always build the staged installation (client/cli/daemon)
#    - "Current executable" will build the current target you have selected in
#      Qt Creator - for client/cli/daemon this builds the staging area, but for
#      integtest or unit tests it will build the test instead
# If you select "stage", Qt Creator will still know how to build tests, etc.
# if they are started or debugged from the IDE.
#
# Some ancillary targets (Windows installer, Mac OpenVPN helpers, etc.) aren't
# modeled in CMake currently (but could be added if needed).
#
# All the source files are _not_ indicated to CMake - Qt Creator is able to find
# them without this, and CMake would spew lots of output pretending to compile
# those files.  Qt Creator's "project" view won't show all the source files;
# just use the "filesystem" view instead, PIA Desktop's source is organized by
# component already.

cmake_minimum_required(VERSION 3.10)
project(pia_desktop)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOGEN OFF) # suppress CMake moc/rcc rules
find_package(Qt5 COMPONENTS Core Quick Network Qml Quick QuickControls2 Gui Test REQUIRED)

include_directories(
    cli/src
    client/src
    clientlib/src
    common/src
    common/src/builtin
    daemon/src
    deps/embeddable-wg-library/src
    extras/installer/mac/helper
    integtest/src
    tests/src
)

file(GLOB CLIENT_RES "client/res/**/*")
file(GLOB SUPPORTTOOL_COMPONENTS "extras/support-tool/components/**/*")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RAKE_VARIANT debug)
else()
    set(RAKE_VARIANT release)
endif()

if(DEFINED ENV{BRAND})
    set(BRAND ENV{BRAND})
else()
    set(BRAND "pia")
endif()

# The brand name is needed on Mac to define the staging root and client target.
# There isn't a JSON parser available in CMake, this command should work for
# most sane JSON.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(COMMAND cat "${CMAKE_SOURCE_DIR}/brands/${BRAND}/brandinfo.json"
                    COMMAND grep "\"brandName\""
                    COMMAND sed "-e" "s/ *\"brandName\": *\"\\(.*\\)\",*/\\1/"
                    COMMAND tr "-d" "\\n"
                    OUTPUT_VARIABLE BRAND_NAME)
else()
    set(BRAND_NAME ${BRAND}) # Dummy value, not used on other platforms
endif()

# Set a variable to a value selected for the current platform from the values
# given
function(set_for_platform name valueWin valueMac valueLinux)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(${name} ${valueWin} PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(${name} ${valueMac} PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(${name} ${valueLinux} PARENT_SCOPE)
    endif()
endfunction()

# Dependency used to always invoke rake, since rake really manages the
# dependencies.
add_custom_command(OUTPUT always_build COMMAND ${CMAKE_COMMAND} -E echo)

set_for_platform(RAKE "rake.cmd" "rake" "rake")

function(rake_target cmakeName rakeName)
    add_custom_target(${cmakeName} ${CMAKE_COMMAND} -E env "RUBYOPT=-Eutf-8" ${RAKE} -j16 VARIANT=${RAKE_VARIANT} BRAND=${BRAND} ${rakeName}
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                      DEPENDS always_build USES_TERMINAL)
endfunction()

# The CMake 'stage' target is hooked up to the Rake 'default' target, because
# the default target is windeploy on Windows (runs windeploy on the staging
# area).
rake_target(stage default)
rake_target(rake-test test)
rake_target(integtest-stage integtest)
rake_target(rake-all all)
# Hook up CMake's 'all' to rake's default only - this is the default target for
# Qt Creator.  It still will know how to build unit tests / integ tests if they
# are started/debugged from the IDE.
set_property(TARGET stage PROPERTY EXCLUDE_FROM_ALL OFF)
# Rake 'clean' target - provided even though we can't hook it up to cmake's clean
rake_target(rake-clean clean)

# Hooking up the executable targets to Qt Creator via CMake is a bit tricky.
#
# CMake has to think it is really building these targets for them to appear in
# the target lists queried by Qt Creator.  (They can't be imported executables,
# etc.)
#
# We don't specify all the source files, but each target does need to specify
# at least one source file to satisfy CMake.
#
# We can suppress all the default compile/link commands by wrapping them with
# 'true', which makes them no-ops.  Then, add a dependency on the 'stage'
# target, which actually builds the executable.  Set the output directory and
# output name to the staged executable.
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E true")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E true")

set(RAKE_OUT "${CMAKE_SOURCE_DIR}/out/${BRAND}_${RAKE_VARIANT}_x86_64")
set_for_platform(RAKE_STAGE_BIN
                 "${RAKE_OUT}/stage"
                 "${RAKE_OUT}/stage/${BRAND_NAME}.app/Contents/MacOS"
                 "${RAKE_OUT}/stage/bin"
                )
set_for_platform(RAKE_INTEGTEST_BIN
                 "${RAKE_OUT}/integtest-stage/${BRAND}-integtest"
                 "${RAKE_OUT}/integtest-stage/${BRAND}-integtest.app/Contents/MacOS"
                 "${RAKE_OUT}/integtest-stage/${BRAND}-integtest/bin"
                )

# Implement the clean target by removing the entire output directory.  There's
# no way in CMake to hook clean up to a custom action, but this is all the rake
# clean task does anyway.
set_property(DIRECTORY PROPERTY ADDITIONAL_CLEAN_FILES ${RAKE_OUT})

function(stage_target name sourceFile execWin execMac execLinux)
    add_executable(${name} EXCLUDE_FROM_ALL ${sourceFile})
    set_for_platform(TARGET_FILE ${execWin} ${execMac} ${execLinux})
    set_property(TARGET ${name} PROPERTY OUTPUT_NAME ${TARGET_FILE})
    add_dependencies(${name} stage)
    set_property(TARGET ${name} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${RAKE_STAGE_BIN})
endfunction()

stage_target(cli cli/src/cli.cpp "${BRAND}ctl" "${BRAND}ctl" "${BRAND}ctl")
target_link_libraries(cli Qt5::Core Qt5::Network)

stage_target(client client/src/main.cpp "${BRAND}-client" ${BRAND_NAME} "${BRAND}-client")
target_link_libraries(client Qt5::Core Qt5::Network Qt5::Qml Qt5::Quick Qt5::QuickControls2 Qt5::Gui)

stage_target(daemon daemon/src/daemon.cpp "${BRAND}-service" "${BRAND}-daemon" "${BRAND}-daemon")
target_link_libraries(daemon Qt5::Core Qt5::Network)

add_executable(integtest EXCLUDE_FROM_ALL integtest/src/integtest.cpp)
target_link_libraries(integtest Qt5::Core Qt5::Network Qt5::Test)
set_property(TARGET integtest PROPERTY OUTPUT_NAME "${BRAND}-integtest")
add_dependencies(integtest integtest-stage)
set_property(TARGET integtest PROPERTY RUNTIME_OUTPUT_DIRECTORY ${RAKE_INTEGTEST_BIN})

function(unit_test name)
    set(TESTNAME "test-${name}")
    rake_target("rake-${TESTNAME}" ${TESTNAME})
    add_executable(${TESTNAME} EXCLUDE_FROM_ALL "tests/tst_${name}.cpp")
    target_link_libraries(${TESTNAME} Qt5::Core Qt5::Qml Qt5::Quick Qt5::QuickControls2 Qt5::Gui Qt5::Network)
    set_property(TARGET ${TESTNAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${RAKE_OUT}/${TESTNAME}")
    add_dependencies(${TESTNAME} "rake-${TESTNAME}")
endfunction()

unit_test("apiclient")
unit_test("check")
unit_test("connectionconfig")
unit_test("exec")
unit_test("json")
unit_test("jsonrefresher")
unit_test("jsonrpc")
unit_test("latencytracker")
unit_test("linebuffer")
unit_test("localsockets")
unit_test("nearestlocations")
unit_test("networkmonitor")
unit_test("networktaskwithretry")
unit_test("nodelist")
unit_test("nullable_t")
unit_test("originalnetworkscan")
unit_test("openssl")
unit_test("path")
unit_test("portforwarder")
unit_test("raii")
unit_test("semversion")
unit_test("settings")
unit_test("subnetbypass")
unit_test("tasks")
unit_test("transportselector")
unit_test("updatedownloader")
unit_test("vpnmethod")
unit_test("wireguarduapi")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    unit_test("wfp_filters")
endif()
