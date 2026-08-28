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

for c in python3 timeout flock; do
    command -v "$c" >/dev/null || { echo "harness: missing $c" >&2; exit 2; }
done

# One case takes the environment's write permission away and requires the write
# to be refused. Root ignores the permission, so under root that case fails for
# a reason that has nothing to do with the product.
[ "$(id -u)" -ne 0 ] || {
    echo "harness: run as an ordinary user; root defeats the write-refusal case" >&2
    exit 2
}

mkdir -p "$FIXTURE" 2>/dev/null || {
    echo "harness: cannot create the fixture directory $FIXTURE" >&2
    exit 2
}
FIXTURE=$(CDPATH='' cd -- "$FIXTURE" && pwd) || {
    echo "harness: cannot enter the fixture directory" >&2
    exit 2
}

if ! grep -qaF -- "$FIXTURE/fw_env.config" "$CLIENT"; then
    echo "harness: $CLIENT does not carry $FIXTURE/fw_env.config" >&2
    echo "harness: rebuild both halves with --env-config $FIXTURE/fw_env.config" >&2
    exit 2
fi

# The client links the library statically as it is built here, so the string
# just checked is the whole answer for this run. The library carries its own
# copy of the same path, and that copy is what would decide the moment the
# linking changes -- it sits first on the library path used below. Holding it
# to the same rule while it is there to check keeps this guard from quietly
# covering half of what the header promises.
for lib in "$LIBDIR"/libfs_updater.so*; do
    [ -e "$lib" ] || continue
    grep -qaF -- "$FIXTURE/fw_env.config" "$lib" || {
        echo "harness: $lib does not carry $FIXTURE/fw_env.config" >&2
        echo "harness: the two halves were built against different fixtures" >&2
        exit 2
    }
done

# The environment writer takes this lock before every write, and it waits for
# it rather than failing. Asking only whether the file can be created answers
# the wrong question: a lock somebody else holds passes that check and then
# stalls the first case. Take it the same way the writer does, non-blocking,
# and give it straight back -- and open it for append, because truncating a
# file another process is using is not a probe.
LOCK=/var/lock/fw_printenv.lock
if ! ( flock -n 9 ) 9>>"$LOCK" 2>/dev/null; then
    if [ -e "$LOCK" ]; then
        echo "harness: $LOCK is held; another environment writer is running" >&2
    else
        echo "harness: cannot create $LOCK, which the environment writer takes" >&2
    fi
    exit 2
fi

# The mount evidence comes from the REAL /sys/class/block: the binary runs under
# user-mode emulation, and sysfs is not part of the emulated root. With no loop
# device bound anywhere on this machine the mount question answers "nothing
# mounted", which is what the derived cases below are written against. Bind one
# -- a snap, a mounted image, an unrelated build -- and those cases silently
# measure a different shape: the indeterminate answers become "reboot taken" or
# "reboot still owed", and they do it without any case naming a reason. Held
# here rather than assumed, because the run cannot tell the two apart afterwards.
for _loop_backing in /sys/class/block/loop*/loop/backing_file; do
    [ -r "$_loop_backing" ] || continue
    echo "harness: $_loop_backing is readable -- a loop device is bound on this host" >&2
    echo "harness: the mount-evidence cases would measure a different shape; detach it first" >&2
    exit 2
done

ENV_BIN="$FIXTURE/env.bin"
printf '%s 0x0000 %s\n' "$ENV_BIN" "$(python3 "$MKENV" size)" > "$FIXTURE/fw_env.config"

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
EXPECTED_CASES=63

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
# The target-digit member of the comparison is the first row above; repeating it
# here under a second name only made one product change look like two.
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
expect_verb "commit: an incomplete install with nothing in flight is refused" \
    --commit_update 18 2 0000
# Nothing writes this state, so a device carrying it got it from outside. It used
# to be a dead end: its acknowledge predicate wanted the running slot uncommitted,
# which is the opposite of what the state means, so every verb refused. Meanwhile
# the consuming layer counts its code among the failed ones and calls commit to
# settle it on every boot, and the deadline timer reboots on the failure. It has
# to settle from any shape, which is what this case measures.
# The answer is its own code, not an ordinary confirmation: nothing was
# confirmed here, a legacy value was migrated, and a fleet that cannot tell the
# two apart cannot see that a device arrived carrying an older generation.
expect_verb "commit: an induced failed-reboot state is migrated, and says so" \
    --commit_update 59 1 0000
# The refusal has to say WHICH of the two reasons it is. This shape is the
# second one -- state 9 has an arm, the arm wants both slots uncommitted, and
# the bitfield is settled -- and a caller can only act on the reason, not on
# "not allowed". The state number alone leaves the two indistinguishable.
expect_verb_named "commit: an owned state whose slot precondition fails says what it wanted" \
    --commit_update 18 \
    'update_reboot_state=9 has an arm, but its slot precondition does not hold; it expects an uncommitted firmware and application slot' \
    9 0000
expect_verb_named "commit: an unreadable state is refused" --commit_update 18 \
    'holds uninterpretable content: "zz"' zz 0000
