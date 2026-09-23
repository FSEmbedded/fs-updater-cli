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
                      └─> fs-updater CLI (Cancel / IsInstalled)
```

The handler must be built with its `BUILD_DBUS_SUPPORT` option enabled. The
source also has a build without it, which drives a file-signal-based
`fs-updater` that this framework does not ship; it does not work with this
CLI, which is D-Bus-only. With `BUILD_DBUS_SUPPORT`, the handler talks to
`fs-updater-service` two different ways depending on the ADU step:

1. **Direct D-Bus calls** — `Download()`, `Install()`, and `Apply()` call the
   `de.fsembedded.fsupdate1` service directly (`fus_service_client`), without
   going through the CLI or adu-shell at all.
2. **Child process launch** — `Cancel()` and `IsInstalled()` shell out
   through adu-shell to the `fs-updater` CLI, reading its exit code as
   `result.ExtendedResultCode`. These two steps read durable state and, in
   the states the [mapping below](#adu-lifecycle--integration-mapping) names,
   prepare a rollback or run a commit.

See [D-Bus Session Protocol](dbus-session-protocol.md) and
`dbus/de.fsembedded.fsupdate1.xml` in `fs-updater-service` for the full
interface contract behind the direct calls.

---

## Update type strings

The update type is read from the ADU manifest field
`handlerProperties.updateType`:

| String | D-Bus type string |
|--------|-------------------|
| `"firmware"` | `"fw"` |
| `"application"` | `"app"` |
| `"common-firmware"` | `"fw"` |
| `"common-application"` | `"app"` |
| `"common-both"` | `"fw+app"` |

The handler maps each string (`update_type_to_dbus_string()`) to the D-Bus
interface's type string and passes it to `StartDownload` and `StartInstall`;
any other string fails the step. The service records the type, requires
`StartInstall` to repeat the one `StartDownload` announced, and publishes it
as `UpdateType`, but does not hand it to the library: the library detects the
payload's format from the file itself and installs whatever the file
contains. See the library's
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/bundle-format.md#accepted-inputs).
In `IsInstalled()` the type selects which version the handler compares:
firmware, application, or both for `"common-both"`.

---

## ADU lifecycle → integration mapping

| ADU Step | Handler Method | How it reaches the service |
|----------|---------------|------------------------------|
| Download | `Download()` | Direct D-Bus: `StartDownload`, then `FinishDownload` after the transfer |
| Install | `Install()` | Direct D-Bus: `StartInstall`, poll `InstallState` property |
| Apply | `Apply()` | Direct D-Bus: `Apply()`; issues the reboot itself via `workflow_request_immediate_reboot()` when it reports one is required |
| Cancel | `Cancel()` | adu-shell → `fs-updater --update_reboot_state`; on exit 24 `--rollback_update` (adu-shell's cancel action), then another state read that accepts only 28 or 27 — an application rollback reports neither, so the step fails after the rollback is stored; on exit 28 `--commit_update`, whose result is compared against 27, which a commit never returns, so the step reports `ADUC_Result_Cancel_Success` with the not-allowed-state extended code (28 is reported both before and after a slot switch's reboot, so the commit returns 18 before that reboot and 16 after it; either result leads to that extended code, see [Rebooting after a firmware rollback or slot switch](../reference/cli.md#rebooting-after-a-firmware-rollback-or-slot-switch)); every other state fails the step |
| IsInstalled | `IsInstalled()` | adu-shell → `fs-updater --firmware_version` / `--application_version` and `--update_reboot_state`; on exit 20 or 21 also `--commit_update`, but only after the version comparison did not match — with a matching version, 20 or 21 fails the step as an unknown state and nothing is committed |
| Backup | `Backup()` | — (no-op) |
| Restore | `Restore()` | — (unsupported, no-op) |

The Delivery Optimization SDK (`ExtensionManager::Download()`) drives the
actual HTTP transfer between `StartDownload` and `FinishDownload`. The
interface has a `ReportDownloadProgress` method for a caller to push interim
progress, but this handler does not call it: during an ADU-driven download
`DownloadProgress` stays at the 0 that `StartDownload` sets, and a successful
`FinishDownload` sets it to 100. No interim value is published.

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
of `"idle"` reading as "failed"), but this handler does not call
`GetState()` anywhere: an ADU-driven install that is in flight when the
service restarts is not resumed by it. What *does* survive a
restart is the **durable** U-Boot state below, which is why the exit-code
based recovery in the next paragraph works regardless.

**Leftover install/apply state across a device restart.** If the device
restarts between `Install()` completing and `Apply()` running, the durable
`update_reboot_state` U-Boot variable already holds an `INCOMPLETE_*` value.
On resume the handler must:

1. Call `--update_reboot_state` to read the current state via its **exit code**.
2. Take the next step the
   [CLI Reference](../reference/cli.md#--update_reboot_state) lists for that
   code. 26 means the reboot is still owed: `Apply()`. 23–25 mean the restart
   already booted the update, so the step is `--commit_update`, not another
   `Apply()`.
3. A failed install (20, 21, or 22) is settled by `--commit_update`, never by
   `Cancel()`, which fails the step in these states; the CLI Reference gives
   the reason.
   `IsInstalled()` issues that commit itself when it reads 20 or 21 after a
   version mismatch (see the mapping above), but not for 22.

The library's
[Stale and stuck states](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/state-machine.md#stale-and-stuck-states)
table covers the recovery per stored state.

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

`Cancel` and `IsInstalled` route their `fs-updater` calls through a common
helper. The one exception is the rollback in `Cancel()`, which runs adu-shell's
`cancel` action (`fs-updater --rollback_update`) instead:

```
HandleExecuteAction(targetaction)
  └─> adu-shell --update-type fus/update
                --update-action execute
                --target-options <targetaction>
       └─> fusupdate_tasks::Execute()
            └─> fs-updater <targetaction>
```

The CLI exit code is passed back as `result.ExtendedResultCode`. These two
steps use this path rather than a direct D-Bus call because
they act on state that outlives a session — the reboot state in the U-Boot
environment and the installed versions, which the library reads from files
(see its [Versions](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/api.md#versions)
entry) — that the CLI already exposes correctly whether or not a D-Bus
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
| `0x1000`+ | Reserved for child-process exit codes (`0x1000` + exit code); defined, but not used by this handler |

On the Cancel / IsInstalled path the CLI exit code itself, without an offset,
is the `ExtendedResultCode` the handler evaluates.

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
