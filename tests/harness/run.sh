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
# Two preconditions the cases cannot report for themselves, so both are checked
# before any of them run: the fixture directory has to be the one the two build
# steps were pointed at, because the path is compiled in rather than passed --
# otherwise every case seeds one file and measures another -- and the lock the
# environment writer takes has to be creatable, or every write verb answers with
# a generic error that reads as a product failure.
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

for c in python3 timeout; do
    command -v "$c" >/dev/null || { echo "harness: missing $c" >&2; exit 2; }
done

mkdir -p "$FIXTURE"
FIXTURE=$(CDPATH='' cd -- "$FIXTURE" && pwd)

if ! grep -qaF -- "$FIXTURE/fw_env.config" "$CLIENT"; then
    echo "harness: $CLIENT does not carry $FIXTURE/fw_env.config" >&2
    echo "harness: rebuild both halves with --env-config $FIXTURE/fw_env.config" >&2
    exit 2
fi

LOCK=/var/lock/fw_printenv.lock
if ! ( : > "$LOCK" ) 2>/dev/null; then
    echo "harness: cannot create $LOCK, which the environment writer takes" >&2
    exit 2
fi

ENV_BIN="$FIXTURE/env.bin"
printf '%s 0x0000 0x2000\n' "$ENV_BIN" > "$FIXTURE/fw_env.config"

# One case takes the environment's write permission away on purpose. Dying
# between that and its restore leaves a fixture nothing can seed again -- and
# the next run would spend its cases on a stale environment rather than on the
# product. Give the permission back however this run ends.
restore_fixture() {
    if [ -e "$ENV_BIN" ]; then chmod 0644 "$ENV_BIN" 2>/dev/null || true; fi
}
trap 'status=$?; restore_fixture; exit $status' EXIT
trap 'restore_fixture; exit 130' INT
trap 'restore_fixture; exit 143' TERM HUP

# The summary used to count failures only, so a run that never reached half the
# file -- an early return, a block left behind by an edit -- would still have
# ended in the sentence that says everything passed. The total is declared here
# and checked against what actually ran. It has to be bumped when a case is
# added, and that is the point: a case count nobody maintains cannot notice a
# case that disappears.
EXPECTED_CASES=45

fails=0
passes=0

# A seed that fails has to stop the run. Without `set -e` its status was
# dropped, and the case then measured whatever the previous case had left
# behind: for the decode table, twenty-one consecutive rows all expecting the
# same recovery code, a seed that stops working part-way through keeps every
# later row green while it tests nothing at all.
seed() { # seed <state> <update-bits> [override k=v...]
    _state=$1; _bits=$2; shift 2
    python3 "$MKENV" build "$ENV_BIN" \
        update_reboot_state="$_state" update="$_bits" \
        'BOOT_ORDER=A B' 'BOOT_ORDER_OLD=A B' BOOT_B_LEFT=3 \
        application=A 'rauc_cmd=rauc.slot=A' 'console=ttymxc1,115200' \
        BOOT_A_LEFT=3 "$@" && return 0
    echo "harness: cannot seed state $_state bits $_bits into $ENV_BIN" >&2
    echo "harness: the cases after this one would have measured a stale fixture" >&2
    exit 2
}

# No case may wait forever. The environment writer takes a *blocking* lock, so
# a concurrent write from anywhere on the machine, or an earlier run still
# alive, stalls a case with no output and no end.
#
# The kill signal is not a detail. An expiry has to be unmistakable for an
# answer, and the ordinary expiry status is 124 -- the exact code most of the
# cases below expect. Killed instead, the run reports 137, which no verb here
# produces, so a hang can never be read as a pass.
CASE_TIMEOUT=30

client() { # client <args...> -> sets rc, out
    out=$(timeout --signal=KILL "$CASE_TIMEOUT" "$QEMU" -L "$SYSROOT" \
        -E LD_LIBRARY_PATH="$LIBDIR:$SYSROOT/usr/lib:$SYSROOT/lib" \
        "$CLIENT" "$@" 2>&1)
    rc=$?
    if [ "$rc" -eq 137 ]; then
        echo "harness: no answer within ${CASE_TIMEOUT}s from: $*" >&2
        echo "harness: something holds $LOCK, or an earlier run is still alive" >&2
        exit 2
    fi
}

# Some of these codes carry very little on their own. The recovery answer and
# the write-path error are what the client says for a whole family of failures
# -- an environment that is corrupt, or missing entirely, produces both -- so a
# case that checks only the number has shown that something went wrong, not
# that the thing it seeded went wrong. Where the product prints a line naming
# the mechanism, the case requires that line too, and for a rejected value the
# line quotes the value: the case then also proves the binary read what this
# run seeded rather than what an earlier one left behind.
out_names() { # out_names <substring>
    printf '%s' "$out" | grep -qF -- "$1"
}

verdict() { # verdict <name> <context> -- reads $_why
    if [ -z "$_why" ]; then
        passes=$((passes + 1))
        echo "PASS: $1"
    else
        echo "FAIL: $1 -- $2: $_why"
        echo "      $(printf '%s' "$out" | tail -n 1)"
        fails=$((fails + 1))
    fi
}

