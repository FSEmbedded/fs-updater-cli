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
           [--set_app_state_bad <A|B>] [--is_app_state_bad <A|B>]
           [--set_fw_state_bad <A|B>] [--is_fw_state_bad <A|B>]
           [--] [install_path]
```

---

## Category A: Install and commit

### `--install_update [install_path]`

Install an update. Two forms, distinguished by whether a path is given:

- **With a path** (`--install_update /mnt/usb/update.fs`, or a bare trailing
  operand: `fs-updater /mnt/usb/update.fs`): install that local `.fs` bundle
  via `InstallLocal` on the D-Bus service. Blocking by default — the CLI
  streams live progress to stdout and returns the terminal 0/4/8 ÷ 3/7/11
  verdict (see [Return Codes](return-codes.md)) once the service reports the
  install finished or failed. Pass `--detach` to return immediately instead.
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

The old two-step procedure (`--update_file`/`--update_type` for raw
component files, `--automatic` for a USB-stick environment-variable flow)
has been retired; there is one install entry point now. See
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/reference/bundle-format.md)
for the `.fs` container layout.

```bash
# Blocking local install, type auto-detected from the bundle
fs-updater --install_update /mnt/usb/update.fs
fs-updater /mnt/usb/update.fs                 # bare path, same effect

# Start it and come back later
fs-updater --install_update /mnt/usb/update.fs --detach
fs-updater --install_progress
```

| Exit code | Meaning |
|:---------:|---------|
| 0 / 4 / 8 | Firmware / application / firmware+application installed |
| 3 / 7 / 11 | System error (firmware / application / combined) — the only failure variant the D-Bus-backed install path currently produces; 1/2/5/6/9/10 (progress/internal-error) are reserved but not reachable today |
| 46 | No path given, and no finished download staged to advance |
| 47 | Accepted; still running (`--detach`, or a session already in flight whose id could not be read back), or the blocking wait hit its no-progress timeout or lost the D-Bus watch — the install may still be running; poll `--install_progress` |
| 48 | No path given, and the D-Bus service already reports the (other-owned) install finished |
| 49 | Installation failed — used when the outcome's update type could not be classified |
| 61 | Path does not exist or is not accessible |
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

Confirm the active update or rollback, or settle a failed install. Writes to
the U-Boot environment and returns `update_reboot_state` to 0 (idle).

For an update or a rollback, call it after rebooting into the new or
rolled-back slot. Which state it settles, and with which exit code, is listed
per state under [`--update_reboot_state`](#--update_reboot_state).

| Exit code | Meaning |
|:---------:|---------|
| 16 | Committed successfully |
| 17 | Nothing to commit (already idle) |
| 18 | U-Boot state incompatible |
| 19 | System error |
| 58 | An install interrupted before its target was activated has been settled (discarded, slot quarantined) |
| 59 | A durable state from an older generation has been settled (nothing confirmed, nothing discarded) |

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
| 23 | `INCOMPLETE_FW_UPDATE` (2) | The reboot into the new firmware slot took effect | `--commit_update` |
| 24 | `INCOMPLETE_APP_UPDATE` (3) | The new application image is mounted | `--commit_update` |
| 25 | `INCOMPLETE_APP_FW_UPDATE` (4) | The reboot into the new firmware slot took effect | `--commit_update` |
| 26 | 2, 3 or 4 | The reboot into the update has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 27 | `NO_UPDATE_REBOOT_PENDING` (0) | Always | None |
| 28 | `ROLLBACK_FW_REBOOT_PENDING` (7) | The rollback's reboot has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 29 | `ROLLBACK_APP_REBOOT_PENDING` (8) | The rollback's reboot has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 30 | `ROLLBACK_APP_FW_REBOOT_PENDING` (9) | The rollback's reboot has not happened | `--apply_update`, then `--commit_update` after the reboot |
| 31 | 7, or `INCOMPLETE_FW_ROLLBACK` (10) | 7: the rollback's reboot took effect; 10: always | `--commit_update` |
| 32 | 8, or `INCOMPLETE_APP_ROLLBACK` (11) | 8: the rollback's reboot took effect; 11: always | `--commit_update` |
| 33 | 9, or `INCOMPLETE_APP_FW_ROLLBACK` (12) | 9: the rollback's reboot took effect; 12: always | `--commit_update` |
| 55 | 2, 3 or 4 | Whether the reboot took effect cannot be told (`RebootCompleteState::INDETERMINATE`), see below | See below |
| 57 | 8 | No application image is mounted, or the loop devices cannot be read (`classify_app_rollback()` answers `INDETERMINATE`) | `--commit_update` |
| 124 | `UNKNOWN_STATE` (13) | The stored value is absent, unreadable or not interpretable; the message goes to stderr | Nothing in the CLI leads out; see the library's recovery for `UNKNOWN_STATE` |

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

- **2:** the firmware install never reached the boot order. `--commit_update`
  settles it with no reboot, quarantines the slot and exits 58.
- **3:** no application image is mounted at all. The library's
  [transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
  (Phase 2) describes this shape. `--commit_update` refuses it and exits 19.
- **4:** the combined install never reached the boot order. Neither verb
  leads out cleanly: `--commit_update` fails with 19, and `--rollback_update`
  is refused for the firmware half.

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
its next step: 28–30 need `--apply_update` and a commit after the reboot, 27
means the device is already back on the proven slot. A combined update rolled
back before its reboot stores a state that no verb leads out of; see
[Rolling back a combined update before its reboot](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#rolling-back-a-combined-update-before-its-reboot).

| Exit code | Meaning |
|:---------:|---------|
| 12 | Rollback prepared |
| 13 | Progress error |
| 14 | Internal error |
| 15 | System error |
| 54 | Refused: target slot is marked bad, or the firmware install never reached the boot order |
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
| A D-Bus install this CLI process is tracking has finished | Calls `Apply` on the service, then `reboot(2)` directly if it reports a reboot is required |
| No install tracked in this session | Reads the durable `update_reboot_state` via the library and reboots if that state needs it |

Neither path creates a file or waits on anything external; `--apply_update`
either reboots or reports there was nothing to do.

| Exit code | Meaning |
|:---------:|---------|
| 50 | Applied (reboot performed, or none was needed) |
| 51 | Nothing to apply, or the service `Apply` call failed |
| 70 | `reboot(2)` returned an error; see stderr |

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
there is no separate "waiting to start" state on the current session model.

---

## Category D: Query (read-only)

### `--firmware_version`

Print the current firmware version string (from U-Boot environment) to stdout.
Always exits 0 on success.

### `--application_version`

Print the current application version string (from U-Boot environment) to stdout.
Always exits 0 on success.

### `--version`

Print the CLI version and build date to stdout. Version is set in
`CMakeLists.txt`. Always exits 0.

---

## Category E: State-bad flags

These arguments manage the `update` U-Boot variable that marks individual slots
as bad (unbootable). A bad slot is excluded from rollback targets.

### `--set_app_state_bad <A|B>`

Mark the specified application slot as bad.

### `--is_app_state_bad <A|B>`

Query whether the specified application slot is marked bad. Prints `1` (bad)
or `0` (not bad) to stdout.

### `--set_fw_state_bad <A|B>`

Mark the specified firmware slot as bad.

### `--is_fw_state_bad <A|B>`

Query whether the specified firmware slot is marked bad.

All four arguments share the same exit-code range:

| Exit code | Meaning |
|:---------:|---------|
| 52 | Operation successful |
| 53 | Invalid slot parameter (not `A` or `B`) |
| 54 | Slot switch rejected: target slot is bad |

---

## Category F: Modifiers

### `--debug`

Enable verbose debug logging to stderr. Combinable with any action argument.

### `--serial`

Send log output to the serial console instead of the default. Combinable
with any action argument, like `--debug`.

```bash
fs-updater --debug --serial --install_update /mnt/usb/firmware.fs
```

---

## U-Boot variables

| Variable | Values | Written by | Purpose |
|----------|--------|-----------|---------|
| `update` | 4-char string | CLI | Per-slot state: `0`=committed, `1`=uncommitted, `2`=bad. Positions: [0]=FW_A, [1]=APP_A, [2]=FW_B, [3]=APP_B |
| `update_reboot_state` | 0–13 | CLI / dynamic-overlay | 13-state machine position |
| `BOOT_ORDER` | `"A B"` / `"B A"` | CLI / U-Boot | Boot slot priority |
| `BOOT_ORDER_OLD` | `"A B"` / `"B A"` | CLI | Previous boot order, used as rollback reference |
| `BOOT_A_LEFT` | 0–3 | U-Boot | Remaining boot attempts for slot A |
| `BOOT_B_LEFT` | 0–3 | U-Boot | Remaining boot attempts for slot B |
| `rauc_cmd` | `"rauc.slot=A"` / `"rauc.slot=B"` | U-Boot | Currently booted slot (from kernel cmdline) |
| `application` | `A` / `B` | CLI | Active application slot |

**`update` variable example:** `"0010"` = FW_A committed, APP_A committed, FW_B uncommitted, APP_B committed.

---

## CLI validation errors

| Exit code | Meaning |
|:---------:|---------|
| 61 | Path passed to `--install_update` (or the bare operand) does not exist or is not accessible |
| 65 | Multiple mutually exclusive action flags passed |
| 1 | Any option value the parser rejects outright — not a decimal number where one is required, a state letter that isn't exactly one character, an unknown option — exits `1` with usage printed, ahead of the codes above |

Codes 60 (`INVALID_UPDATE_TYPE`), 62 (`MISSING_ENV_UPDATE_STICK`), 63
(`MISSING_ENV_UPDATE_FILE`), and 64 (`UPDATE_TYPE_WITHOUT_FILE`) belonged to
the retired `--update_type`/`--automatic` flags. They are reserved by the
stability contract — never reused for something else — but this CLI no
longer produces them.

## Fatal errors

| Exit code | Meaning |
|:---------:|---------|
| 124 | An exception escaped `main()` — framework bug or unexpected hardware state |
