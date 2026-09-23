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
       ├─> fs-updater-service   (D-Bus: Download / Install / Apply)
       └─> ADUC_LaunchChildProcess(adu-shell)
            └─> fusupdate_tasks  (DoFUSUpdateTask)
                 └─> ADUC_LaunchChildProcess(fs-updater)
                      └─> fs-updater CLI (Cancel / IsInstalled state queries)
```

The handler is built with `BUILD_DBUS_SUPPORT` (the default; a legacy
`!BUILD_DBUS_SUPPORT` build exists in the source for an older, file-signal-based
`fs-updater` this framework no longer ships — do not pair it with a current
CLI, which is D-Bus-only). With `BUILD_DBUS_SUPPORT`, the handler talks to
`fs-updater-service` two different ways depending on the ADU step:

1. **Direct D-Bus calls** — `Download()`, `Install()`, and `Apply()` call the
   `de.fsembedded.fsupdate1` service directly (`fus_service_client`), without
   going through the CLI or adu-shell at all.
2. **Child process launch** — `Cancel()` and `IsInstalled()` still shell out
   through adu-shell to the `fs-updater` CLI, reading its exit code as
   `result.ExtendedResultCode`. These two steps only ever *query or unwind*
   durable U-Boot state; they were not moved to the direct D-Bus path.

See [D-Bus Session Protocol](dbus-session-protocol.md) and
`dbus/de.fsembedded.fsupdate1.xml` in `fs-updater-service` for the full
interface contract behind the direct calls.

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

These map to the D-Bus interface's own `"fw"`/`"app"`/`"fw+app"` type strings
(`update_type_to_dbus_string()` in the handler) for `StartDownload`,
`StartInstall`, and `InstallLocal`.

---

## ADU lifecycle → integration mapping

| ADU Step | Handler Method | How it reaches the service |
|----------|---------------|------------------------------|
| Download | `Download()` | Direct D-Bus: `StartDownload`, then `FinishDownload` after the transfer |
| Install | `Install()` | Direct D-Bus: `StartInstall`, poll `InstallState` property |
| Apply | `Apply()` | Direct D-Bus: `Apply()`; issues the reboot itself via `workflow_request_immediate_reboot()` when it reports one is required |
| Cancel | `Cancel()` | `adu-shell execute` → `fs-updater --update_reboot_state` / `--commit_update`, via `HandleExecuteAction()` below |
| IsInstalled | `IsInstalled()` | `adu-shell execute` → `fs-updater --firmware_version` / `--application_version`, via `HandleExecuteAction()` |
| Backup | `Backup()` | — (no-op) |
| Restore | `Restore()` | — (unsupported, no-op) |

The Delivery Optimization SDK (`ExtensionManager::Download()`) drives the
actual HTTP transfer between `StartDownload` and `FinishDownload`. The
interface has a `ReportDownloadProgress` method for a caller to push interim
progress, but this handler does not call it — `DownloadProgress` stays
unset for the duration of an ADU-driven download; only `DownloadState`
("in_progress" → "finished"/"failed") changes.

---

## D-Bus session lifecycle (Download / Install / Apply)

The handler is a **direct D-Bus client** for these three steps — no signal
files, no intermediate process.

```
Handler.Download()                       fs-updater-service
  StartDownload(type, version, size) ──▶  opens session, returns session_id
  ExtensionManager::Download()  (HTTP transfer, handler-owned)
  FinishDownload(session_id, success, payload_path) ──▶  DownloadState = finished/failed

Handler.Install()
  StartInstall(session_id, type) ──▶  service drives FSUpdate::update_image() asynchronously
  poll InstallState  ◀──  "in_progress" | "finished" | "failed"

Handler.Apply()
  Apply() ──▶  reboot_required
  workflow_request_immediate_reboot()  (handler's own call, if reboot_required)
```

### Stale and stuck states

**A service restart loses the in-memory session.** The service's own session
counter restarts at 1 when it restarts, and a client that had a session in
flight sees it vanish — the interface's own stability notes describe the
recovery pattern (`GetState()`, matching `SessionId` against an `InstallState`
of `"idle"` reading as "failed"), but this handler does not currently call
`GetState()` anywhere: an ADU-driven install that is in flight when the
service restarts is not resumed by this code today. What *does* survive a
restart is the **durable** U-Boot state below, which is why the exit-code
based recovery in the next paragraph works regardless.

**Leftover install/apply state across a device restart.** If the device
restarts between `Install()` completing and `Apply()` running, the durable
`update_reboot_state` U-Boot variable already holds an `INCOMPLETE_*` value.
On resume the handler must:

1. Call `--update_reboot_state` to read the current state via its **exit code**.
2. If exit code is 23, 24, or 25 (`INCOMPLETE_*`): proceed directly to `Apply()`.
3. If exit code is 20 or 21 (`FAILED_*`) or 22 (`FW_UPDATE_REBOOT_FAILED`): the
   previous install failed; call `Cancel()` to roll back before retrying.

See the [state machine recovery table](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/state-machine.md#stale-and-stuck-states)
for the full per-state recovery calls, and
[Return Codes](../reference/return-codes.md#update-state-query---update_reboot_state)
for the exit-code-to-state mapping.

**This handler's `Install()` poll loop has no overall timeout.** It caps
consecutive *empty/unreadable* `InstallState` reads at `MAX_EMPTY_RETRIES`,
but a service that is up and keeps answering `"in_progress"` is polled
forever — there is no deadline on reaching a terminal state. (The CLI's own
`--install_update`/`--install_progress` path is different: its
`wait_for_install()` has an `idle_timeout_ms` and returns a distinct
"timed out, may still be running" result — see
[D-Bus Session Protocol](dbus-session-protocol.md) — but this handler does
not use that code path.) Integrate a bounded overall deadline in `Install()`
if your deployment requires bounded recovery time.

---

## `HandleExecuteAction()` dispatch

`Cancel` and `IsInstalled` route `fs-updater` calls through a common helper:

```
HandleExecuteAction(targetaction)
  └─> adu-shell --update-type fus/update
                --update-action execute
                --target-options <targetaction>
       └─> fusupdate_tasks::Execute()
            └─> fs-updater <targetaction>
```

The CLI exit code is passed back as `result.ExtendedResultCode`. These two
steps stay on this path rather than moving to a direct D-Bus call because
they act on **durable** U-Boot state (the current reboot-state, firmware/app
version) that the CLI already exposes correctly whether or not a D-Bus
session happens to be tracked — there is nothing session-scoped for them to
gain from calling the service directly.

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
| `0x1000`+ | CLI exit code passthrough (Cancel / IsInstalled path only) |

---

## Source files (fus-device-update-azure)

| File | Purpose |
|------|---------|
| `fsupdate_handler.cpp` | Handler implementation — seven ContentHandler methods |
| `fsupdate_handler.hpp` | Handler class, `update_type_t` enum |
| `fsupdate_result.h` | Extended result code definitions |
| `fus_service_client.h`/`.cpp` | Thin sd-bus client for `de.fsembedded.fsupdate1`, used by `Download()`/`Install()`/`Apply()` |
| `fusupdate_tasks.cpp` | adu-shell task functions (Cancel, IsInstalled path) |
| `fusupdate_tasks.hpp` | Task function declarations |
| `adushell_const.hpp` | Update type and action string constants |
