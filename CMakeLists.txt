cmake_minimum_required(VERSION 3.5)
# set the project name
project(fs_updater_cli)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_CXX_FLAGS "-Wall -Wextra -Os -s")
#set(CMAKE_CXX_FLAGS "-Wall -Wextra -O -g1 -ggdb3")

include(GNUInstallDirs)

set(SOURCES
    "src/main.cpp"
    "src/cli/cli.cpp"
    "src/cli/SynchronizedSerial.cpp"
    "src/logger/LoggerSinkSerial.cpp"
)

set(HEADERS
    "src/cli/fs_updater_error.h"
)

set(update_version_type "uint64" CACHE STRING "data type fw, app version")


if("${update_version_type}" STREQUAL "string")
        add_definitions (-DUPDATE_VERSION_TYPE_STRING)
endif ()

if("${update_version_type}" STREQUAL "uint64")
        add_definitions (-DUPDATE_VERSION_TYPE_UINT64)
endif ()

find_library(inicpp_lib NAMES libinicpp.a libinicpp.so)
find_library(ubootenv_lib NAMES libubootenv.so)
find_library(z_lib NAMES libz.so)
find_library(botan_lib NAMES libbotan-2.so)
# Do not use libpthread.a if you do not link against a static version of glibc and link botan against a static version of glibc.
find_library(pthread_lib NAMES libpthread.so)
find_library(jsoncpp_lib NAMES libjsoncpp_static.a libjsoncpp.so)
find_library(fs_updater_lib NAMES libfs_updater.a libfs_updater.so)
find_library(archiv_lib NAMES libarchive.so)

message("Path to library inicpp: ${inicpp_lib}")
message("Path to library ubootenv: ${ubootenv_lib}")
message("Path to library zlib: ${z_lib}")
message("Path to library botan: ${botan_lib}")
message("Path to library libarchiv: ${archiv_lib}")
message("Path to library pthread: ${pthread_lib}")
message("Path to library jsoncpp: ${jsoncpp_lib}")
message("Path to library fs-updater: ${fs_updater_lib}")


add_executable(fs_updater_cli ${SOURCES})
target_link_libraries(fs_updater_cli 
        ${fs_updater_lib}
        ${jsoncpp_lib}
        ${inicpp_lib}
        ${botan_lib}
        ${pthread_lib}
        ${ubootenv_lib}
        ${z_lib}
        ${archiv_lib}
      )

set_target_properties(fs_updater_cli PROPERTIES OUTPUT_NAME "fs-updater")
install(TARGETS fs_updater_cli RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})
