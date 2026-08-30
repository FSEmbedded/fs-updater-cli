#!/bin/bash
set -e

SDK_ROOT="${SDK_ROOT:-/opt/fslc-xwayland/5.15-scarthgap}"
SDK_ENV="$SDK_ROOT/environment-setup-cortexa53-fslc-linux"
SDK_CMAKE="$SDK_ROOT/sysroots/x86_64-fslcsdk-linux/usr/bin/cmake"
SDK_CTEST="$SDK_ROOT/sysroots/x86_64-fslcsdk-linux/usr/bin/ctest"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: build.sh <target> [options]

Targets:
  debug       Cross-compile Debug build (default)
  release     Cross-compile Release build (-Os, LTO)
  sanitize    Cross-compile Debug build with ASan + UBSan
  test        Native build + run unit tests
  fuzz        Native libFuzzer build (requires clang) + a short smoke run
              per target against fuzz/seed_corpus/; for an actual fuzzing
              session run the built binary directly, e.g.
              build_fuzz/fuzz/fuzz_cli_args -max_total_time=300
              build_fuzz/corpus_cli_args fuzz/seed_corpus/cli_args
              (new inputs land in the first directory, never in
              seed_corpus/)
  clean       Remove all build directories

Options:
  --speed           Optimize for speed (-O2) instead of size (-Os)
  --uint64          Use uint64 version type instead of string
  --lib <build_dir> Use locally built fs-updater-lib from this build directory
  --no-dbus         Disable D-Bus cloud-flow handlers (BUILD_DBUS_SUPPORT=OFF).
                    On this branch D-Bus is the default; opt out only when
                    the lib is also built --no-dbus.
  --sanitize        With 'test': run the native suite under ASan/UBSan
                    (separate build_test_san/ dir; cross builds keep the
                    'sanitize' target).
  --env-config <path>
                    Build against another fw_env.config instead of the
                    device's. For test harnesses; the default is unchanged
                    without it.
EOF
    exit 1
}

TARGET=""
LIB_BUILD_DIR=""
EXTRA_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
    --speed)   EXTRA_ARGS+=("-DOPTIMIZE_FOR=SPEED") ;;
    --sanitize) EXTRA_ARGS+=("-DENABLE_SANITIZERS=ON"); TEST_SUFFIX="_san" ;;
    --uint64)  EXTRA_ARGS+=("-Dupdate_version_type=uint64") ;;
    --lib)     LIB_BUILD_DIR="$(realpath "$2")"; shift ;;
    --no-dbus) EXTRA_ARGS+=("-DBUILD_DBUS_SUPPORT=OFF") ;;
    --env-config)
        # Point the build at another fw_env.config. For a harness that drives
        # this binary against a prepared environment instead of the device's;
        # the default is baked in and unchanged without this flag.
        EXTRA_ARGS+=("-DUBOOT_CONFIG_PATH=$(realpath -m "$2")"); shift ;;
    debug | release | sanitize | test | fuzz | clean)
        if [ -n "$TARGET" ]; then
            echo "Multiple targets specified: $TARGET and $1"
            usage
        fi
        TARGET="$1"
        ;;
    *)
        echo "Unknown option: $1"
        usage
        ;;
    esac
    shift
done

TARGET="${TARGET:-debug}"

if [ -n "${TEST_SUFFIX:-}" ] && [ "$TARGET" != "test" ]; then
    echo "--sanitize applies to the 'test' target only (cross builds: use the 'sanitize' target)"
    exit 1
fi

build_cross() {
    local build_dir="$PROJECT_ROOT/build"
    local cmake_args=("$@")

    unset LD_LIBRARY_PATH
    # Without this the failure is a bare "No such file or directory" from
    # `source`, which says neither that the location is configurable nor that
    # only the cross targets need it.
    [ -f "$SDK_ENV" ] || { echo "cross build needs the SDK: $SDK_ENV not found — set SDK_ROOT, or use the 'test' target for a host build"; exit 1; }
    source "$SDK_ENV"

    if [ -n "$LIB_BUILD_DIR" ]; then
        local lib_install="$PROJECT_ROOT/build/fus_lib_install"
        echo "Installing fs-updater-lib from $LIB_BUILD_DIR..."
        "$SDK_CMAKE" --install "$LIB_BUILD_DIR" --prefix "$lib_install"
        cmake_args+=("-DFUS_LIB_DIR=$lib_install")
    fi

    mkdir -p "$build_dir" && cd "$build_dir"
    "$SDK_CMAKE" "${cmake_args[@]}" "$PROJECT_ROOT"
    make -j"$(nproc)"
}

