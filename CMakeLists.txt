cmake_minimum_required(VERSION 3.18)
project(fs_updater_cli VERSION 1.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

# ==============================================================================
# Options
# ==============================================================================

set(OPTIMIZE_FOR "SIZE" CACHE STRING "Release optimization: SIZE or SPEED")
set_property(CACHE OPTIMIZE_FOR PROPERTY STRINGS SIZE SPEED)

set(update_version_type "string" CACHE STRING "Data type for fw/app version")

set(FUS_LIB_DIR "" CACHE PATH "Local fs-updater-lib install prefix (overrides SDK)")

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

# ==============================================================================
# Generated config header
# ==============================================================================

configure_file(cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# ==============================================================================
# Dependencies
# ==============================================================================

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(BOTAN2 REQUIRED botan-2)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

# ==============================================================================
# Sources
# ==============================================================================

set(SOURCES
    src/main.cpp
    src/cli/cli.cpp
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
# fs-updater-lib throws exceptions; CLI must catch.
# TCLAP (header-only) requires RTTI.
# ==============================================================================

target_compile_options(fs_updater_cli PRIVATE
    -Wall
    -Wextra
    -Wpedantic
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
    ${LIBARCHIVE_INCLUDE_DIRS}
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
    ${LIBARCHIVE_LIBRARIES}
    Threads::Threads
    ubootenv
    z
)

# ==============================================================================
# Install
# ==============================================================================

install(TARGETS fs_updater_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})
