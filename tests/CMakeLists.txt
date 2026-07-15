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
    test_arg_scan.cpp
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

# The golden files are read at run time, so the test binary is told where they
# live rather than the expectations being copied into it.
target_compile_definitions(fs_updater_cli_parse_tests PRIVATE
    FSUP_CORPUS_FILE="${CMAKE_CURRENT_SOURCE_DIR}/golden/argv_corpus.txt"
    FSUP_GOLDEN_HELP="${CMAKE_CURRENT_SOURCE_DIR}/golden/help.txt"
)

target_link_libraries(fs_updater_cli_parse_tests PRIVATE GTest::gtest_main)

add_test(NAME fs_updater_cli_parse_tests COMMAND fs_updater_cli_parse_tests)
