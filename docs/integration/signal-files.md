# Signal Files

Work directory: `/tmp/adu/.work/` (default, controlled by `TEMP_ADU_WORK_DIR`).

Signal files are the IPC mechanism between the ADU agent and the CLI's
Category C arguments (`--is_update_available`, `--download_update`,
`--download_progress`, `--install_update`, `--apply_update`). These arguments
have no direct library interaction — they read and create files in the work
directory.

## File overview

| File | Created by | Read by | Purpose |
|------|------------|---------|---------|
| `update_type` | ADU agent | `--is_update_available` | `"firmware"`, `"application"`, or `"both"` |
| `update_version` | ADU agent | `--is_update_available` | Version string of the pending update |
| `update_size` | ADU agent | `--is_update_available`, `--download_progress` | Expected file size in bytes |
| `update_location` | ADU agent | `--download_progress` | Path to the file being downloaded |
| `downloadUpdate` | `--download_update` | ADU agent | Signal: start the download |
| `installUpdate` | `--install_update` | ADU agent | Signal: run the installation |
| `updateInstalled` | ADU agent / lib | `--install_update`, `--apply_update` | Installation complete |
| `applyUpdate` | `--apply_update` | ADU agent | Signal: trigger apply (network mode) |
| `rollbackUpdate` | `--rollback_update`, `--switch_*_slot` | `--apply_update` | Rollback prepared |

## Network update pipeline

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

## Local update flow

Local `--update_file` installs directly via the library; no signal files are
created except `updateInstalled` on completion.

```mermaid
sequenceDiagram
    participant User
    participant CLI as fs-updater
    participant FS as Work Dir Files

    User->>CLI: --update_file /mnt/usb/update.fs
    Note over CLI: Library installs directly,<br/>no signal files needed

    CLI->>FS: write updateInstalled

    User->>CLI: --apply_update
    Note over FS: No applyUpdate / downloadUpdate files
    CLI->>CLI: Direct reboot (local mode)
```

## Rollback signal flow

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
