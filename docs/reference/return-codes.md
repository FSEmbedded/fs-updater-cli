# Return Codes

All exit codes from `src/cli/fs_updater_error.h`. Valid POSIX range is 0–125
(126+ reserved by POSIX for exec failures and signals).

## Stability contract

- **Append-only.** Never renumber or reuse an existing value — scripts depend on stable numbers.
- **New categories start at the next free value** above the highest assigned code. Reserve 4–5 slots per category for future additions.
- **Values 55 and 57** are claimed by `UPDATER_UPDATE_REBOOT_STATE`, **56** by `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED`, and **58–59** by `UPDATER_COMMIT_STATE`.
- **Do not exceed 125.** Values 124–125 are reserved for fatal/framework-level codes.
- **A code being listed here does not mean the binary produces it.** Several
  are reserved and never produced; the tables below mark each one explicitly
  rather than dropping it, because the append-only rule means a script may
  still be watching for it.

---

## Update install (`--install_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 0 | `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL` | Firmware installed |
| 1 | `UPDATER_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, never produced)* |
| 2 | `UPDATER_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, never produced)* |
| 3 | `UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR` | Firmware install failed |
| 4 | `UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Application installed |
| 5 | `UPDATER_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, never produced)* |
| 6 | `UPDATER_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, never produced)* |
| 7 | `UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | Application install failed |
| 8 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Both installed |
| 9 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, never produced)* |
| 10 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, never produced)* |
| 11 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | Combined install failed |

