# Return Codes

All exit codes from `src/cli/fs_updater_error.h`. Valid POSIX range is 0–125
(126+ reserved by POSIX for exec failures and signals).

## Stability contract

- **Append-only.** Never renumber or reuse an existing value — scripts depend on stable numbers.
- **New categories start at the next free value** above the highest assigned code. Reserve 4–5 slots per category for future additions.
- **Values 55 and 57** are claimed by `UPDATER_UPDATE_REBOOT_STATE`, **56** by `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED`, and **58–59** by `UPDATER_COMMIT_STATE`.
- **Do not exceed 125.** Values 124–125 are reserved for fatal/framework-level codes.
- **A code being listed here does not mean today's binary can produce it.** The
  CLI's install path moved to a D-Bus-backed service since these numbers were
  assigned; several are reserved-but-currently-unreachable rather than retired
  — the table below marks each one explicitly rather than silently dropping it,
  because the append-only rule means a script may still be watching for it.

---

## Update install (`--install_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 0 | `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL` | Firmware installed |
| 1 | `UPDATER_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, not currently produced)* |
| 2 | `UPDATER_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, not currently produced)* |
| 3 | `UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR` | Firmware install failed |
| 4 | `UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Application installed |
| 5 | `UPDATER_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, not currently produced)* |
| 6 | `UPDATER_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, not currently produced)* |
| 7 | `UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | Application install failed |
| 8 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Both installed |
| 9 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | *(reserved, not currently produced)* |
| 10 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | *(reserved, not currently produced)* |
| 11 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | Combined install failed |

