#!/bin/sh
# Drive the shipping client against a prepared U-Boot environment.
#
# Seeds the environment as bytes on disk, in the layout the device uses, and
# runs the real binary against it -- so the derived codes come out of the
# code that ships, not out of a stub.
#
# What it needs, and why the build is two steps: the path to fw_env.config is
# compiled in, and the default-environment constructor lives in the library, so
# both halves have to be built against the same fixture.
#
#   ./scripts/build.sh debug --env-config <fixture>/fw_env.config           (library)
#   ./scripts/build.sh debug --lib <lib>/build --env-config <same path>     (client)
#
# The binary is built for the target, so it runs here under user-mode
# emulation -- the emulator ships with the SDK. Nothing boots, and no kernel
# is involved.
#
# Usage: run.sh <fixture-dir> <client-binary> <lib-build-dir> <sdk-root>
set -u

FIXTURE=${1:?usage: run.sh <fixture-dir> <client-binary> <lib-build-dir> <sdk-root>}
CLIENT=${2:?missing client binary}
LIBDIR=${3:?missing library build dir}
SDK=${4:?missing SDK root}

QEMU="$SDK/sysroots/x86_64-fslcsdk-linux/usr/bin/qemu-aarch64"
SYSROOT="$SDK/sysroots/cortexa53-fslc-linux"
MKENV="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)/mkenv.py"

for f in "$QEMU" "$CLIENT" "$MKENV"; do
    [ -e "$f" ] || { echo "harness: missing $f" >&2; exit 2; }
done

mkdir -p "$FIXTURE"
ENV_BIN="$FIXTURE/env.bin"
printf '%s 0x0000 0x2000\n' "$ENV_BIN" > "$FIXTURE/fw_env.config"

fails=0

seed() { # seed <state> <update-bits> [extra k=v...]
    _state=$1; _bits=$2; shift 2
    python3 "$MKENV" build "$ENV_BIN" \
        update_reboot_state="$_state" update="$_bits" \
        'BOOT_ORDER=A B' 'BOOT_ORDER_OLD=A B' BOOT_A_LEFT=3 BOOT_B_LEFT=3 \
        application=A 'rauc_cmd=rauc.slot=A' 'console=ttymxc1,115200' "$@"
}

client() { # client <args...> -> sets rc, out
    out=$("$QEMU" -L "$SYSROOT" \
        -E LD_LIBRARY_PATH="$LIBDIR:$SYSROOT/usr/lib:$SYSROOT/lib" \
        "$CLIENT" "$@" 2>&1)
    rc=$?
}

expect_rc() { # expect_rc <name> <want> <state> <bits>
    seed "$3" "$4"
    client --update_reboot_state
    if [ "$rc" = "$2" ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1 -- seeded state $3 bits $4: want rc $2, got $rc"
        echo "      $(printf '%s' "$out" | tail -n 1)"
        fails=$((fails + 1))
    fi
}

# The read is total over what the storage can hold: a value outside the
# alphabet must answer the recovery code rather than idle, or a consumer that
# treats only idle as "nothing to do" keeps skipping a device nobody can read.
expect_rc "an unreadable value is not idle"        124 9x   0000
expect_rc "garbage is not idle"                    124 abc  0000
expect_rc "a settled device is idle"               27  0     0000

# The derived answers. The stored value alone does not determine the code: the
# client asks whether the reboot happened and whether the image is mounted, so
# the same stored state yields different answers on different evidence. A stub
# that maps state to code cannot show this.
expect_rc "firmware installed, reboot taken"       23  2     0100
expect_rc "application installed, nothing mounted" 55  3     0001

if [ "$fails" -ne 0 ]; then
    echo "environment harness: $fails case(s) FAILED"
    exit 1
fi
echo "environment harness: all cases passed"
