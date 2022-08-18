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
# Qt Creator defaults to the "all" target, which is hooked up to the default
# target in Rake since the true "all" target builds _everything_ including all
# unit tests, etc.
#
# However, this means that actually running unit tests (etc.) from Qt Creator
# won't build the right target.  To have Qt Creator build the correct target
# for the currently selected executable:
#  - Go to the Projects page
#  - Select "Build" under "current kit"
#  - Under "Build Steps", expand the CMake build step
#  - Select "Current Executable" instead of "all"
# Now Qt Creator will build whatever target you have chosen to run.
#
# Some ancillary targets (Windows installer, Mac OpenVPN helpers, etc.) aren't
# modeled in CMake currently (but could be added if needed).

cmake_minimum_required(VERSION 3.10)
project(pia_desktop)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOGEN OFF) # suppress CMake moc/rcc rules
find_package(Qt5 COMPONENTS Core Quick Network Qml Quick QuickControls2 Gui Test REQUIRED)

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

set_for_platform(RAKE "rake.bat" "rake" "rake")

include_directories(
    ./
    kapps_core/api/
    kapps_net/api/
    kapps_regions/api/
    out/${BRAND}_${RAKE_VARIANT}_x86_64/probe-version
    deps/jsonmcpp/include/
    deps/embeddable-wg-library/src/
    deps/lzma/src/
)

function(rake_target cmakeName rakeName)
    # Qt Creator now seems to set a PLATFORM variable that conflicts with the
    # one expected by our build system, clear it when running rake
    add_custom_target(${cmakeName} ${CMAKE_COMMAND} -E env "RUBYOPT=-Eutf-8" "PLATFORM=" ${RAKE} -j16 VARIANT=${RAKE_VARIANT} BRAND=${BRAND} ${rakeName}
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
set_for_platform(RAKE_STAGE_LIB
                 "${RAKE_OUT}/stage"
                 "${RAKE_OUT}/stage/${BRAND_NAME}.app/Contents/Frameworks"
                 "${RAKE_OUT}/stage/lib"
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

function(source_files name sourceDirectory)
    aux_source_directory("${sourceDirectory}/src" TARGET_SOURCE_GENERAL_${name})
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        aux_source_directory("${sourceDirectory}/src/win" TARGET_SOURCE_PLATFORM_${name})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        aux_source_directory("${sourceDirectory}/src/mac" TARGET_SOURCE_PLATFORM_${name})
        aux_source_directory("${sourceDirectory}/src/posix" TARGET_SOURCE_POSIX_${name})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        aux_source_directory("${sourceDirectory}/src/linux" TARGET_SOURCE_PLATFORM_${name})
        aux_source_directory("${sourceDirectory}/src/posix" TARGET_SOURCE_POSIX_${name})
    endif()
    set(TARGET_SOURCE_${name} ${TARGET_SOURCE_GENERAL_${name}}
        ${TARGET_SOURCE_PLATFORM_${name}} ${TARGET_SOURCE_POSIX_${name}}
        PARENT_SCOPE)
endfunction()

# Define a library target in the stage directory
function(stage_lib name sourceDirectory libName)
    source_files(${name} ${sourceDirectory})
    add_library(${name} SHARED ${TARGET_SOURCE_${name}})
    set_for_platform(TARGET_FILE "${libName}.dll" "${libName}.dylib" "${libName}.so")
    set_property(TARGET ${name} PROPERTY OUTPUT_NAME ${TARGET_FILE})
    add_dependencies(${name} stage)
    set_property(TARGET ${name} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${RAKE_STAGE_LIB})
    list(APPEND ALL_LIB_TARGETS ${name})
endfunction()

# Define a kapps library (these have a consistent convention for target name,
# source directory, and lib name, the few non-kapps libraries aren't all
# consistent)
function(stage_kapps_lib kappsName)
    stage_lib("kapps_${kappsName}" "kapps_${kappsName}" "kapps_${kappsName}")
endfunction()

# Define an executable target in the stage directory
function(stage_target name sourceDirectory execWin execMac execLinux)
    source_files(${name} ${sourceDirectory})
    add_executable(${name} EXCLUDE_FROM_ALL "${TARGET_SOURCE_${name}}")
    set_for_platform(TARGET_FILE ${execWin} ${execMac} ${execLinux})
    set_property(TARGET ${name} PROPERTY OUTPUT_NAME ${TARGET_FILE})
    add_dependencies(${name} stage)
    set_property(TARGET ${name} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${RAKE_STAGE_BIN})
    # Let Qt Creator assume that all executables depend on all libraries.  This
    # is crude but is sufficient for the code model to work.
    target_link_libraries(${name} ${ALL_LIB_TARGETS})
endfunction()

# Define all libraries
stage_kapps_lib(core)
stage_kapps_lib(net)
stage_kapps_lib(regions)
stage_lib(common common "${BRAND}-commonlib")
stage_lib(clientlib clientlib "${BRAND}-clientlib")

stage_target(cli cli "${BRAND}ctl" "${BRAND}ctl" "${BRAND}ctl")
target_link_libraries(cli Qt5::Core Qt5::Network)

stage_target(client client "${BRAND}-client" ${BRAND_NAME} "${BRAND}-client")
target_link_libraries(client Qt5::Core Qt5::Network Qt5::Qml Qt5::Quick Qt5::QuickControls2 Qt5::Gui)
# Annoyingly, Qt's CMake support only permits listing resources via a QRC file.
# The rake build system (like qbs) allows picking up resources automatically
# so we don't have to repeat them all in the build script.
# Use the generated QRC file, although this means a build must be performed for
# Qt Creator to find QML sources.
target_sources(client PRIVATE "${RAKE_OUT}/pia-client/qrc_pia-client.qrc")

stage_target(daemon daemon "${BRAND}-service" "${BRAND}-daemon" "${BRAND}-daemon")
target_link_libraries(daemon Qt5::Core Qt5::Network)

source_files(integtest integtest)
add_executable(integtest EXCLUDE_FROM_ALL ${TARGET_SOURCE_integtest})
target_link_libraries(integtest Qt5::Core Qt5::Network Qt5::Test ${ALL_LIB_TARGETS})
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

unit_test("any")
unit_test("apiclient")
unit_test("check")
unit_test("connectionconfig")
unit_test("exec")
unit_test("json")
unit_test("jsonrefresher")
unit_test("jsonrpc")
unit_test("jsonstate")
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
unit_test("regionlist")
unit_test("retainshared")
unit_test("semversion")
unit_test("servicegroup")
unit_test("settings")
unit_test("subnetbypass")
unit_test("tasks")
unit_test("transportselector")
unit_test("updatedownloader")
unit_test("vpnmethod")
unit_test("wireguarduapi")
unit_test("workthread")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    unit_test("wfp_filters")
endif()
