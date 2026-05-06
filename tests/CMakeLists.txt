include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic)

set(TEST_SOURCES
    test_fs_updater_error.cpp
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
