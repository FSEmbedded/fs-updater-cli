# CLI Reference

Binary: `fs-updater`, installed to `/usr/sbin/`.

All action arguments are **mutually exclusive** except `--debug` and `--serial`
(combinable with any action) and `--detach` (modifier for `--install_update`
only — see below).

The CLI is built with D-Bus support only (`BUILD_DBUS_SUPPORT` is a required
`ON` option; the build fails otherwise). The commands below that talk to the
system daemon (`de.fsembedded.fsupdate1`) do so over D-Bus, not through files
or another process — see [D-Bus Session Protocol](../integration/dbus-session-protocol.md)
for the underlying calls and [Azure Device Update Integration](../integration/azure-device-update.md)
for how the ADU handler drives the same interface directly.

See [Return Codes](return-codes.md) for the full exit-code table.

```
fs-updater [--rollback_update] [--switch_fw_slot] [--switch_app_slot]
           [--commit_update] [--update_reboot_state] [--debug]
           [--firmware_version] [--application_version] [--version] [-h]
           [--apply_update] [--install_update] [--detach] [--serial]
           [--install_progress] [--cancel_install <session_id (uint32)>]
           [--download_progress] [--download_update] [--is_update_available]
           [--set_app_state_bad <accepted states: A or B>]
           [--is_app_state_bad <accepted states: A or B>]
           [--set_fw_state_bad <accepted states: A or B>]
           [--is_fw_state_bad <accepted states: A or B>] [--] [install_path]
```

`fs-updater --help` prints this block with a description of every option.

---

## Category A: Install and commit

### `--install_update [install_path]`

Install an update. Two forms, distinguished by whether a path is given:

- **With a path** (`--install_update /mnt/usb/update.fs`): install that local
  bundle via `InstallLocal` on the D-Bus service. The path is a positional
  operand and is accepted only together with `--install_update`: a path on
  its own exits 65, an empty path exits 61. The CLI resolves it to an
  absolute path before passing it on. Blocking by default — the CLI
  streams live progress to stdout and returns the terminal 0/4/8 ÷ 3/7/11
  verdict (see [Return Codes](return-codes.md)) once the service reports the
  install finished or failed. Pass `--detach` to return immediately instead.
  The blocking wait gives up after 120 s without progress (exit 47); the
  environment variable `FSUP_INSTALL_WAIT_MS` overrides that limit with a
  value from 1 to 3600000 milliseconds.
  A `--` before the path ends option parsing, so a path that itself starts
  with `-` is still read as the path.
