# fs-updater Return Codes

All return codes from `fs-updater-cli/src/cli/fs_updater_error.h`. Usable POSIX exit-code range is 0-125 (126+ reserved by POSIX for exec failures and signals).

## Extension Rule

- **Append-only.** Never renumber or reuse an existing value — scripted callers depend on stable numbers.
- **New categories start at the next free value ≥ 55.** Reserve small gaps (4-5 slots) within a category to allow later additions to stay range-adjacent to siblings.
- **Do not exceed 125.** Values 124-125 are reserved for fatal/framework-level codes (see `UPDATER_FATAL`).
- **Values 55-59** are reserved for future `UPDATER_SETGET_UPDATE_STATE` extension.

## Firmware Update (`--update_file`, `--automatic`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 0 | `UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL` | Firmware update installed successfully |
| 1 | `UPDATER_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR` | Error during firmware update progress |
| 2 | `UPDATER_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR` | Internal framework error (firmware) |
| 3 | `UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR` | System error during firmware update |

## Application Update (`--update_file`, `--automatic`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 4 | `UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Application update installed successfully |
| 5 | `UPDATER_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | Error during application update progress |
| 6 | `UPDATER_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | Internal framework error (application) |
| 7 | `UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | System error during application update |

## Firmware + Application Update (`--update_file`, `--automatic`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 8 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL` | Both updates installed successfully |
| 9 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR` | Error during combined update progress |
| 10 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR` | Internal framework error (combined) |
| 11 | `UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR` | System error during combined update |

## Rollback (`--rollback_update`, `--switch_fw_slot`, `--switch_app_slot`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 12 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL` | Rollback/switch prepared successfully |
| 13 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR` | Error during rollback progress |
| 14 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR` | Internal framework error (rollback) |
| 15 | `UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR` | System error during rollback |

## Commit (`--commit_update`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 16 | `UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL` | Update/rollback committed |
| 17 | `UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED` | No pending update to commit |
| 18 | `UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE` | U-Boot state incompatible |
| 19 | `UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR` | System error during commit |

## Reboot State (`--update_reboot_state`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 20 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE` | Application update failed |
| 21 | `UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE` | Firmware update failed |
| 22 | `UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED` | Firmware reboot failed (booted old slot) |
| 23 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE` | Firmware update pending reboot |
| 24 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE` | Application update pending reboot |
| 25 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE` | Both updates pending reboot |
| 26 | `UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING` | Reboot pending (reboot not yet complete) |
| 27 | `UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING` | Idle, no pending update |
| 28 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING` | Firmware rollback pending reboot |
| 29 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING` | App rollback pending reboot |
| 30 | `UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_FW_REBOOT_PENDING` | Both rollback pending reboot |
| 31 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK` | Firmware rollback incomplete after reboot |
| 32 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK` | App rollback incomplete after reboot |
| 33 | `UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK` | Both rollback incomplete after reboot |

## Update Availability (`--is_update_available`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 34 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE` | No update found |
| 35 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE` | Firmware update available |
| 36 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE` | Application update available |
| 37 | `UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE` | Both updates available |

## Download (`--download_update`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 38 | `UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED` | No update to download |
| 39 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED` | Download initiated |
| 40 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE` | Download already in progress |
| 41 | `UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED` | Failed to start download |

## Download Progress (`--download_progress`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 42 | `UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED` | No download in progress |
| 43 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START` | Waiting for download location |
| 44 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS` | Download in progress (prints percentage) |
| 45 | `UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED` | Download complete (100%) |

## Installation (`--install_update`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 46 | `UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED` | No download ready to install |
| 47 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS` | Installation initiated/ongoing |
| 48 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED` | Installation complete |
| 49 | `UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED` | Installation failed |

## Apply (`--apply_update`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 50 | `UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL` | Reboot initiated or apply signal created |
| 51 | `UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED` | Failed to reboot or create signal |

## State Bad Flags (`--set_*_state_bad`, `--is_*_state_bad`)

| Code | Enum | Meaning |
|:----:|------|---------|
| 52 | `UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL` | Get/set operation successful |
| 53 | `UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG` | Invalid state parameter |
| 54 | `UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD` | Slot switch rejected: target state is bad |

## CLI Validation (`--update_file`, `--automatic`, argument combinations)

| Code | Enum | Meaning |
|:----:|------|---------|
| 60 | `UPDATER_CLI_VALIDATION::INVALID_UPDATE_TYPE` | `--update_type` value is not `fw` or `app` |
| 61 | `UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND` | Path passed to `--update_file` does not exist |
| 62 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_STICK` | `UPDATE_STICK` environment variable not set (`--automatic`) |
| 63 | `UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_FILE` | `UPDATE_FILE` environment variable not set (`--automatic`) |
| 64 | `UPDATER_CLI_VALIDATION::UPDATE_TYPE_WITHOUT_FILE` | `--update_type` passed without `--update_file` |
| 65 | `UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO` | Multiple mutually exclusive action flags were passed |

## System-level Errors

| Code | Enum | Meaning |
|:----:|------|---------|
| 70 | `UPDATER_SYSTEM::REBOOT_FAILED` | `reboot(2)` syscall failed; see stderr for `strerror()` detail |

## Fatal

| Code | Enum | Meaning |
|:----:|------|---------|
| 124 | `UPDATER_FATAL::UNHANDLED_EXCEPTION` | An exception escaped `main()` — framework bug or unexpected state |

## Query Success

| Code | Source | Meaning |
|:----:|--------|---------|
| 0 | `--version`, `--firmware_version`, `--application_version` | Query operation succeeded |

## Known Issues

- `getInfoAboutAboutBundle` in `rauc_handler.h`: double "About" typo in method name (to be renamed in D-Bus client, see [RAUC_DBUS_PLAN.md](../../docs/cli/RAUC_DBUS_PLAN.md))
