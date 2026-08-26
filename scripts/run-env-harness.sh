#!/bin/sh
# Build both halves against a fixture of their own, then run the environment
# harness against them.
#
# Doing this by hand is two builds with the same path spelled three times, and
# the path is compiled in rather than passed: the default-environment
# constructor lives in the library, so a client built against one fixture and a
# library built against another produce a run that seeds one file and measures
# the other. The harness refuses that, but only after the builds are already
# wrong. This does it in one step instead.
#
# Usage: run-env-harness.sh [fixture-dir]
#
#   SDK_ROOT   required -- the cross SDK both halves are built with
#   LIB_REPO   the library checkout (default: the sibling fs-updater-lib)
#
# The fixture is a working directory, not a source artefact: it holds the
# environment image the cases seed and the configuration pointing at it, and it
# can be thrown away between runs.
set -eu

: "${SDK_ROOT:?set SDK_ROOT to the cross SDK the binaries are built with}"
export SDK_ROOT

HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
CLI_REPO=$(CDPATH='' cd -- "$HERE/.." && pwd)
LIB_REPO=${LIB_REPO:-$CLI_REPO/../fs-updater-lib}

[ -x "$LIB_REPO/scripts/build.sh" ] || {
    echo "harness: no library checkout at $LIB_REPO -- set LIB_REPO" >&2
    exit 2
}
LIB_REPO=$(CDPATH='' cd -- "$LIB_REPO" && pwd)

FIXTURE=${1:-$CLI_REPO/build-harness-fixture}
mkdir -p "$FIXTURE"
FIXTURE=$(CDPATH='' cd -- "$FIXTURE" && pwd)
CONFIG="$FIXTURE/fw_env.config"

echo "harness: building the library against $CONFIG"
( cd "$LIB_REPO" && ./scripts/build.sh debug --env-config "$CONFIG" )

echo "harness: building the client against the same fixture"
( cd "$CLI_REPO" && ./scripts/build.sh debug --lib "$LIB_REPO/build" --env-config "$CONFIG" )

echo "harness: running the cases"
exec "$CLI_REPO/tests/harness/run.sh" \
    "$FIXTURE" "$CLI_REPO/build/fs-updater" "$LIB_REPO/build" "$SDK_ROOT"
