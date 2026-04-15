# Signal Files

Work directory: `/tmp/adu/.work/` (default, `TEMP_ADU_WORK_DIR`)

## File Overview

| File | Type | Created By | Read By | Purpose |
|------|------|------------|---------|---------|
| `update_type` | Metadata | ADU agent | `--is_update_available` | "firmware", "application", or both |
| `update_version` | Metadata | ADU agent | `--is_update_available` | Version string |
| `update_size` | Metadata | ADU agent | `--is_update_available`, `--download_progress` | Size in bytes |
| `update_location` | Metadata | ADU agent | `--download_progress` | Path to downloading file |
| `downloadUpdate` | Signal | `--download_update` | `--download_progress`, ADU agent | Trigger download |
| `installUpdate` | Signal | `--install_update` | ADU agent | Trigger installation |
| `updateInstalled` | Signal | ADU agent / library | `--install_update`, `--apply_update` | Installation complete |
| `applyUpdate` | Signal | `--apply_update` | ADU agent | Trigger apply (network mode) |
| `rollbackUpdate` | Signal | `--rollback_update`, `--switch_*_slot` | `--apply_update` | Rollback prepared |

## Network Update Pipeline

```mermaid
sequenceDiagram
    participant ADU as ADU Agent
    participant FS as Work Dir Files
    participant CLI as fs-updater

    ADU->>FS: write update_type, update_version, update_size

    CLI->>FS: --is_update_available (read metadata)
    CLI-->>CLI: return 35/36/37

    CLI->>FS: --download_update (create downloadUpdate)
    ADU->>FS: read downloadUpdate, start download
    ADU->>FS: write update_location

    loop Poll progress
        CLI->>FS: --download_progress (read update_location + file size)
        CLI-->>CLI: return 44 (in progress) or 45 (done)
    end

    CLI->>FS: --install_update (create installUpdate)
    ADU->>FS: read installUpdate, run installation
    ADU->>FS: write updateInstalled

    CLI->>FS: --apply_update (create applyUpdate)
    ADU->>FS: read applyUpdate, trigger reboot
```

## Local Update Flow

```mermaid
sequenceDiagram
    participant User
    participant CLI as fs-updater
    participant FS as Work Dir Files

    User->>CLI: --update_file /mnt/usb/update.fs
    Note over CLI: Library installs directly,<br/>no signal files needed

    CLI->>FS: write updateInstalled

    User->>CLI: --apply_update
    Note over FS: No applyUpdate/downloadUpdate files
    CLI->>CLI: Direct reboot (local mode)
```

## Rollback Signal Flow

```mermaid
sequenceDiagram
    participant User
    participant CLI as fs-updater
    participant FS as Work Dir Files
    participant UB as U-Boot Env

    User->>CLI: --rollback_update
    CLI->>UB: update_reboot_state = 7/8/9
    CLI->>FS: create rollbackUpdate

    User->>CLI: --apply_update
    CLI->>FS: read rollbackUpdate (exists)
    CLI->>UB: update_reboot_state = 10/11/12
    CLI->>CLI: reboot()
```
