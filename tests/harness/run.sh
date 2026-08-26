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

seed() { # seed <state> <update-bits> [override k=v...]
    _state=$1; _bits=$2; shift 2
    python3 "$MKENV" build "$ENV_BIN" \
        update_reboot_state="$_state" update="$_bits" \
        'BOOT_ORDER=A B' 'BOOT_ORDER_OLD=A B' BOOT_B_LEFT=3 \
        application=A 'rauc_cmd=rauc.slot=A' 'console=ttymxc1,115200' \
        BOOT_A_LEFT=3 "$@"
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
expect_rc "firmware installed, reboot taken"       23  2     0010
expect_rc "application installed, nothing mounted" 55  3     0001

# The bitfield is [fw_A][app_A][fw_B][app_B]; running on A, an install's target
# is B. The query does not consult it at all -- the derived answer comes from
# the stored state and the boot and mount evidence -- so the same state answers
# the same way whichever digits are set. Pinned because a reader seeing a
# bitfield in a fixture will otherwise assume it is what produced the answer.
expect_rc "the query ignores the bitfield (target digit set)" 23 2 0010
expect_rc "the query ignores the bitfield (wrong digit set)" 23 2 0100
expect_rc "the query ignores the bitfield (no digit set)"    23 2 0000

# The decoder's alphabet, at the client boundary. The library pins these
# against its own environment doubles; this runs the same values through the
# shipping binary and a real file, so the answer comes from the parsing that
# ships rather than from a double's idea of it. Every one of them must reach
# the recovery answer: a consumer that treats only the settled code as
# "nothing to do" keeps skipping a device whose state nobody could read.
#
# Rows the library's table has and this one cannot: a value with an embedded
# NUL, which the environment format uses as its own separator, and the
# multi-kilobyte values, which do not fit the fixture's block.
decode_row() { # decode_row <name> <raw value>
    seed 0 0000 update_reboot_state="$2"
    client --update_reboot_state
    if [ "$rc" = 124 ]; then
        echo "PASS: unreadable value rejected -- $1"
    else
        echo "FAIL: unreadable value rejected -- $1: want rc 124, got $rc"
        fails=$((fails + 1))
    fi
}

decode_row "out of range 13"      "13"
decode_row "out of range 14"      "14"
decode_row "out of range 99"      "99"
decode_row "out of range 255"     "255"
decode_row "out of range 256"     "256"
decode_row "wider than 32 bits"   "4294967296"
decode_row "wider than 64 bits"   "184467440737095516160"
decode_row "empty"                ""
decode_row "space only"           " "
decode_row "tab only"             "$(printf '\t')"
decode_row "trailing space"       "2 "
decode_row "leading space"        " 2"
decode_row "alphabetic"           "abc"
decode_row "digit then alpha"     "2abc"
decode_row "hex prefix"           "0x02"
decode_row "explicit plus"        "+2"
decode_row "negative"             "-1"
decode_row "decimal point"        "2.0"
decode_row "decimal comma"        "2,0"
decode_row "leading zero"         "012"
decode_row "non-ascii digit"      "$(printf '\357\274\220')"

# The table's guard against a silent pass: if seeding stopped working, every
# row above would keep answering from whatever was written last -- which is
# also unreadable, so they would all stay green while testing nothing. A
# canonical value seeded the same way must still decode.
expect_rc "a canonical value still decodes after the table" 27 0 0000

# Which verb refuses which state, and with which code. Measured against this
# binary rather than assumed; the rows below are the ones that carry meaning
# beyond "the verb echoes the state it found".
#
# Every row seeds the bitfield as all-zero, i.e. the state is set but the
# precondition its owning arm checks does not hold. That is the shape the
# terminal refusal exists for.
expect_verb() { # expect_verb <name> <verb> <want-rc> <state> <bits>
    seed "$4" "$5"
    client "$2"
    if [ "$rc" = "$3" ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1 -- $2 on state $4: want rc $3, got $rc"
        fails=$((fails + 1))
    fi
}

expect_verb "commit: nothing to do on a settled device" --commit_update   17 0  0000
expect_verb "commit: a decided firmware rollback finalises" --commit_update 16 7  0000
expect_verb "commit: an unowned shape is refused by name" --commit_update 18 9  0000
expect_verb "commit: an unreadable state is refused"      --commit_update 18 zz 0000
expect_verb "rollback: nothing pending on a settled device" --rollback_update 27 0 0000
expect_verb "rollback: a pending firmware update is undone" --rollback_update 12 2 0000
expect_verb "rollback: an indeterminate app rollback is named" --rollback_update 57 8 0000
expect_verb "rollback: an unreadable state is refused"     --rollback_update 124 zz 0000

# With a single uncommitted firmware digit set, the commit reaches the arm that
# owns the state and settles an install whose target was never activated -- the
# boot order still leads with the running slot, so nothing ever booted what was
# installed. It answers with its own code rather than the ordinary success,
# which is the whole point of that code: a backend told "committed" would record
# a discarded update as a confirmed one.
#
# What that arm keys on is *one* uncommitted firmware digit, either slot's, and
# not the target's: the two rows below differ only in which slot carries it and
# answer the same, and repeating both from the other running slot answers the
# same again. The running slot does not enter into it.
expect_verb "commit: an install whose target never booted settles distinctly" \
    --commit_update 58 2 0010
expect_verb "commit: the settle keys on one uncommitted digit, not on the target" \
    --commit_update 58 2 1000

# Two uncommitted firmware digits never reach that arm at all. The value is
# validated where the variable is read, and a bitfield claiming two firmware
# slots in flight is refused there as out of contract, so the commit ends in the
# generic error of the read path rather than in any state arm -- which also
# makes the arm's own two-in-flight guard unreachable from here. This is the
# family where an unacceptable bitfield jams the commit and every install after
# it.
expect_verb "an ambiguous bitfield is refused on read, before any arm" \
    --commit_update 19 2 1010

# The two cells below are today's behaviour and they are the defect, not the
# contract: an application rollback with no mountable image cannot be settled
# by either verb, so the device parks. They are pinned so the fix has to come
# through here and cannot land unnoticed -- when it does, these two fail and
# the expectations move.
expect_verb "commit cannot settle an app rollback with nothing mounted" \
    --commit_update 19 8 0000
expect_verb "rollback refuses a pending app update with nothing mounted" \
    --rollback_update 15 3 0000

# Deliberately absent: --apply_update. Where it has real work it ends in a
# reboot, and there is no reboot here, so it answers with the system-level
# failure. That is the harness speaking, not the product, and pinning it would
# pin the emulation.

# A write that cannot be persisted. The environment carries the only record of
# what the device is doing, so the failure has to be reported rather than
# assumed, and what is already there has to survive: a half-written environment
# is worse than an unchanged one, because the next read cannot tell the
# difference between a value and a casualty.
#
# The write is provoked by a spent boot budget on a settled device -- the one
# branch that puts the budget back, and the only write an otherwise idle client
# makes.
expect_write_refused() { # expect_write_refused <name> <want-rc>
    seed 0 0000 BOOT_A_LEFT=1
    chmod 0444 "$ENV_BIN"
    client --commit_update
    chmod 0644 "$ENV_BIN"
    _left=$(python3 "$MKENV" read "$ENV_BIN" 2>/dev/null | sed -n 's/^BOOT_A_LEFT=//p')
    _ok=1
    [ "$rc" = "$2" ] || { echo "      want rc $2, got $rc"; _ok=0; }
    [ "$_left" = "1" ] || { echo "      the refused write left BOOT_A_LEFT=$_left"; _ok=0; }
    if [ "$_ok" -eq 1 ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1"
        fails=$((fails + 1))
    fi
}

expect_write_refused "a write that cannot be persisted is reported, not assumed" 19

if [ "$fails" -ne 0 ]; then
    echo "environment harness: $fails case(s) FAILED"
    exit 1
fi
echo "environment harness: all cases passed"
