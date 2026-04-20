#!/bin/bash
set -e

SDK_ROOT="${SDK_ROOT:-/opt/fslc-xwayland/5.15-scarthgap}"
SDK_ENV="$SDK_ROOT/environment-setup-cortexa53-fslc-linux"
SDK_CMAKE="$SDK_ROOT/sysroots/x86_64-fslcsdk-linux/usr/bin/cmake"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: build.sh <target> [options]

Targets:
  debug       Cross-compile Debug build (default)
  release     Cross-compile Release build (-Os, LTO)
  sanitize    Cross-compile Debug build with ASan + UBSan
  clean       Remove all build directories

Options:
  --speed           Optimize for speed (-O2) instead of size (-Os)
  --uint64          Use uint64 version type instead of string
  --lib <build_dir> Use locally built fs-updater-lib from this build directory
EOF
    exit 1
}

TARGET=""
LIB_BUILD_DIR=""
EXTRA_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
    --speed)   EXTRA_ARGS+=("-DOPTIMIZE_FOR=SPEED") ;;
    --uint64)  EXTRA_ARGS+=("-Dupdate_version_type=uint64") ;;
    --lib)     LIB_BUILD_DIR="$(realpath "$2")"; shift ;;
    debug | release | sanitize | clean)
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

build_cross() {
    local build_dir="$PROJECT_ROOT/build"
    local cmake_args=("$@")

    unset LD_LIBRARY_PATH
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
clean)
    rm -rf "$PROJECT_ROOT/build"
    echo "Build directory removed."
    ;;
*)
    usage
    ;;
esac
