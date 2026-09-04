cmake_minimum_required(VERSION 3.18)
project(fs_updater_cli VERSION 2.2.9 LANGUAGES CXX)

include(GNUInstallDirs)

# ==============================================================================
# Options
# ==============================================================================

set(OPTIMIZE_FOR "SIZE" CACHE STRING "Release optimization: SIZE or SPEED")
set_property(CACHE OPTIMIZE_FOR PROPERTY STRINGS SIZE SPEED)

set(update_version_type "string" CACHE STRING "Data type for fw/app version")

set(FUS_LIB_DIR "" CACHE PATH "Local fs-updater-lib install prefix (overrides SDK)")

# Source identity reported by --version. The project version moves rarely, so on
# its own it cannot tell two builds apart; a recipe passes the revision it pinned
# (-DFUS_SOURCE_ID=${SRCREV}). Falls back to the working tree's own description,
# and to "unknown" outside a checkout, so the field is never silently empty.
set(FUS_SOURCE_ID "" CACHE STRING "Source revision baked into --version")
if(NOT FUS_SOURCE_ID)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --always --dirty
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE FUS_SOURCE_ID
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
endif()
if(NOT FUS_SOURCE_ID)
    set(FUS_SOURCE_ID "unknown")
endif()

option(BUILD_DBUS_SUPPORT "Build cloud-flow CLI handlers as D-Bus clients" ON)
option(BUILD_TESTING      "Build unit tests (native build)"                  OFF)
option(BUILD_MAIN_TARGET  "Build CLI executable (OFF = tests only)"          ON)
option(FSUP_WERROR        "Treat compiler warnings as errors (-Werror)"      ON)
option(ENABLE_SANITIZERS  "Enable ASan and UBSan (native test builds)"       OFF)
option(BUILD_FUZZING      "Build the libFuzzer target (needs clang)"         OFF)

# Validate options
if(NOT OPTIMIZE_FOR MATCHES "^(SIZE|SPEED)$")
    message(FATAL_ERROR "OPTIMIZE_FOR must be SIZE or SPEED, got: ${OPTIMIZE_FOR}")
endif()

if("${update_version_type}" STREQUAL "string")
    set(UPDATE_VERSION_TYPE_STRING 1)
    set(UPDATE_VERSION_TYPE_UINT64 0)
elseif("${update_version_type}" STREQUAL "uint64")
    set(UPDATE_VERSION_TYPE_STRING 0)
    set(UPDATE_VERSION_TYPE_UINT64 1)
else()
    message(FATAL_ERROR "Invalid update_version_type: ${update_version_type}")
endif()

# Override CMake's default Release flags (-O3 -DNDEBUG) to avoid conflicting -O levels.
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "" FORCE)

# Global so the test binaries (and fetched gtest) share the sanitizer runtime.
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ==============================================================================
# Generated config header
# ==============================================================================

configure_file(cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# ==============================================================================
# Tests-only mode (skip CLI executable when cross-compile libs unavailable)
# ==============================================================================

# The fuzz target shares this early return: like the tests it needs neither the
# CLI executable nor the cross-compile libraries, and building it through the
# ordinary path would drag both in.
if((BUILD_TESTING OR BUILD_FUZZING) AND NOT BUILD_MAIN_TARGET)
    if(BUILD_TESTING)
        enable_testing()
        add_subdirectory(tests)
    endif()
    if(BUILD_FUZZING)
        add_subdirectory(fuzz)
    endif()
    return()
endif()

# ==============================================================================
# Dependencies
# ==============================================================================

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(BOTAN2 REQUIRED botan-2)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# ==============================================================================
# Sources
# ==============================================================================

set(SOURCES
    src/main.cpp
    src/cli/cli.cpp
    src/cli/cli_classify.cpp
    src/cli/cli_args.cpp
    src/cli/SynchronizedSerial.cpp
    src/logger/LoggerSinkSerial.cpp
)

set(HEADERS
    src/cli/fs_updater_error.h
)

# ==============================================================================
# Target
# ==============================================================================

add_executable(fs_updater_cli ${SOURCES})

target_compile_features(fs_updater_cli PRIVATE cxx_std_17)
set_target_properties(fs_updater_cli PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "fs-updater"
)

# ==============================================================================
# Compiler and linker flags
#
# Deviation: -fno-exceptions and -fno-rtti are NOT applied.
# fs-updater-lib throws exceptions; CLI must catch. RTTI now has no user of its
# own here and is only still on because the flags are flipped together, in the
# slice that makes the lib shim the last exception island.
# ==============================================================================

target_compile_options(fs_updater_cli PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    # The cli is warning-clean. Gated so a toolchain bump can be
    # unblocked with -DFSUP_WERROR=OFF.
    $<$<BOOL:${FSUP_WERROR}>:-Werror>
    # Debug
    $<$<CONFIG:Debug>:-Og -g>
    # Release optimizations
    $<$<CONFIG:Release>:$<IF:$<STREQUAL:${OPTIMIZE_FOR},SPEED>,-O2,-Os>>
    $<$<CONFIG:Release>:-flto>
    $<$<CONFIG:Release>:-ffunction-sections>
    $<$<CONFIG:Release>:-fdata-sections>
)

target_link_options(fs_updater_cli PRIVATE
    $<$<CONFIG:Release>:-Wl,--gc-sections>
    $<$<CONFIG:Release>:-flto>
    $<$<CONFIG:Release>:-s>
)

# ==============================================================================
# Include directories
# ==============================================================================

target_include_directories(fs_updater_cli PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    src/cli
    ${BOTAN2_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

if(FUS_LIB_DIR)
    target_include_directories(fs_updater_cli PRIVATE ${FUS_LIB_DIR}/include)
    target_link_directories(fs_updater_cli PRIVATE ${FUS_LIB_DIR}/lib)
endif()

# ==============================================================================
# Link libraries
# ==============================================================================

target_link_libraries(fs_updater_cli PRIVATE
    fs_updater
    ${BOTAN2_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
    ubootenv
    z
)

# ==============================================================================
# D-Bus cloud-flow support (optional)
# ==============================================================================

if(BUILD_DBUS_SUPPORT)
    pkg_check_modules(LIBSYSTEMD REQUIRED libsystemd)
    target_compile_definitions(fs_updater_cli PRIVATE BUILD_DBUS_SUPPORT=1)
    target_include_directories(fs_updater_cli PRIVATE ${LIBSYSTEMD_INCLUDE_DIRS})
    target_link_libraries(fs_updater_cli PRIVATE ${LIBSYSTEMD_LIBRARIES})
    target_sources(fs_updater_cli PRIVATE src/cli/fus_dbus_client.cpp)
endif()

# Which fw_env.config this binary opens. Empty leaves the header's constant
# standing, so an ordinary build is unchanged; a harness sets it to drive the
# binary against a prepared environment instead of a device's. The library
# carries the same option for the units it compiles itself.
set(UBOOT_CONFIG_PATH "" CACHE STRING "fw_env.config path compiled into this binary")
if(UBOOT_CONFIG_PATH)
    target_compile_definitions(fs_updater_cli PRIVATE UBOOT_CONFIG_PATH="${UBOOT_CONFIG_PATH}")
endif()

# ==============================================================================
# Install
# ==============================================================================

install(TARGETS fs_updater_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})

# ==============================================================================
# Tests
# ==============================================================================

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