expect_verb "rollback: nothing pending on a settled device" --rollback_update 27 0 0000
expect_verb "rollback: a pending firmware update is undone" --rollback_update 12 2 0000
expect_verb_named "rollback: an indeterminate app rollback is named" --rollback_update 57 \
    'no app image mounted' 8 0000

# The same state, answered without the probe. A running application slot still
# marked uncommitted is the rollback's own record that it was enacted, so the
# bitfield settles the question first -- which is why this row answers "commit
# requested" where the row above answers "indeterminate", on the same state and
# the same (empty) set of loop devices. The digit is the RUNNING slot's: the
# fixture runs A, so index 1 of [fw_A][app_A][fw_B][app_B].
expect_verb "rollback: the bitfield decides an app rollback before the mount is asked" \
    --rollback_update 32 8 0100

# The shortcut reads the RUNNING slot only, and it reads a bit, not a digit.
#
# A '3' is uncommitted AND bad at once. Nothing in this tree writes one, and no
# commit path settles one, but the validator accepts it -- so a device can carry
# it, and what the shortcut then does is a contract rather than an accident: on
# the running slot it satisfies the uncommitted bit and decides the answer
# without a probe; on the target slot it does not enter into this question at
# all, and the mount evidence still decides. Both rows would answer the same as
# their plain-digit siblings if the check ever became an equality on '1'.
expect_verb "rollback: a bad-and-uncommitted running digit still feeds the shortcut" \
    --rollback_update 32 8 0300
expect_verb "rollback: the target's digit does not feed the running slot's shortcut" \
    --rollback_update 57 8 0003

# The switch verb decides the target slot before it builds the collaborator that
# would install to it. This fixture has no RAUC configuration -- the shape of a
# device that was never provisioned -- so with the decision made afterwards the
# operator is told a configuration is missing where the answerable problem is
# the target slot. The digit is the TARGET slot's: the fixture runs A.
expect_verb_named "switch: an unprovisioned target names the slot, not the missing configuration" \
    --switch_app_slot 56 'slot B was never provisioned' 0 0000

# An unacceptable bitfield is refused where the variable is read, before any
# classification runs -- the state is never even asked about. Pinned at state 8
# because that is the arm which reaches a probe: an out-of-contract field must
# not get as far as deciding what is mounted.
#
# On the status verb this is the recovery code. The same throw reaches a
# different number through a mutating verb -- that verb's own handler maps it to
# its system error -- which is why the row names the verb it was measured on
# rather than the state alone.
expect_rc_named "an unacceptable bitfield is refused before the state is classified" 124 \
    'does not allowed content: "per-bit validation" instead:1010' 8 1010
expect_verb_named "rollback: an unreadable state is refused" --rollback_update 124 \
    'holds uninterpretable content: "zz"' zz 0000

# The two query verbs answer on stdout and in the exit code, and both halves are
# pinned here: the line is the answer, the code says the query ran. The code
# used to be the constructor's 0 -- the only verb in this table whose success
# was 0, next to twenty-odd whose success is not, and indistinguishable from a
# verb that set nothing at all.
expect_verb_named "query: a slot that is not bad answers 0 and says the query ran" \
    --is_app_state_bad=B 52 '0' 0 0000
expect_verb_named "query: a slot marked bad answers 1 and says the query ran" \
    --is_app_state_bad=B 52 '1' 0 0002
# The firmware query keeps the same contract and gets its own row rather than
# riding on the application one. Firmware B is index 2 of [fw_A][app_A][fw_B][app_B].
expect_verb_named "query: the firmware twin answers the same way" \
    --is_fw_state_bad=B 52 '1' 0 0020

# The setters had no case here at all, and that is how a defect reached a
# device: every row above reads the field, none of them writes it, so what the
# marking verbs leave behind rested on a single bench measurement. The verbs
# answer with a code and print nothing, so the code alone would say only that
# something ran -- the assertion that carries the meaning is the field
# afterwards.
expect_verb_writes() { # <name> <verb> <want-rc> <state> <bits> <want-bits-after> [override k=v...]
    _n=$1; _v=$2; _w=$3; _s=$4; _b=$5; _a=$6; shift 6
    seed "$_s" "$_b" "$@"
    client "$_v"
    _bits_now=$(python3 "$MKENV" read "$ENV_BIN" 2>/dev/null | sed -n 's/^update=//p')
    judge "$_w" ""
    [ "$_bits_now" = "$_a" ] || _why="${_why:+$_why; }want update $_a, got $_bits_now"
    verdict "$_n" "seeded state $_s bits $_b, ran $_v"
}

expect_verb_writes "mark: a committed application slot becomes bad" \
    --set_app_state_bad=B 52 0 0000 0002
expect_verb_writes "mark: the firmware twin writes its own digit" \
    --set_fw_state_bad=B 52 0 0000 0020

