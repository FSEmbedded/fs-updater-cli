# fs-updater CLI Reference

Command-line interface for the F&S Update Framework. Binary: `fs-updater`, installed to `/usr/sbin/`.

## Architecture

See [diagrams/architecture.md](../../docs/cli/diagrams/architecture.md) for component layers, CLI dispatch flow, and library internal structure.

## Parameter Table

All parameters are mutually exclusive except `--debug` (combinable with any) and `--update_type` (only with `--update_file` or `--automatic`).

### Category A: Core Update Flow

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 1 | `--update_file <path>` | ValueArg | Install update from local file | 0-11 | [01](../../docs/cli/workflows/01-update-file.md) |
| 2 | `--update_type <fw\|app>` | ValueArg | Restrict update to fw or app | -- | [02](../../docs/cli/workflows/02-update-type.md) |
| 3 | `--commit_update` | SwitchArg | Confirm update/rollback/switch/fail | 16-19 | [03](../../docs/cli/workflows/03-commit-update.md) |
| 4 | `--update_reboot_state` | SwitchArg | Get current update state | 20-33 | [04](../../docs/cli/workflows/04-update-reboot-state.md) |
| 5 | `--automatic` | SwitchArg | Auto update via env vars | 0-11 | [05](../../docs/cli/workflows/05-automatic.md) |

### Category B: Rollback & Slot Management

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 14 | `--apply_update` | SwitchArg | Apply update/rollback + reboot | 50-51 | [14](../../docs/cli/workflows/14-apply-update.md) |
| 15 | `--rollback_update` | SwitchArg | Rollback to previous version | 12-15 | [15](../../docs/cli/workflows/15-rollback-update.md) |
| 16 | `--switch_fw_slot` | SwitchArg | Switch firmware A<->B | 12-15 | [16](../../docs/cli/workflows/16-switch-fw-slot.md) |
| 17 | `--switch_app_slot` | SwitchArg | Switch application A<->B | 12-15 | [17](../../docs/cli/workflows/17-switch-app-slot.md) |

### Category C: Network Update Pipeline

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 10 | `--is_update_available` | SwitchArg | Check if update exists on server | 34-37 | [10](../../docs/cli/workflows/10-is-update-available.md) |
| 11 | `--download_update` | SwitchArg | Start update download | 38-41 | [11](../../docs/cli/workflows/11-download-update.md) |
| 12 | `--download_progress` | SwitchArg | Show download progress | 42-45 | [12](../../docs/cli/workflows/12-download-progress.md) |
| 13 | `--install_update` | SwitchArg | Install downloaded update | 46-49 | [13](../../docs/cli/workflows/13-install-update.md) |

### Category D: Query (read-only)

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 7 | `--firmware_version` | SwitchArg | Print firmware version | 0 | [07](../../docs/cli/workflows/07-firmware-version.md) |
| 8 | `--application_version` | SwitchArg | Print application version | 0 | [08](../../docs/cli/workflows/08-application-version.md) |
| 9 | `--version` | SwitchArg | Print CLI version + build date | 0 | [09](../../docs/cli/workflows/09-version.md) |

### Category E: State Bad Flags

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 18 | `--set_app_state_bad <A\|B>` | ValueArg | Mark app slot as bad | 52-53 | [18](../../docs/cli/workflows/18-set-app-state-bad.md) |
| 19 | `--is_app_state_bad <A\|B>` | ValueArg | Check if app slot is bad | 52-53 | [19](../../docs/cli/workflows/19-is-app-state-bad.md) |
| 20 | `--set_fw_state_bad <A\|B>` | ValueArg | Mark firmware slot as bad | 52-53 | [20](../../docs/cli/workflows/20-set-fw-state-bad.md) |
| 21 | `--is_fw_state_bad <A\|B>` | ValueArg | Check if firmware slot is bad | 52-53 | [21](../../docs/cli/workflows/21-is-fw-state-bad.md) |

### Category F: Modifiers

| # | Parameter | Type | Description | Return Codes | Workflow |
|---|-----------|------|-------------|:------------:|----------|
| 6 | `--debug` | SwitchArg | Enable debug logging | -- | [06](../../docs/cli/workflows/06-debug.md) |

## State Machine

13-state machine defined in `updateDefinitions.h` (`UBootBootstateFlags`, values 0-12 + UNKNOWN=13).

See [diagrams/state-machine.md](../../docs/cli/diagrams/state-machine.md) for state overview table, full transition diagram, update lifecycle, and rollback lifecycle.

## U-Boot Variables

| Variable | Type | Values | Purpose |
|----------|------|--------|---------|
| `update` | char[4] | 0/1/2 per slot | Slot state: 0=committed, 1=uncommitted, 2=bad |
| `update_reboot_state` | uint8 | 0-12 | State machine position |
| `BOOT_ORDER` | string | "A B" / "B A" | Boot slot priority |
| `BOOT_ORDER_OLD` | string | "A B" / "B A" | Previous boot order (rollback reference) |
| `BOOT_A_LEFT` | uint8 | 0-3 | Remaining boot attempts slot A |
| `BOOT_B_LEFT` | uint8 | 0-3 | Remaining boot attempts slot B |
| `rauc_cmd` | string | "rauc.slot=A/B" | Current running slot (from kernel cmdline) |
| `application` | char | A / B | Active application slot |

### `update` Variable Encoding

4-character string, each position is a slot state:

```
Position: [0]        [1]        [2]        [3]
Slot:     FW_A       APP_A      FW_B       APP_B
Values:   0=committed  1=uncommitted  2=bad
```

Example: `"0010"` = FW_A committed, APP_A committed, FW_B uncommitted, APP_B committed

## Signal Files

Work directory: `/tmp/adu/.work/` (default, `TEMP_ADU_WORK_DIR`). Used for ADU agent communication in network update pipeline.

See [diagrams/signal-files.md](diagrams/signal-files.md) for file overview, network update pipeline, local update flow, and rollback signal flow.

## Source Files

| File | Content |
|------|---------|
| `src/main.cpp` | Entry point |
| `src/cli/cli.h` | CLI class, 21 TCLAP args |
| `src/cli/cli.cpp` | CLI dispatch logic (~1350 lines) |
| `src/cli/fs_updater_error.h` | 55 return codes, 12 enum classes |
| `src/cli/fs_updater_types.h` | `version_t` typedef |
| `../fs-updater-lib/src/handle_update/fsupdate.h` | FSUpdate public API |
| `../fs-updater-lib/src/handle_update/updateDefinitions.h` | 13-state machine enum |
| `../fs-updater-lib/src/handle_update/handleUpdate.cpp` | Bootstate logic |

## See Also

- [Return Codes](RETURN_CODES.md) — complete return code reference
- [Working Procedure](../../docs/cli/WORKING_PROCEDURE.md) — recommended order for workflow improvements
- [ADU Integration](../../docs/cli/ADU_INTEGRATION.md) — Azure Device Update handler + adu-shell task dispatch
- [RAUC D-Bus Plan](../../docs/cli/RAUC_DBUS_PLAN.md) — planned replacement of subprocess RAUC with sd-bus
- [Library Architecture](../../fs-updater-lib/docs/architecture.md)
- [Safety Rules](../../docs/safety-rules.md) — state machine critical rules
- [U-Boot Selector Analysis](../../docs/uboot-selector-analysis.md)