- **Without a path:** trigger `StartInstall` for a download some other D-Bus
  caller already finished (`DownloadState == "finished"`, with the
  `SessionId`/`UpdateType` that caller's `FinishDownload` recorded). Reports
  46 (nothing queued) if that state isn't there yet. Not the ADU handler's
  path — it calls `StartInstall` itself directly over D-Bus (see
  [Azure Device Update Integration](../integration/azure-device-update.md#adu-lifecycle--integration-mapping));
  this form is for a manual step or a different integrator advancing a
  download it started itself.

The formats an install accepts and the `.fs` container layout are described in
the library's
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/bundle-format.md)
reference.

```bash
# Blocking local install, type auto-detected from the bundle
fs-updater --install_update /mnt/usb/update.fs

# Start it and come back later
fs-updater --install_update /mnt/usb/update.fs --detach
fs-updater --install_progress
```

| Exit code | Meaning |
|:---------:|---------|
| 0 / 4 / 8 | Firmware / application / firmware+application installed |
| 3 / 7 / 11 | System error (firmware / application / combined) — the only per-type failure verdict the install path produces; 1/2/5/6/9/10 (progress/internal-error) are reserved and never produced |
| 46 | No path given, and no finished download staged to advance |
| 47 | Accepted and still running, or the blocking wait ended without an outcome; the cases are listed under [Return Codes](return-codes.md#install-progress---install_progress-and---install_updates-in-flightfailed-cases). Poll `--install_progress` |
| 48 | No path given, and the D-Bus service already reports the (other-owned) install finished; or a local install succeeded with an update type that could not be classified |
| 49 | The `InstallLocal`/`StartInstall` call failed, or a local install failed with an update type that could not be classified |
| 61 | Path is empty, does not exist or cannot be resolved (a file that exists but is unreadable is rejected by the service: 49) |
| 65 | A path without `--install_update`, `--detach` without a path, or a second action flag |
| 66 | Another install or download is already in progress (`Error.Busy`) |
| 67 | Rejected by bus policy (`Error.AccessDenied`) |

A reboot is required before `--commit_update`.

### `--detach`

Modifier for `--install_update <path>`: start the install and return
immediately with the session id instead of blocking on its outcome. Poll
`--install_progress` for status.

### `--install_progress`

Report the state of the install the D-Bus service is currently tracking
(`InstallState`/`InstallProgress`). Independent of how that install was
started — a detached local install, an ADU-driven one, or one this CLI
process did not itself start.

| Exit code | Meaning |
|:---------:|---------|
| 46 | No installation queued |
| 47 | Installation in progress (percentage printed to stdout) |
| 48 | Installation finished (percentage printed to stdout) |
| 49 | Installation failed (last reached percentage printed to stdout) |

### `--cancel_install <session_id (uint32)>`

Best-effort cancel for the given session id (`CancelInstall` on the bus).
Does not interrupt the running install — the worker still runs to
completion — it only changes how the outcome is reported once it does; see
the `CancelInstall` method doc in the D-Bus interface for the exact
semantics.

| Exit code | Meaning |
|:---------:|---------|
| 1 | `session_id` is not a plain decimal number (generic parse error; usage printed) |
| 47 | Cancel requested; final outcome via `--install_progress` |
| 49 | `session_id` is `0`, or the `CancelInstall` D-Bus call failed |

### `--commit_update`

Confirm the active update or rollback, or settle a failed install. A commit
that settles a state writes the U-Boot environment and returns
`update_reboot_state` to 0 (idle). Exit 17 changes no variable; a refusal (18) or a
failure (19) does not settle the state.

For an update or a rollback, call it after rebooting into the new or
rolled-back slot. Which state it settles, and with which exit code, is listed
per state under [`--update_reboot_state`](#--update_reboot_state).

| Exit code | Meaning |
|:---------:|---------|
| 16 | Committed successfully. What it settled depends on the stored state: an update, rollback or slot switch is confirmed, a failed install acknowledged. After a bootloader fallback (23, 25) the failed update is discarded: its firmware slot is marked bad and the boot order restored (the library's [transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram), Phase 2). After an install interrupted during the image write the update is discarded too (exit 55 from state 2 below). With nothing pending, the running slot's boot budget was restored (the routine mark-good) |
| 17 | Nothing pending and nothing to restore |
| 18 | U-Boot state incompatible |
| 19 | System error |
| 58 | An install interrupted before its target was activated has been settled (discarded, slot quarantined) |
| 59 | The stored state was `FW_UPDATE_REBOOT_FAILED` (1), which the library does not write, and has been settled (nothing confirmed, nothing discarded) |

### `--update_reboot_state`

Read the stored `update_reboot_state`, and for the pending states the evidence
of whether the reboot took effect, and answer with one exit code. A
human-readable line goes to stdout.

This table is the mapping from exit code to the library's stored state. The
states themselves, their numeric values and what each one means are defined in
the [fs-updater-lib state machine](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#state-table);
the enum names of the exit codes are listed in
[Return Codes](return-codes.md#update-state-query---update_reboot_state).

| Exit code | Stored library state | Reported when | Next step |
|:---------:|----------------------|---------------|-----------|
| 20 | `FAILED_APP_UPDATE` (6) | Always | `--commit_update` (16), see below |
| 21 | `FAILED_FW_UPDATE` (5) | Always | `--commit_update` (16), see below |
| 22 | `FW_UPDATE_REBOOT_FAILED` (1) | Always | `--commit_update` (59) |
| 23 | `INCOMPLETE_FW_UPDATE` (2) | The update's reboot happened: into the new firmware slot, or a bootloader fallback to the old one | `--commit_update`; after a fallback, commit rather than roll back (see [`--rollback_update`](#--rollback_update)) |
| 24 | `INCOMPLETE_APP_UPDATE` (3) | The new application image is mounted | `--commit_update` |
| 25 | `INCOMPLETE_APP_FW_UPDATE` (4) | As for 23 | `--commit_update`; after a fallback, commit rather than roll back (see [`--rollback_update`](#--rollback_update)) |
| 26 | 2, 3 or 4 | The reboot into the update has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 27 | `NO_UPDATE_REBOOT_PENDING` (0) | Always | None |
| 28 | `ROLLBACK_FW_REBOOT_PENDING` (7) | A firmware slot switch, before and after its reboot | Reboot once, then `--commit_update`; see [below](#rebooting-after-a-firmware-rollback-or-slot-switch) |
| 29 | `ROLLBACK_APP_REBOOT_PENDING` (8) | The rollback's reboot has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 30 | `ROLLBACK_APP_FW_REBOOT_PENDING` (9) | The rollback's reboot has not happened, or a combined update was rolled back before its own reboot | `--apply_update`, then `--commit_update` after the reboot; for a combined update rolled back before its reboot, see [`--rollback_update`](#--rollback_update) |
| 31 | 7, or `INCOMPLETE_FW_ROLLBACK` (10) | 7: a firmware update rolled back after its reboot, before and after the rollback's reboot, or a slot switch whose boot fell back to the previous slot; 10: always | 7: as for 28; 10: `--commit_update` (16; it can refuse with 18) |
| 32 | 8, or `INCOMPLETE_APP_ROLLBACK` (11) | 8: the rollback's reboot took effect; 11: always | `--commit_update` (16) |
| 33 | 9, or `INCOMPLETE_APP_FW_ROLLBACK` (12) | 9: the rollback's reboot took effect; 12: always | `--commit_update` (16; for 12 it can refuse with 18) |
| 55 | 2, 3 or 4 | Whether the reboot took effect cannot be told (`RebootCompleteState::INDETERMINATE`), see below | See below |
| 57 | 8 | No application image is mounted (`classify_app_rollback()` answers `INDETERMINATE`), or the loop devices cannot be read | Nothing mounted: `--commit_update` settles it. Loop devices unreadable: the commit reads the same evidence and fails with 19 until they can be read |
| 124 | `UNKNOWN_STATE` (13) | The stored value is absent, unreadable or not interpretable; the message goes to stderr | Nothing in the CLI leads out; see the library's recovery for `UNKNOWN_STATE` |

#### Rebooting after a firmware rollback or slot switch

For state 7 the exit code tells how the state came about, not whether its
reboot has happened: a slot switch reports 28 and a rolled-back firmware update
31, both before and after the reboot. Reboot once (`--apply_update`), then run
`--commit_update`. Before that reboot the commit is refused with 18 and changes
nothing; after it, the commit settles the state with 16. `--apply_update`
reboots on every 7, so a caller that has already rebooted commits directly.
For 8 and 9 the codes tell the two apart (29 and 32, 30 and 33). The evidence
the commit reads is described in the library's
[transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
(Phase 4).

**Failed installs (20, 21, 22) are settled by `--commit_update`, not rolled
back.** A failed install never left the proven slot, so there is nothing to
undo. `--rollback_update`, `--switch_fw_slot` and `--switch_app_slot` refuse
these states: they print the state and exit with its code from the table
above. For 20 and 21 the commit checks that the slot bitfield matches the
stored state and exits 18 when it does not. For 22 it has no precondition and
exits 59. The per-state recovery, including what to do when the commit is
refused, is in the library's
[Stale and stuck states](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#stale-and-stuck-states)
table.

**Exit 55** has three sources, one per stored state:

- **2:** the firmware install was interrupted before its slot led the boot
  order. `--commit_update` settles it with no reboot: exit 58 when the slot
  was never taken out of the rotation (it is quarantined), 16 when the install
  was interrupted during the image write. Either way the update is discarded
  and its slot quarantined; this 16 confirms nothing. The library describes both in
  [The install's two interruption windows](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#the-installs-two-interruption-windows).
- **3:** no application image is mounted at all. The library's
  [transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
  (Phase 2) describes this shape. `--commit_update` refuses it and exits 19;
  `--rollback_update` settles it with no reboot (exit 12, state 0).
- **4:** a firmware slot is in flight that does not lead the boot order while
  the combined state is stored. The library's own writes do not produce this
  shape (an interrupted combined install is stored as 2, see above). Neither
  verb leads out cleanly: while `BOOT_ORDER` equals `BOOT_ORDER_OLD`,
  `--commit_update` fails with 19 (with differing orders the result follows the
  boot evidence, e.g. 16 after a failed reboot, 18 while the reboot is
  missing), and
  `--rollback_update` is refused for the firmware half.

---

## Category B: Rollback and slot management

### `--rollback_update`

Roll back the pending update: the firmware, the application, or both,
whichever the stored state names. Local durable-state operation — no D-Bus
call.

Acts only while an update is pending (library states 2–4, which
`--update_reboot_state` reports as 23–26 or 55). In every other state, a
failed install included, it changes nothing, prints the state and exits with
that state's `--update_reboot_state` code.

What the rollback stores depends on whether the update's reboot has happened;
the library's
[transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
(Phase 3) lists every case. Run `--update_reboot_state` afterwards and follow
its next step: 27 means the device is already back on the proven slot, 29–31
need a reboot and then a commit (see
[Rebooting after a firmware rollback or slot switch](#rebooting-after-a-firmware-rollback-or-slot-switch)).
After a bootloader fallback the reboot has already undone the firmware. On
state 2 the rollback changes nothing, still exits 12, and
`--update_reboot_state` keeps reporting 23. On state 4 it still points the
application back and stores 9, whose commit settles the failed firmware slot
without marking it bad. Commit rather than roll back after a fallback; see the
library's
[transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
(Phase 3). A combined update rolled
back before its reboot stores a state that no verb leads out of; see
[Rolling back a combined update before its reboot](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#rolling-back-a-combined-update-before-its-reboot).

| Exit code | Meaning |
|:---------:|---------|
| 12 | Rollback prepared (after a bootloader fallback on state 2, nothing changed; see above) |
| 13 | Progress error |
| 14 | Internal error |
| 15 | System error |
| 54 | Refused: target slot is marked bad or uncommitted, or the firmware install never reached the boot order |
| 56 | Refused: target slot was never provisioned (no image installed there) |

### `--switch_fw_slot`

Switch the active firmware slot (A → B or B → A) without going through a
full update cycle. Same error mapping as `--rollback_update` above
(shared classifier). A reboot via `--apply_update` is required.

Acts only when nothing is pending (`--update_reboot_state` would exit 27). In
any other state it changes nothing, prints the state and exits with that
state's `--update_reboot_state` code. The same holds for `--switch_app_slot`.

Same exit code range as `--rollback_update` (12–15, 54).

### `--switch_app_slot`

Switch the active application slot (A → B or B → A). Same semantics as
`--switch_fw_slot`, including the shared error mapping.

Same exit code range (12–15, 54, 56).

### `--apply_update`

Complete a pending install or rollback. Two paths, chosen automatically:

| Condition | Action |
|-----------|--------|
| The service reports `InstallState == "finished"` | Calls `Apply` on the service, then reboots if it reports a reboot is required |
| The service reports `InstallState == "in_progress"` | Refuses (51); applying mid-install would reboot during the write |
| Anything else | Reads the durable `update_reboot_state` via the library and reboots if that state needs it |

The reboot is requested from init: the CLI flushes the file systems
(`sync()`) and sends `SIGINT` to PID 1, which a systemd system handles as an
orderly reboot. Neither path creates a file or waits on anything external;
`--apply_update` either reboots or reports there was nothing to do.

| Exit code | Meaning |
|:---------:|---------|
| 50 | Applied (reboot performed, or none was needed) |
| 51 | Nothing to apply, an install still in progress, or the service `Apply` call failed |
| 70 | Signalling PID 1 for the reboot failed; see stderr |

---

## Category C: Network update state (D-Bus, read-only from the CLI's side)

These arguments read the D-Bus service's session state. In a cloud (ADU)
deployment the ADU handler is its own direct D-Bus client for the equivalent
calls (`StartDownload`/`StartInstall`/`Apply`) and does not invoke these;
they exist for manual/diagnostic use, or for an integrator that wants to
drive the same session through the CLI instead of writing its own D-Bus
client. See [D-Bus Session Protocol](../integration/dbus-session-protocol.md)
for the underlying `CheckUpdateAvailable`/`StartDownload`/... calls and the
`de.fsembedded.fsupdate1` interface reference for the full contract.

### `--is_update_available`

Calls `CheckUpdateAvailable` and prints the result (type, version, size).

| Exit code | Meaning |
|:---------:|---------|
| 34 | No update available |
| 35 | Firmware update available |
| 36 | Application update available |
| 37 | Firmware + application available |

### `--download_update`

Reports whether a download is currently in progress on the service. Does
**not** start a download itself — the update source (the ADU handler, over
D-Bus) does that with `StartDownload`; this command only observes
`DownloadState`.

| Exit code | Meaning |
|:---------:|---------|
| 38 | Nothing queued |
| 40 | A download is already in progress |

Codes 39 (`UPDATE_DOWNLOAD_STARTED`) and 41 (`UPDATE_DOWNLOAD_FAILED`) are
reserved by the stability contract but not produced by this command — it
never initiates or fails a download itself.

### `--download_progress`

Reads `DownloadState`/`DownloadProgress` and reports percentage to stdout.

| Exit code | Meaning |
|:---------:|---------|
| 42 | No download active, or the last one failed |
| 44 | Downloading (percentage printed to stdout) |
| 45 | Download complete |

Code 43 (`UPDATE_DOWNLOAD_WAITING_TO_START`) is reserved but not produced —
the session model has no separate "waiting to start" state.

---

## Category D: Query (read-only)

### `--firmware_version`

Print the current firmware version to stdout; the library reads it from the
file named in its
[Versions](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/api.md#versions)
entry. Exits 0 on success.

### `--application_version`

Print the current application version to stdout, read the same way as
`--firmware_version`. Exits 0 on success.

### `--version`

Print the CLI version (set in `CMakeLists.txt`), the source revision of the CLI
and of the loaded library, and the build date and time to stdout. Exits 0, or
124 if `fw_env.config` cannot be read (see
[Return Codes](return-codes.md#fatal)). Running `fs-updater` with no action
prints the same line.

---

## Category E: State-bad flags

These arguments read and set the bad mark of one slot in the `update` U-Boot
variable (format in the library's
[`update` variable format](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/uboot-variables.md#update-variable-format)).
A slot marked bad is refused as a rollback or switch target.

### `--set_app_state_bad <A|B>`

Mark the specified application slot as bad.

### `--is_app_state_bad <A|B>`

Query whether the specified application slot is marked bad. Prints `1` (bad)
or `0` (not bad) to stdout; for an invalid slot it prints `X`.

### `--set_fw_state_bad <A|B>`

Mark the specified firmware slot as bad.

### `--is_fw_state_bad <A|B>`

Query whether the specified firmware slot is marked bad. Output as for
`--is_app_state_bad`.

The slot is `A` or `B`, in either case. All four arguments share the same
exit codes:

| Exit code | Meaning |
|:---------:|---------|
| 52 | Operation successful |
| 53 | Invalid slot parameter (not `A` or `B`) |
| 124 | The `update` variable could not be read or written; the message goes to stderr |

---

## Category F: Modifiers

### `--debug`

Raise the log level from warnings to debug output. The log goes to stdout, or
to the serial console with `--serial`. Combinable with any action argument.

### `--serial`

Send log output to the serial console instead of stdout. The device is the one
named in the U-Boot `console` variable. Combinable with any action argument,
like `--debug`.

```bash
fs-updater --debug --serial --install_update /mnt/usb/firmware.fs
```

---

## U-Boot variables

The variables, their accepted values and every writer are described once, in
the library's
[U-Boot Variables](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/uboot-variables.md#variable-reference)
reference. The CLI reaches them through the library, the state-bad flags
(Category E) included. Its one direct read is `console`, which `--serial` uses
to find the serial device it writes the log to.

---

## CLI validation errors

| Exit code | Meaning |
|:---------:|---------|
| 61 | Path passed to `--install_update` is empty, does not exist or cannot be resolved (an existing but unreadable file: 49) |
| 65 | Multiple mutually exclusive action flags passed, a path without `--install_update`, or `--detach` without a path |
| 1 | Any option value the parser rejects outright — not a decimal number where one is required, a state letter that isn't exactly one character, an unknown option — exits `1` with usage printed, ahead of the codes above |

Codes 60 (`INVALID_UPDATE_TYPE`), 62 (`MISSING_ENV_UPDATE_STICK`), 63
(`MISSING_ENV_UPDATE_FILE`), and 64 (`UPDATE_TYPE_WITHOUT_FILE`) are reserved
by the stability contract — never reused for something else — and never
produced by this CLI.

## Fatal errors

| Exit code | Meaning |
|:---------:|---------|
| 124 | An exception reached `main()` (see [Return Codes](return-codes.md#fatal)), the stored update state is not interpretable (`--update_reboot_state`), or the update state could not be accessed (state-bad flags) |