The install path (`InstallLocal`/`StartInstall` on the D-Bus service) reports
only success or a single system-error verdict per type; it does not currently
distinguish a progress error from an internal one, so 1/2/5/6/9/10 are
reserved slots, not dead numbers to reuse. An install whose type could not be
classified at all reports 49 instead (see `--install_progress` below) rather
than guessing a type family. Start-of-install failures (path not found,
busy, denied) are reported before any of these: see 61/66/67 under
[CLI validation errors](#cli-validation-and-session-errors).

## Install progress (`--install_progress`, and `--install_update`'s in-flight/failed cases)

| Code | Enum | Trigger |
|:----:|------|---------|
| 46 | `UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED` | Nothing tracked |
| 47 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS` | Running (`--detach`, or an already-active session `--install_update` picked up), or the blocking wait hit its no-progress timeout or lost the D-Bus watch — the install may still be running |
| 48 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED` | Finished |
| 49 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED` | Failed, or `--install_update` completed with a type D-Bus couldn't classify |

## Rollback and slot switch (`--rollback_update`, `--switch_fw_slot`, `--switch_app_slot`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 12 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL` | Rollback / switch prepared |
| 13 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR` | Error during rollback |
| 14 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR` | Internal error (rollback) |
| 15 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR` | System error (rollback) |
| 54 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD` | Refused: target slot is marked bad, or (firmware only) the install never reached the boot order — shares the code with the state-bad flags below via a common classifier |
| 56 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED` | Refused: target slot was never provisioned. Not reachable on `--switch_fw_slot`, which has no unprovisioned refusal |

## Commit (`--commit_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 16 | `UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL` | Update / rollback committed |
| 17 | `UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED` | Nothing to commit (idle) |
| 18 | `UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE` | U-Boot state incompatible |
| 19 | `UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR` | System error during commit |
| 58 | `UPDATER_COMMIT_STATE::STALLED_INSTALL_SETTLED` | An install interrupted before its target was activated has been settled: that update is discarded, its slot is quarantined, and the device still runs the firmware it ran before |
| 59 | `UPDATER_COMMIT_STATE::LEGACY_STATE_MIGRATED` | A durable state that no current flow writes has been settled: the device carried it in from a superseded firmware or an edited environment. Nothing was confirmed and nothing discarded |

## Update state query (`--update_reboot_state`)

| Code | Enum | State |
|:----:|------|-------|
| 20 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE` | Application update failed |
| 21 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE` | Firmware update failed |
| 22 | `UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED` | FW installed; bootloader fell back to old slot |
| 23 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE` | FW installed, awaiting reboot |
| 24 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE` | APP installed, awaiting reboot |
| 25 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE` | Both installed, awaiting reboot |
| 26 | `UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING` | Reboot requested |
| 27 | `UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING` | Idle |
| 28 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING` | FW rollback, awaiting reboot |
| 29 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING` | APP rollback, awaiting reboot |
| 30 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_FW_REBOOT_PENDING` | Both rollbacks, awaiting reboot |
| 31 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK` | FW rolled back, awaiting commit |
| 32 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK` | APP rolled back, awaiting commit |
| 33 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK` | Both rolled back, awaiting commit |
| 55 | `UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_STATE_INDETERMINATE` | An update is pending, but the reboot state cannot be told: either whether the reboot took effect is unanswerable, or a firmware install never reached the boot order (`--commit_update` settles it, no reboot needed). The combined app+firmware shape has no clean remedy here |
| 57 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_INDETERMINATE` | An app rollback awaits commit, but no app image is loop-mounted; distinct from 55 so a pending-update watcher does not count a rollback as a pending update |

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
| 39 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED` | *(reserved, not currently produced — this command only observes `DownloadState`, it does not call `StartDownload` itself)* |
| 40 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE` | A download is already in progress |
| 41 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED` | *(reserved, not currently produced — same reason as 39)* |

## Download progress (`--download_progress`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 42 | `UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED` | No download active, or the tracked download failed |
| 43 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START` | *(reserved, not currently produced — no separate "waiting" state on the current session model)* |
| 44 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS` | Downloading (percentage on stdout) |
| 45 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED` | Download complete |

## Apply (`--apply_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 50 | `UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL` | Applied — rebooted, or nothing needed a reboot |
| 51 | `UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED` | Nothing to apply, or the `Apply` D-Bus call failed |

## State-bad flags (`--set_*_state_bad`, `--is_*_state_bad`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 52 | `UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL` | Get / set succeeded |
| 53 | `UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG` | Invalid slot (not `A` or `B`) |
| 54 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD` | Switch rejected: target slot is bad |
| 56 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED` | Switch or rollback rejected: target slot was never provisioned (no image file installed). Not reachable on the firmware switch, which has no unprovisioned refusal |

## CLI validation and session errors

| Code | Enum | Trigger |
|:----:|------|---------|
| 60 | `UPDATER_CLI_VALIDATION::INVALID_UPDATE_TYPE` | *(retired with `--update_type`; reserved, never produced by this CLI)* |
| 61 | `UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND` | Path given to `--install_update` (or the bare operand) does not exist or is not accessible |
| 62 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_STICK` | *(retired with `--automatic`; reserved, never produced by this CLI)* |
| 63 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_FILE` | *(retired with `--automatic`; reserved, never produced by this CLI)* |
| 64 | `UPDATER_CLI_VALIDATION::UPDATE_TYPE_WITHOUT_FILE` | *(retired with `--update_type`; reserved, never produced by this CLI)* |
| 65 | `UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO` | Mutually exclusive flags combined, or a guard rejected the parsed arguments (e.g. `--cancel_install` with a bad session id) |
| 66 | `UPDATER_CLI_VALIDATION::INSTALL_BUSY` | The service rejected the install because another install or download is already in flight (`de.fsembedded.fsupdate1.Error.Busy`; a same-host direct peer with no error name maps its raw `-EBUSY` here too) |
| 67 | `UPDATER_CLI_VALIDATION::PERMISSION_DENIED` | The install request was rejected by policy (polkit / D-Bus bus policy — `org.freedesktop.DBus.Error.AccessDenied`) |

## System-level

| Code | Enum | Trigger |
|:----:|------|---------|
| 70 | `UPDATER_SYSTEM::REBOOT_FAILED` | `reboot(2)` syscall failed; details on stderr |

## Fatal

| Code | Enum | Trigger |
|:----:|------|---------|
| 124 | `UPDATER_FATAL::UNHANDLED_EXCEPTION` | Exception escaped `main()` — framework bug |

## Query success

`--version`, `--firmware_version`, and `--application_version` always exit `0`
on success (no dedicated success enum — they share `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL`
by convention).