The install path (`InstallLocal`/`StartInstall` on the D-Bus service) reports
only success or a single system-error verdict per type; it does not
distinguish a progress error from an internal one, so 1/2/5/6/9/10 are
reserved slots, not dead numbers to reuse. An install whose type could not be
classified at all reports 48 (success) or 49 (failure) instead (see
`--install_progress` below) rather than guessing a type family. Start-of-install failures (path not found,
busy, denied) are reported before any of these: see 61/66/67 under
[CLI validation errors](#cli-validation-and-session-errors).

## Install progress (`--install_progress`, and `--install_update`'s in-flight/failed cases)

| Code | Enum | Trigger |
|:----:|------|---------|
| 46 | `UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED` | Nothing tracked |
| 47 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS` | Accepted and running: `--detach`, a pathless `--install_update` whose `StartInstall` was accepted, or a local install whose session id could not be read back; a pathless `--install_update` that finds an install already in progress (`InstallState == "in_progress"`, no `StartInstall` call); or the blocking wait hit its no-progress timeout or lost the D-Bus watch — the install may still be running. Also `--cancel_install` when the cancel request was accepted; the outcome follows via `--install_progress` |
| 48 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED` | Finished, or `--install_update` succeeded with an update type that could not be classified |
| 49 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED` | Failed; `--install_update` failed with an update type that could not be classified, or its `InstallLocal`/`StartInstall` call failed; `--cancel_install` with session id `0` or a failed `CancelInstall` call |

## Rollback and slot switch (`--rollback_update`, `--switch_fw_slot`, `--switch_app_slot`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 12 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL` | Rollback / switch prepared |
| 13 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR` | Error during rollback |
| 14 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR` | Internal error (rollback) |
| 15 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR` | System error (rollback) |
| 54 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD` | Refused: target slot is marked bad or uncommitted, (firmware only) the install never reached the boot order, or the durable update state owns no such move (nothing was changed; the message names the state and the way out) |
| 56 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED` | Refused: target slot was never provisioned. Not reachable on `--switch_fw_slot`, which has no unprovisioned refusal |

`--rollback_update`, `--switch_fw_slot` and `--switch_app_slot` share one
errno mapper, so a refusal the library raises for the same reason arrives as
the same code on every door. `--rollback_update` only calls into the library
while a library state 2, 3 or 4 is stored — every other state is answered by
that state's own `--update_reboot_state` code before the library is asked
(see [`--rollback_update`](cli.md#--rollback_update)). Within 2/3/4 the
library can still refuse the call it was given — an update whose own digits
cannot be identified — and that refusal also exits 54, before anything is
staged; see the library's
[transition diagram](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#transition-diagram)
(Phase 3). `--switch_fw_slot` and `--switch_app_slot` call into the library
only from the idle state, so the same refusal there is not reached from the
states a caller normally acts on.

## Commit (`--commit_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 16 | `UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL` | Committed; what it confirmed or discarded per state is listed under [CLI Reference — `--commit_update`](cli.md#--commit_update) |
| 17 | `UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED` | Nothing pending and nothing to restore |
| 18 | `UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE` | U-Boot state incompatible |
| 19 | `UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR` | System error during commit |
| 58 | `UPDATER_COMMIT_STATE::STALLED_INSTALL_SETTLED` | An install interrupted before its target was activated has been settled: that update is discarded, its slot is quarantined, and the device still runs the firmware it ran before |
| 59 | `UPDATER_COMMIT_STATE::LEGACY_STATE_MIGRATED` | The stored state was `FW_UPDATE_REBOOT_FAILED` (1), which the library does not write (it is carried in from an earlier library version or an edited environment), and has been settled. Nothing was confirmed and nothing discarded. The carried-in rollback states 10–12 are committed with 16; see the library's [Stale and stuck states](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#stale-and-stuck-states) |

## Update state query (`--update_reboot_state`)

Which stored library state each code reports, under which condition, and the
next step for it are listed under
[CLI Reference — `--update_reboot_state`](cli.md#--update_reboot_state).

| Code | Enum |
|:----:|------|
| 20 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE` |
| 21 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE` |
| 22 | `UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED` |
| 23 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE` |
| 24 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE` |
| 25 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE` |
| 26 | `UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING` |
| 27 | `UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING` |
| 28 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING` |
| 29 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING` |
| 30 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_FW_REBOOT_PENDING` |
| 31 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK` |
| 32 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK` |
| 33 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK` |
| 55 | `UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_STATE_INDETERMINATE` |
| 57 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_INDETERMINATE` |

A stored state the CLI cannot interpret is answered with 124
(`UPDATER_FATAL::UNHANDLED_EXCEPTION`), not with a code from this range, so a
caller that treats only 27 as "nothing to do" does not act on it.

## Update availability (`--is_update_available`)

Backed by `CheckUpdateAvailable` on the D-Bus service.

| Code | Enum | Trigger |
|:----:|------|---------|
| 34 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE` | No update pending |
| 35 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE` | Firmware update detected |
| 36 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE` | Application update detected |
| 37 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE` | Both detected |

## Download (`--download_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 38 | `UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED` | Nothing queued |
| 39 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED` | *(reserved, never produced — this command only observes `DownloadState`, it does not call `StartDownload` itself)* |
| 40 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE` | A download is already in progress |
| 41 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED` | *(reserved, never produced — same reason as 39)* |

## Download progress (`--download_progress`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 42 | `UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED` | No download active, or the tracked download failed |
| 43 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START` | *(reserved, never produced — the session model has no separate "waiting" state)* |
| 44 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS` | Downloading (percentage on stdout) |
| 45 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED` | Download complete |

## Apply (`--apply_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 50 | `UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL` | Applied — rebooted, or nothing needed a reboot |
| 51 | `UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED` | Nothing to apply, an install still in progress, or the `Apply` D-Bus call failed |

## State-bad flags (`--set_*_state_bad`, `--is_*_state_bad`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 52 | `UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL` | Get / set succeeded |
| 53 | `UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG` | Invalid slot (not `A` or `B`) |

54 and 56 belong to the same enum but are produced only by the rollback and
slot-switch verbs; see [Rollback and slot switch](#rollback-and-slot-switch---rollback_update---switch_fw_slot---switch_app_slot).
An unreadable `update` variable makes these four flags exit 124.

## CLI validation and session errors

| Code | Enum | Trigger |
|:----:|------|---------|
| 60 | `UPDATER_CLI_VALIDATION::INVALID_UPDATE_TYPE` | *(reserved, never produced by this CLI)* |
| 61 | `UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND` | Path given to `--install_update` is empty, does not exist or cannot be resolved by the CLI (`realpath`). A path that resolves is checked again by the service, which runs as root, with `access(R_OK)`; when that check fails the call fails and the CLI exits 49 |
| 62 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_STICK` | *(reserved, never produced by this CLI)* |
| 63 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_FILE` | *(reserved, never produced by this CLI)* |
| 64 | `UPDATER_CLI_VALIDATION::UPDATE_TYPE_WITHOUT_FILE` | *(reserved, never produced by this CLI)* |
| 65 | `UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO` | Mutually exclusive action flags combined, a path without `--install_update`, or `--detach` without a path |
| 66 | `UPDATER_CLI_VALIDATION::INSTALL_BUSY` | The service rejected the install because another install or download is already in flight (`de.fsembedded.fsupdate1.Error.Busy`; a same-host direct peer with no error name maps its raw `-EBUSY` here too) |
| 67 | `UPDATER_CLI_VALIDATION::PERMISSION_DENIED` | The install request was rejected by policy (polkit / D-Bus bus policy — `org.freedesktop.DBus.Error.AccessDenied`) |

## System-level

| Code | Enum | Trigger |
|:----:|------|---------|
| 70 | `UPDATER_SYSTEM::REBOOT_FAILED` | Signalling PID 1 for the reboot failed; details on stderr |

## Fatal

| Code | Enum | Trigger |
|:----:|------|---------|
| 124 | `UPDATER_FATAL::UNHANDLED_EXCEPTION` | An exception reached `main()` uncaught by the command's handler. Among them: the library is constructed before any action runs, `--version` and the no-action line included, and its construction throws when `libubootenv` cannot be initialised or `fw_env.config` cannot be read (see the library's [Construction](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/api.md#construction)); with `--serial`, the same startup also throws when the U-Boot `console` variable cannot be read; `--firmware_version` and `--application_version` exit 124 when the version cannot be read (see the library's [Versions](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/api.md#versions)). Also the answer of `--update_reboot_state` for a stored state it cannot interpret, and of the state-bad flags when the `update` variable cannot be accessed |

## Query success

`--version`, `--firmware_version`, and `--application_version` exit `0`
on success (124 on the failures listed under [Fatal](#fatal)) (no dedicated success enum — they share `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL`
by convention).
