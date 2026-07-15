find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

set(TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic)

set(TEST_SOURCES
    test_fs_updater_error.cpp
    test_cli_classify.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/cli_classify.cpp
)

add_executable(fs_updater_cli_tests ${TEST_SOURCES})

target_compile_features(fs_updater_cli_tests PRIVATE cxx_std_17)
set_target_properties(fs_updater_cli_tests PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(fs_updater_cli_tests PRIVATE ${TEST_COMPILE_OPTIONS})

target_include_directories(fs_updater_cli_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(fs_updater_cli_tests PRIVATE GTest::gtest_main)

add_test(NAME fs_updater_cli_tests COMMAND fs_updater_cli_tests)

# ---------------------------------------------------------------------------
# Parse tests
#
# Kept a separate target so the one above stays free of the argument library:
# that one tests pure logic and must keep linking nothing but gtest.
# ---------------------------------------------------------------------------

# The argument library is header-only but is not installed natively. Take it
# from the same sysroot the cross build compiles against — a vendored copy could
# drift in version, and since it renders the usage text itself, the golden would
# then pin bytes the shipped binary never prints.
#
# Two sysroot conventions to cover: the SDK_ROOT glob below serves the native
# test flow (scripts/build.sh test), which invokes CMake directly with no
# toolchain file. The Yocto recipe build (bitbake do_configure) instead sets
# CMAKE_TOOLCHAIN_FILE, which points CMAKE_FIND_ROOT_PATH at recipe-sysroot —
# SDK_ROOT is not part of that environment, so find_path() is the fallback
# that actually resolves there (tclap is already populated in that sysroot).
if(NOT DEFINED TCLAP_INCLUDE)
    file(GLOB _sysroot_includes "$ENV{SDK_ROOT}/sysroots/*/usr/include")
    foreach(_candidate ${_sysroot_includes})
        if(EXISTS "${_candidate}/tclap/CmdLine.h")
            set(TCLAP_INCLUDE "${_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT TCLAP_INCLUDE OR NOT EXISTS "${TCLAP_INCLUDE}/tclap/CmdLine.h")
    find_path(TCLAP_INCLUDE NAMES tclap/CmdLine.h)
endif()

if(NOT TCLAP_INCLUDE OR NOT EXISTS "${TCLAP_INCLUDE}/tclap/CmdLine.h")
    message(FATAL_ERROR
        "tclap/CmdLine.h not found. Set SDK_ROOT so it can be located in the SDK "
        "sysroot, or pass -DTCLAP_INCLUDE=<dir containing tclap/CmdLine.h>.")
endif()

# Stage just the one subdirectory. The sysroot's include root also holds the
# target's libc headers, and putting that on a native compile's search path
# breaks it — the copy is refreshed from the sysroot on every configure, so it
# is the same version the cross build uses without dragging the rest along.
set(TCLAP_STAGE "${CMAKE_CURRENT_BINARY_DIR}/tclap-include")
file(REMOVE_RECURSE "${TCLAP_STAGE}")
file(COPY "${TCLAP_INCLUDE}/tclap" DESTINATION "${TCLAP_STAGE}")

add_executable(fs_updater_cli_parse_tests
    test_cli_parse.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/cli_args.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/cli_classify.cpp
)

target_compile_features(fs_updater_cli_parse_tests PRIVATE cxx_std_17)
set_target_properties(fs_updater_cli_parse_tests PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(fs_updater_cli_parse_tests PRIVATE ${TEST_COMPILE_OPTIONS})

target_include_directories(fs_updater_cli_parse_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_include_directories(fs_updater_cli_parse_tests SYSTEM PRIVATE ${TCLAP_STAGE})

# The golden files are read at run time, so the test binary is told where they
# live rather than the expectations being copied into it.
target_compile_definitions(fs_updater_cli_parse_tests PRIVATE
    FSUP_CORPUS_FILE="${CMAKE_CURRENT_SOURCE_DIR}/golden/argv_corpus.txt"
    FSUP_GOLDEN_HELP="${CMAKE_CURRENT_SOURCE_DIR}/golden/help.txt"
)

target_link_libraries(fs_updater_cli_parse_tests PRIVATE GTest::gtest_main)

add_test(NAME fs_updater_cli_parse_tests COMMAND fs_updater_cli_parse_tests)
