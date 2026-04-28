# CLI Reference

Binary: `fs-updater`, installed to `/usr/sbin/`.

All action arguments are **mutually exclusive** except `--debug` (combinable
with any action) and `--update_type` (modifier for `--update_file` only —
see below).

See [Return Codes](return-codes.md) for the full exit-code table.

---

## Category A: Core update flow

### `--update_file <path>`

Install an update bundle from a local file path.

Two procedures are supported. The new procedure (`.fs` bundle) is preferred
for new integrations; the old procedure (component files + `--update_type`)
remains fully supported. See
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/reference/bundle-format.md)
for both layouts.

**New procedure (`.fs` bundles):** pass a `.fs` archive; the bundle type
(firmware, application, or both) is detected from the embedded
`fsupdate.json`. Do not combine with `--update_type`.

**Old procedure (component files):** pass a raw RAUC artifact (`.raucb`) or
a raw application bundle and specify the component with `--update_type fw`
or `--update_type app`.

```bash
# New procedure — bundle type auto-detected from file
fs-updater --update_file /mnt/usb/update.fs
fs-updater --update_file /mnt/usb/firmware.fs

# Old procedure — explicit type required
fs-updater --update_file /mnt/usb/firmware.raucb --update_type fw
fs-updater --update_file /mnt/usb/application_signed --update_type app
```

| Exit code | Meaning |
|:---------:|---------|
| 0 | Firmware update installed |
| 4 | Application update installed |
| 8 | Firmware + application installed |
| 1/5/9 | Progress error |
| 2/6/10 | Internal error |
| 3/7/11 | System error |
| 61 | File not found |

A reboot is required before `--commit_update`.

### `--update_type <fw|app>`

Specifies the component type for old-format component files used with
`--update_file`. Valid values: `fw` (firmware), `app` (application).

Incompatible with `.fs` bundles — when `--update_type` is set, bundle
extraction is skipped and the file is passed directly to the installer.
Cannot be combined with `--automatic`; doing so returns exit 64.

| Exit code | Meaning |
|:---------:|---------|
| 60 | Value is not `fw` or `app` |
| 64 | Passed without `--update_file` |

### `--commit_update`

Confirm the active update or rollback. Writes to U-Boot environment and
sets `update_reboot_state = 0` (idle).

Must be called after rebooting into the new or rolled-back slot.

| Exit code | Meaning |
|:---------:|---------|
| 16 | Committed successfully |
| 17 | Nothing to commit (already idle) |
| 18 | U-Boot state incompatible |
| 19 | System error |

### `--update_reboot_state`

Query the current 13-state machine position from U-Boot. Outputs a
human-readable string to stdout.

| Exit code | `UBootBootstateFlags` state | Meaning |
|:---------:|-----------------------------|---------|
| 20 | `FAILED_APP_UPDATE` | Application update failed |
| 21 | `FAILED_FW_UPDATE` | Firmware update failed |
| 22 | `FW_UPDATE_REBOOT_FAILED` | FW installed, bootloader fell back to old slot |
| 23 | `INCOMPLETE_FW_UPDATE` | Firmware installed, pending reboot |
| 24 | `INCOMPLETE_APP_UPDATE` | Application installed, pending reboot |
| 25 | `INCOMPLETE_APP_FW_UPDATE` | Both installed, pending reboot |
| 26 | `UPDATE_REBOOT_PENDING` | Reboot requested but not yet performed |
| 27 | `NO_UPDATE_REBOOT_PENDING` | Idle — no pending update |
| 28 | `ROLLBACK_FW_REBOOT_PENDING` | FW rollback pending reboot |
| 29 | `ROLLBACK_APP_REBOOT_PENDING` | APP rollback pending reboot |
| 30 | `ROLLBACK_APP_FW_REBOOT_PENDING` | Both rollbacks pending reboot |
| 31 | `INCOMPLETE_FW_ROLLBACK` | FW rolled back, pending commit |
| 32 | `INCOMPLETE_APP_ROLLBACK` | APP rolled back, pending commit |
| 33 | `INCOMPLETE_APP_FW_ROLLBACK` | Both rolled back, pending commit |

See
[fs-updater-lib state machine](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/state-machine.md)
for the full transition diagram and recovery actions for stuck states.

**Stuck-state quick reference** — states that require explicit action before
the next update will succeed:

