# libFuzzer target for the argument parser. Requires clang -- GCC has no
# -fsanitize=fuzzer equivalent. Only reached through `./scripts/build.sh fuzz`,
# which configures a separate build_fuzz/ directory with clang; BUILD_FUZZING
# stays OFF for the ordinary gcc-based `test` build, so this directory is
# genuinely additive and never affects the existing native test path.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "BUILD_FUZZING requires clang (got ${CMAKE_CXX_COMPILER_ID}); "
        "use ./scripts/build.sh fuzz, which sets this up.")
endif()

set(FUZZ_SANITIZE_FLAGS -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)

# Exactly the two sources the parse test links -- the parser and the classifier
# it calls. Nothing here touches D-Bus or the library, so the fuzz build needs
# neither, and keeping the set this small is what makes the target quick enough
# to run on every commit.
foreach(target_name IN ITEMS fuzz_cli_args)
    add_executable(${target_name}
        ${target_name}.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/cli_args.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/cli_classify.cpp
    )
    target_compile_features(${target_name} PRIVATE cxx_std_17)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_options(${target_name} PRIVATE -Wall -Wextra ${FUZZ_SANITIZE_FLAGS})
    target_link_options(${target_name} PRIVATE ${FUZZ_SANITIZE_FLAGS})
endforeach()