# The one that matters. A slot can be marked while an update is in flight on it
# -- the boot guard does exactly that when the trials run out -- and the two
# facts are independent, so the mark must not take the other one with it. It
# used to: the digit was replaced rather than the bit set, the pending-update
# predicate then answered false, and the revert took a path that reaches a
# different end state. Nothing at this level held that.
expect_verb_writes "mark: a slot in flight keeps that fact when it is marked bad" \
    --set_app_state_bad=B 52 3 0001 0003

# The other direction of the same independence, at the door an operator uses.
# A slot can be marked bad after it booted -- the guard does it when the trials
# run out -- and the commit that follows must not undo that verdict. The commit
# path used to write the committed digit as a literal, so it cleared both facts
# at once: the device came out of the update reporting a slot as good that had
# been condemned while running, and the switch verbs would then admit it.
#
# The success predicate reads the boot order and never the digit, so this arm is
# reached with the mark standing.
expect_verb_writes "commit: a firmware update settles without clearing the slot's verdict" \
    --commit_update 16 2 0030 0020 \
    'rauc_cmd=rauc.slot=B' 'BOOT_ORDER=B A' 'BOOT_ORDER_OLD=A B'

# Marking twice must stage nothing: a caller that marks on every boot would
# otherwise write the bootloader environment on every boot.
expect_verb_writes "mark: a slot that is already bad is left alone" \
    --set_app_state_bad=B 52 0 0002 0002

# And the reader side of the same digit, at the product level: settling an
# application rollback clears the in-flight bit and leaves the verdict. The
# commit that does it is the one the boot guard's revert reaches.
expect_verb_writes "commit: settling a rollback keeps a verdict on the slot" \
    --commit_update 16 8 0003 0002

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
expect_verb "commit: an install whose target never booted settles, running the other slot" \
    --commit_update 58 2 0010 'BOOT_ORDER=B A' 'BOOT_ORDER_OLD=B A' \
    application=B 'rauc_cmd=rauc.slot=B'
expect_verb "commit: still one digit and not the target, running the other slot" \
    --commit_update 58 2 1000 'BOOT_ORDER=B A' 'BOOT_ORDER_OLD=B A' \
    application=B 'rauc_cmd=rauc.slot=B'

# Two uncommitted firmware digits never reach that arm at all. The value is
# validated where the variable is read, and a bitfield claiming two firmware
# slots in flight is refused there as out of contract, so the commit ends in the
# generic error of the read path rather than in any state arm -- which also
# makes the arm's own two-in-flight guard unreachable from here. This is the
# family where an unacceptable bitfield jams the commit and every install after
# it.
expect_verb_named "an ambiguous bitfield is refused on read, before any arm" \
    --commit_update 19 'does not allowed content: "per-bit validation" instead:1010' 2 1010

# An application rollback whose image will not mount is settled by the commit,
# on the evidence that the slot switch already happened. Both cases name the
# line that produces the answer, not only the number: a code alone cannot say
# which mechanism reached it.
expect_verb_named "commit: an app rollback with nothing mounted is settled" \
    --commit_update 16 'Commit update' 8 0000

# NOT the mount behaviour its old name claimed, and no longer the collaborator's
# error either. The state says an application update is in flight and the
# bitfield says nothing is, so the pending arm is not taken and this seed reaches
# the committed-slot-switch verdict instead -- where the target slot has no image
# under user-mode emulation. The line is that verdict; the code is the rollback
# handler's plain progress error, which is what it makes of every refusal errno,
# so the two halves say different things and both are pinned. With a RAUC fixture
# present this same seed settles instead of refusing; measuring the mount half
# needs that fixture and is out of scope here.
expect_verb_named "rollback: a settled bitfield sends a pending state to the switch verdict" \
    --rollback_update 13 'slot B was never provisioned' 3 0000

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

# The same failure on the arm that settles an application rollback, which is a
# heavier write: it clears a slot digit, puts BOTH boot budgets back and clears
# the state, and it does all of that in one transaction. A partial write here
# would leave a device whose durable record says one thing and whose budgets say
# another -- worse than the refusal, because the next read cannot tell that
# anything was lost. The seed is the shape whose precondition the commit now
# accepts, so this exercises the path that changed rather than the one that
# always refused.
expect_rollback_write_refused() { # <name> <want-rc>
    seed 8 0000
    chmod 0444 "$ENV_BIN"
    client --commit_update
    chmod 0644 "$ENV_BIN"
    _state_after=$(python3 "$MKENV" read "$ENV_BIN" 2>/dev/null | sed -n 's/^update_reboot_state=//p')
    _bits_after=$(python3 "$MKENV" read "$ENV_BIN" 2>/dev/null | sed -n 's/^update=//p')
    judge "$2" "Cannot write U-Boot Env"
    [ "$_state_after" = "8" ] || _why="${_why:+$_why; }the refused write left update_reboot_state=$_state_after"
    [ "$_bits_after" = "0000" ] || _why="${_why:+$_why; }the refused write left update=$_bits_after"
    verdict "$1" "a rollback commit whose write cannot be persisted"
}

expect_rollback_write_refused "a refused rollback commit leaves the state it could not settle" 19

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
