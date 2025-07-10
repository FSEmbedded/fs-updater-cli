cmake_minimum_required(VERSION 3.5)
project(fs_updater_cli VERSION 1.1.0 LANGUAGES CXX)

# Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Use pkg-config for dependencies (Yocto provides .pc files)
pkg_check_modules(BOTAN2 REQUIRED botan-2)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

# Configuration options
set(update_version_type "string" CACHE STRING "Data type for fw/app version")

# Validate version type
if("${update_version_type}" STREQUAL "string")
    add_definitions(-DUPDATE_VERSION_TYPE_STRING)
elseif("${update_version_type}" STREQUAL "uint64")
    add_definitions(-DUPDATE_VERSION_TYPE_UINT64)
else()
    message(FATAL_ERROR "Invalid update_version_type: ${update_version_type}")
endif()

# Sources
set(SOURCES
    src/main.cpp
    src/cli/cli.cpp
    src/cli/SynchronizedSerial.cpp
    src/logger/LoggerSinkSerial.cpp
)

# Headers
set(HEADERS
    src/cli/fs_updater_error.h
)

# Build executable
add_executable(fs_updater_cli ${SOURCES})

# Compiler flags
target_compile_options(fs_updater_cli PRIVATE
    $<$<CONFIG:Debug>:-Wall -Wextra -Og -g>
    $<$<CONFIG:Release>:-Wall -Wextra -O2 -DNDEBUG>
)

# Include directories (Yocto sysroot handles paths automatically)
target_include_directories(fs_updater_cli PRIVATE
    src/cli
    ${BOTAN2_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
)

# Link libraries - let Yocto handle the paths
target_link_libraries(fs_updater_cli PRIVATE
    fs_updater          # Library name without full path
    ${BOTAN2_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    Threads::Threads
    ubootenv
    z
)

# Set properties
set_target_properties(fs_updater_cli PROPERTIES 
    OUTPUT_NAME "fs-updater"
)

# Installation
install(TARGETS fs_updater_cli 
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})