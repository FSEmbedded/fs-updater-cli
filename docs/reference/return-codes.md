# Return Codes

All exit codes from `src/cli/fs_updater_error.h`. Valid POSIX range is 0–125
(126+ reserved by POSIX for exec failures and signals).

## Stability contract

- **Append-only.** Never renumber or reuse an existing value — scripts depend on stable numbers.
- **New categories start at the next free value ≥ 55.** Reserve 4–5 slots per category for future additions.
- **Values 55–59** are reserved for future `UPDATER_SETGET_UPDATE_STATE` extension.
- **Do not exceed 125.** Values 124–125 are reserved for fatal/framework-level codes.

---

## Update install (`--update_file`, `--automatic`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 0 | `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL` | Firmware installed |
| 1 | `UPDATER_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR` | Error during firmware install |
| 2 | `UPDATER_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR` | Internal error (firmware) |
| 3 | `UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR` | System error (firmware) |
| 4 | `UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Application installed |
| 5 | `UPDATER_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | Error during application install |
| 6 | `UPDATER_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | Internal error (application) |
| 7 | `UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | System error (application) |
| 8 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Both installed |
| 9 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | Error during combined install |
| 10 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | Internal error (combined) |
| 11 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | System error (combined) |

## Rollback and slot switch (`--rollback_update`, `--switch_fw_slot`, `--switch_app_slot`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 12 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL` | Rollback / switch prepared |
| 13 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR` | Error during rollback |
| 14 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR` | Internal error (rollback) |
| 15 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR` | System error (rollback) |

## Commit (`--commit_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 16 | `UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL` | Update / rollback committed |
| 17 | `UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED` | Nothing to commit (idle) |
| 18 | `UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE` | U-Boot state incompatible |
| 19 | `UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR` | System error during commit |

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

## Network update availability (`--is_update_available`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 34 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE` | No update in work directory |
| 35 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE` | Firmware update detected |
| 36 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE` | Application update detected |
| 37 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE` | Both detected |

## Download (`--download_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 38 | `UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED` | No update ready to download |
| 39 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED` | Download initiated |
| 40 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE` | Download already in progress |
| 41 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED` | Failed to start download |

## Download progress (`--download_progress`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 42 | `UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED` | No download active |
| 43 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START` | Waiting for download location |
| 44 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS` | Downloading (percentage on stdout) |
| 45 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED` | Download complete |

## Installation (`--install_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 46 | `UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED` | No downloaded file ready |
| 47 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS` | Installation started |
| 48 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED` | Installation complete |
| 49 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED` | Installation failed |

## Apply (`--apply_update`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 50 | `UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL` | Reboot initiated or apply signal created |
| 51 | `UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED` | Failed to reboot or create signal |

## State-bad flags (`--set_*_state_bad`, `--is_*_state_bad`)

| Code | Enum | Trigger |
|:----:|------|---------|
| 52 | `UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL` | Get / set succeeded |
| 53 | `UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG` | Invalid slot (not `A` or `B`) |
| 54 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD` | Switch rejected: target slot is bad |

## CLI validation

| Code | Enum | Trigger |
|:----:|------|---------|
| 60 | `UPDATER_CLI_VALIDATION::INVALID_UPDATE_TYPE` | `--update_type` not `fw` or `app` |
| 61 | `UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND` | Path given to `--update_file` does not exist |
| 62 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_STICK` | `UPDATE_STICK` not set (`--automatic`) |
| 63 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_FILE` | `UPDATE_FILE` not set (`--automatic`) |
| 64 | `UPDATER_CLI_VALIDATION::UPDATE_TYPE_WITHOUT_FILE` | `--update_type` without `--update_file` |
| 65 | `UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO` | Mutually exclusive flags combined |

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