build_test() {
    local build_dir="$PROJECT_ROOT/build_test${TEST_SUFFIX:-}"
    local cmake_args=("$@")

    # Prefer SDK cmake/ctest; fall back to system cmake/ctest if SDK not present
    local cmake_bin="$SDK_CMAKE"
    local ctest_bin="$SDK_CTEST"
    if [ ! -x "$cmake_bin" ]; then
        cmake_bin="$(command -v cmake 2>/dev/null)" || { echo "cmake not found"; exit 1; }
        ctest_bin="$(command -v ctest 2>/dev/null)" || ctest_bin="$cmake_bin --build . --target test"
    fi

    mkdir -p "$build_dir" && cd "$build_dir"
    "$cmake_bin" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DBUILD_TESTING=ON \
        -DBUILD_MAIN_TARGET=OFF \
        "${cmake_args[@]}" \
        "$PROJECT_ROOT"
    make -j"$(nproc)"
    "$ctest_bin" --output-on-failure
}

build_fuzz() {
    local build_dir="$PROJECT_ROOT/build_fuzz"
    local cmake_args=("$@")

    command -v clang++ >/dev/null 2>&1 || {
        echo "clang++ not found — libFuzzer needs clang (gcc has no -fsanitize=fuzzer)"
        exit 1
    }

    # Same SDK-first-then-fallback resolution as build_test(): cmake does not
    # care which compiler it is told to drive.
    local cmake_bin="$SDK_CMAKE"
    if [ ! -x "$cmake_bin" ]; then
        cmake_bin="$(command -v cmake 2>/dev/null)" || { echo "cmake not found"; exit 1; }
    fi

    # Neither the parser nor the classifier touches D-Bus, so this build needs
    # none of the cross libraries the CLI executable does.
    mkdir -p "$build_dir" && cd "$build_dir"
    "$cmake_bin" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DBUILD_TESTING=OFF \
        -DBUILD_MAIN_TARGET=OFF \
        -DBUILD_FUZZING=ON \
        -DBUILD_DBUS_SUPPORT=OFF \
        "${cmake_args[@]}" \
        "$PROJECT_ROOT"
    make -j"$(nproc)"

    echo
    echo "=== fuzz smoke run (10s/target against the seed corpus) ==="
    # Derived from the sources rather than listed here: a target added to
    # fuzz/CMakeLists.txt but forgotten in this loop would be built and never
    # run, which is the quiet half of a gate that does not gate.
    local t work_corpus
    for src in "$PROJECT_ROOT"/fuzz/fuzz_*.cpp; do
        t="$(basename "$src" .cpp)"
        echo "--- $t ---"
        # The first positional directory is libFuzzer's read-write corpus;
        # findings land there, never in the curated seed corpus under git.
        work_corpus="$build_dir/corpus_${t#fuzz_}"
        mkdir -p "$work_corpus"
        "./fuzz/$t" -max_total_time=10 "$work_corpus" \
            "$PROJECT_ROOT/fuzz/seed_corpus/${t#fuzz_}" \
            || { echo "$t: FAILED (see output above)"; exit 1; }
    done
    echo "fuzz smoke run: PASS"
}

case "$TARGET" in
debug)
    build_cross -DCMAKE_BUILD_TYPE=Debug "${EXTRA_ARGS[@]}"
    ;;
release)
    build_cross -DCMAKE_BUILD_TYPE=Release "${EXTRA_ARGS[@]}"
    ;;
sanitize)
    build_cross -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all" \
        "${EXTRA_ARGS[@]}"
    ;;
test)
    build_test "${EXTRA_ARGS[@]}"
    ;;
fuzz)
    build_fuzz "${EXTRA_ARGS[@]}"
    ;;
clean)
    rm -rf "$PROJECT_ROOT/build" "$PROJECT_ROOT/build_test" "$PROJECT_ROOT/build_test_san" \
        "$PROJECT_ROOT/build_fuzz"
    echo "Build directories removed."
    ;;
*)
    usage
    ;;
esac