judge() { # judge <want-rc> <want-line> -- sets $_why
    _why=""
    [ "$rc" = "$1" ] || _why="want rc $1, got $rc"
    if [ -n "$2" ] && ! out_names "$2"; then
        _why="${_why:+$_why; }nothing in the output named: $2"
    fi
}

expect_rc_named() { # <name> <want> <want-line> <state> <bits> [override k=v...]
    _n=$1; _w=$2; _l=$3; _s=$4; _b=$5; shift 5
    seed "$_s" "$_b" "$@"
    client --update_reboot_state
    judge "$_w" "$_l"
    verdict "$_n" "seeded state $_s bits $_b"
}

expect_rc() { # expect_rc <name> <want> <state> <bits> [override k=v...]
    _en=$1; _ew=$2; shift 2
    expect_rc_named "$_en" "$_ew" "" "$@"
}

# The read is total over what the storage can hold: a value outside the
# alphabet must answer the recovery code rather than idle, or a consumer that
# treats only idle as "nothing to do" keeps skipping a device nobody can read.
expect_rc_named "an unreadable value is not idle" 124 \
    'holds uninterpretable content: "9x"' 9x 0000
expect_rc_named "garbage is not idle" 124 \
    'holds uninterpretable content: "abc"' abc 0000
expect_rc "a settled device is idle"               27  0     0000

# The derived answers. The stored value alone does not determine the code: the
# client asks whether the reboot happened and whether the image is mounted, so
# the same stored state yields different answers on different evidence. A stub
# that maps state to code cannot show this.
#
# The first two rows are that claim, and they are one variable apart. Same
# stored state, same bitfield; in the second the boot order leads with a slot
# other than the running one while the previous order is still recorded -- what
# an install writes and a reboot then consumes. So the same device state is
# reported as awaiting a commit in one case and as still owing its reboot in
# the other, on evidence alone. (The derivation also requires both boot budgets
# untouched; a spent budget does not move this answer by itself.)
#
# The mount side of the same question is out of reach here: nothing loop-mounts
# an application image under user-mode emulation. This pair covers the boot
# order, not the mount.
expect_rc "firmware installed, reboot taken"       23  2     0010
expect_rc "firmware installed, reboot still owed"  26  2     0010 'BOOT_ORDER=B A'
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
    expect_rc_named "unreadable value rejected -- $1" 124 \
        "holds uninterpretable content: \"$2\"" 0 0000 update_reboot_state="$2"
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
expect_verb_named() { # <name> <verb> <want-rc> <want-line> <state> <bits> [override k=v...]
    _n=$1; _v=$2; _w=$3; _l=$4; _s=$5; _b=$6; shift 6
    seed "$_s" "$_b" "$@"
    client "$_v"
    judge "$_w" "$_l"
    verdict "$_n" "$_v on state $_s"
}

expect_verb() { # expect_verb <name> <verb> <want-rc> <state> <bits> [override k=v...]
    _en=$1; _ev=$2; _ew=$3; shift 3
    expect_verb_named "$_en" "$_ev" "$_ew" "" "$@"
}

expect_verb "commit: nothing to do on a settled device" --commit_update   17 0  0000
expect_verb "commit: a decided firmware rollback finalises" --commit_update 16 7  0000
expect_verb_named "commit: an unowned shape is refused by name" --commit_update 18 \
    'no arm settles update_reboot_state=9' 9 0000
expect_verb_named "commit: an unreadable state is refused" --commit_update 18 \
    'holds uninterpretable content: "zz"' zz 0000
expect_verb "rollback: nothing pending on a settled device" --rollback_update 27 0 0000
expect_verb "rollback: a pending firmware update is undone" --rollback_update 12 2 0000
expect_verb "rollback: an indeterminate app rollback is named" --rollback_update 57 8 0000
expect_verb_named "rollback: an unreadable state is refused" --rollback_update 124 \
    'holds uninterpretable content: "zz"' zz 0000

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
expect_verb_named "an ambiguous bitfield is refused on read, before any arm" \
    --commit_update 19 'does not allowed content: "per-bit validation" instead:1010' 2 1010

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
    judge "$2" "Cannot write U-Boot Env"
    [ "$_left" = "1" ] || _why="${_why:+$_why; }the refused write left BOOT_A_LEFT=$_left"
    verdict "$1" "a commit that has to put the boot budget back"
}

expect_write_refused "a write that cannot be persisted is reported, not assumed" 19

ran=$((passes + fails))
if [ "$ran" -ne "$EXPECTED_CASES" ]; then
    echo "environment harness: $ran case(s) ran, $EXPECTED_CASES expected"
    exit 1
fi
if [ "$fails" -ne 0 ]; then
    echo "environment harness: $fails of $ran case(s) FAILED"
    exit 1
fi
echo "environment harness: $passes/$EXPECTED_CASES cases passed"
