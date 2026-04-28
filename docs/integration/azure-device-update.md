# Azure Device Update Integration

The F&S Update Framework integrates with Azure Device Update (ADU) through two
components in the `fus-device-update-azure` repository:

| Component | Purpose |
|-----------|---------|
| `fsupdate_handler` (shared library) | ADU step handler plugin (`fus/update:1`) |
| `fusupdate_tasks` (adu-shell module) | Task dispatcher called by adu-shell |

---

## Call chain

```
ADU Agent
  └─> fsupdate_handler  (ContentHandler plugin, fus/update:1)
       └─> ADUC_LaunchChildProcess(adu-shell)
            └─> fusupdate_tasks  (DoFUSUpdateTask)
                 └─> ADUC_LaunchChildProcess(fs-updater)
                      └─> fs-updater CLI
```

The handler communicates with `fs-updater` through two mechanisms:

1. **Child process launch** — adu-shell → fs-updater; the CLI exit code is
   returned as `result.ExtendedResultCode`.
2. **Signal files** — the handler creates metadata files in
   `/tmp/adu/.work/`; the CLI reads and creates signal files in that same
   directory. See [Signal Files](signal-files.md) for the full protocol.

---

## Update type strings

The update type is read from the ADU manifest field
`handlerProperties.updateType`:

| String | Meaning |
|--------|---------|
| `"firmware"` | Firmware-only update (old `.raucb` format) |
| `"application"` | Application-only update (old component format) |
| `"common-firmware"` | Firmware-only update from `.fs` bundle |
| `"common-application"` | Application-only update from `.fs` bundle |
| `"common-both"` | Firmware + application update from `.fs` bundle |

---

## ADU lifecycle → CLI mapping

| ADU Step | Handler Method | adu-shell Action | fs-updater CLI commands | Wait signal |
|----------|---------------|------------------|------------------------|-------------|
| Download | `Download()` | — (direct) | — | `downloadUpdate` |
| Install | `Install()` | `install` | `--update_file [--update_type]` | `installUpdate` |
| Apply | `Apply()` | `execute` | `--update_reboot_state` | `applyUpdate` |
| Cancel | `Cancel()` | `execute`, `cancel` | `--update_reboot_state`, `--rollback_update` | — |
| IsInstalled | `IsInstalled()` | `execute` | `--firmware_version` / `--application_version`, `--update_reboot_state`, `--commit_update` | — |
| Backup | `Backup()` | — | — (no-op) | — |
| Restore | `Restore()` | — | — (unsupported, no-op) | — |

---

## Signal file lifecycle

The handler **creates** the work-directory files; the CLI **reads and creates**
signal files.

```
Handler (Download)                CLI (polling)
  create_work_dir()
    └─ rm + mkdir /tmp/adu/.work/
  write update_version            --is_update_available
  write update_type                 reads update_type, update_version, update_size
  write update_size
                                  --download_update
  WAIT for downloadUpdate ◄─────    creates downloadUpdate signal
  write update_location           --download_progress
                                    reads update_location, compares file sizes
  ExtensionManager::Download()

Handler (Install)                 --install_update
  WAIT for installUpdate ◄──────    checks update_location, creates installUpdate
  launch fs-updater --update_file

Handler (Apply)
  check --update_reboot_state
  WAIT for applyUpdate ◄────────  --apply_update creates applyUpdate signal
  request reboot or commit
```

### Stale and stuck states

**New download wipes in-progress install state.**
`create_work_dir()` deletes and recreates the entire `/tmp/adu/.work/`
directory at the start of every `Download()` call. If `Download()` is called
while `Install()` is still waiting for `installUpdate`, the signal file and
`update_location` are destroyed — `Install()` blocks forever waiting for a
file that no longer exists. Do not call `Download()` while `Install()` is
in progress.

**Leftover signals from a previous session.**
On device restart (watchdog reset, reboot, process restart), `/tmp/adu/.work/`
may contain signal files and metadata from the previous run. The next
`Download()` call will call `create_work_dir()` and wipe them. However, if the
restart occurs between `Install()` and `Apply()` — after `fs-updater --update_file` has completed but before `applyUpdate` is created — the
`update_reboot_state` U-Boot variable is already set to an `INCOMPLETE_*` value.
On resume the handler must:

1. Call `--update_reboot_state` to read the current state via its **exit code**.
2. If exit code is 23, 24, or 25 (`INCOMPLETE_*`): proceed directly to `Apply()`.
3. If exit code is 20 or 21 (`FAILED_*`) or 22 (`FW_UPDATE_REBOOT_FAILED`): the
   previous install failed; call `Cancel()` to roll back before retrying.

See the [state machine recovery table](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/state-machine.md#stale-and-stuck-states)
for the full per-state recovery calls, and
[Return Codes](../reference/return-codes.md#update-state-query---update_reboot_state)
for the exit-code-to-state mapping.

**Infinite wait.**
The handler's `WAIT for <signal>` loops have no built-in timeout. If the CLI
process dies or the signal is never created, the handler blocks indefinitely.
Integrate a timeout and abort path in `Download()`, `Install()`, and `Apply()`
if your deployment requires bounded recovery time.

---

## `HandleExecuteAction()` dispatch

Apply, Cancel, and IsInstalled all route `fs-updater` calls through a common
helper:

```
HandleExecuteAction(targetaction)
  └─> adu-shell --update-type fus/update
                --update-action execute
                --target-options <targetaction>
       └─> fusupdate_tasks::Execute()
            └─> fs-updater <targetaction>
```

The CLI exit code is passed back as `result.ExtendedResultCode`.

---

## Extended result code ranges

See `fsupdate_result.h` in `fus-device-update-azure` for all codes:

| Range | Category |
|-------|----------|
| `0x000`–`0x0FF` | General / prepare errors |
| `0x100`–`0x1FF` | Download errors |
| `0x200`–`0x2FF` | Install errors |
| `0x300`–`0x3FF` | Apply errors |
| `0x400`–`0x4FF` | Cancel errors |
| `0x500`–`0x5FF` | IsInstalled errors |
| `0x1000`+ | CLI exit code passthrough |

---

## Source files (fus-device-update-azure)

| File | Purpose |
|------|---------|
| `fsupdate_handler.cpp` | Handler implementation — seven ContentHandler methods |
| `fsupdate_handler.hpp` | Handler class, `update_type_t` enum |
| `fsupdate_result.h` | Extended result code definitions |
| `fusupdate_tasks.cpp` | adu-shell task functions (Install, CommitUpdate, Cancel, Execute) |
| `fusupdate_tasks.hpp` | Task function declarations |
| `adushell_const.hpp` | Update type and action string constants |