| Exit code | Required next action |
|:---------:|---------------------|
| 20 (`FAILED_APP_UPDATE`) | `--rollback_update` → `--apply_update` → reboot → `--commit_update` |
| 21 (`FAILED_FW_UPDATE`) | `--rollback_update` → `--apply_update` → reboot → `--commit_update` |
| 22 (`FW_UPDATE_REBOOT_FAILED`) | `--rollback_update` → `--apply_update` → reboot → `--commit_update` |
| 23–25 (`INCOMPLETE_*`) | Reboot (via `--apply_update`), then `--commit_update` — do **not** call `--commit_update` before rebooting |

### `--automatic`

Install an update from environment variables. Intended for automated
USB-stick update flows. **Supports the new `.fs` bundle procedure only** —
old component files (`.raucb`, raw application) are not supported; use
`--update_file --update_type` for those.

| Variable | Required | Description |
|----------|:--------:|-------------|
| `UPDATE_STICK` | Yes | Mount point of the update media |
| `UPDATE_FILE` | Yes | Filename (relative to `UPDATE_STICK`) to install |

```bash
export UPDATE_STICK=/mnt/usb
export UPDATE_FILE=update.fs
fs-updater --automatic
```

| Exit code | Meaning |
|:---------:|---------|
| 0/4/8 | Install successful (same as `--update_file`) |
| 1–11 | Install errors |
| 62 | `UPDATE_STICK` not set |
| 63 | `UPDATE_FILE` not set |

---

## Category B: Rollback and slot management

### `--rollback_update`

Request a rollback to the previous firmware and/or application version.
Sets `update_reboot_state` to 7, 8, or 9 depending on what is rolling back.
Creates the `rollbackUpdate` signal file. A reboot via `--apply_update` is
required to complete the rollback.

| Exit code | Meaning |
|:---------:|---------|
| 12 | Rollback prepared |
| 13 | Progress error |
| 14 | Internal error |
| 15 | System error |

### `--switch_fw_slot`

Switch the active firmware slot (A → B or B → A) without going through a
full update cycle. Creates the `rollbackUpdate` signal file. A reboot via
`--apply_update` is required.

Same exit code range as `--rollback_update` (12–15).

### `--switch_app_slot`

Switch the active application slot (A → B or B → A). Same semantics as
`--switch_fw_slot`.

Same exit code range (12–15).

### `--apply_update`

Initiate reboot to complete a pending update or rollback.

| Mode | Condition | Action |
|------|-----------|--------|
| Local | No ADU agent signal-file context | Calls `reboot(2)` directly |
| ADU signal-file | `applyUpdate` signal file expected | Creates the `applyUpdate` signal file |

| Exit code | Meaning |
|:---------:|---------|
| 50 | Reboot initiated or apply signal created |
| 51 | Apply signal creation failed, or nothing to apply |
| 70 | `reboot(2)` returned an error; see stderr |

---

## Category C: Network update pipeline

These arguments implement signal-file-based IPC with the ADU agent.
See [Signal Files](../integration/signal-files.md) for the protocol.

### `--is_update_available`

Read `update_type`, `update_version`, `update_size` from the work directory.

| Exit code | Meaning |
|:---------:|---------|
| 34 | No update available |
| 35 | Firmware update available |
| 36 | Application update available |
| 37 | Firmware + application available |

### `--download_update`

Check metadata and create the `downloadUpdate` signal file to trigger the
ADU agent to start the download.

| Exit code | Meaning |
|:---------:|---------|
| 38 | No download queued |
| 39 | Download started |
| 40 | Download already in progress |
| 41 | Failed to start download |

### `--download_progress`

Read `update_location` and compare current vs expected file sizes to report
download progress. Prints percentage to stdout.

| Exit code | Meaning |
|:---------:|---------|
| 42 | No download started |
| 43 | Waiting for download location |
| 44 | Download in progress (percentage printed to stdout) |
| 45 | Download complete |

### `--install_update`

Check that `update_location` exists, then create the `installUpdate` signal
file to trigger the ADU agent to run the installation.

| Exit code | Meaning |
|:---------:|---------|
| 46 | No downloaded file ready |
| 47 | Installation started |
| 48 | Installation finished |
| 49 | Installation failed |

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

```bash
fs-updater --debug --update_file /mnt/usb/firmware.raucb
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
| 60 | `--update_type` value is not `fw` or `app` |
| 61 | Path passed to `--update_file` does not exist |
| 62 | `UPDATE_STICK` environment variable not set (`--automatic`) |
| 63 | `UPDATE_FILE` environment variable not set (`--automatic`) |
| 64 | `--update_type` passed without `--update_file` |
| 65 | Multiple mutually exclusive action flags passed |

## Fatal errors

| Exit code | Meaning |
|:---------:|---------|
| 124 | An exception escaped `main()` — framework bug or unexpected hardware state |
