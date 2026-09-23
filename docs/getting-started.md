# Getting Started

## Prerequisites

- SDK installed at `/opt/fslc-xwayland/5.15-scarthgap`, or `SDK_ROOT` set to
  its location; `scripts/build.sh` sources its
  `environment-setup-cortexa53-fslc-linux` itself
- `fs-updater-lib` built and installed (either in SDK sysroot or via `--lib`)
- Target device with U-Boot environment configured for A/B updates

## Build and install

```bash
# Cross-compile
./scripts/build.sh release

# Install to device (copy the binary)
scp build/fs-updater root@<device>:/usr/sbin/fs-updater
```

## Install a local update file

`--install_update` installs a `.fs` bundle from a local path via the
D-Bus service, blocking by default until the outcome is known. See
[CLI Reference — `--install_update`](reference/cli.md#--install_update-install_path)
for the `--detach` form and the D-Bus call it makes, and
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/master/docs/reference/bundle-format.md)
for the container layout.

```bash
# Install firmware and application bundle from a local path
fs-updater --install_update /mnt/usb/update.fs
```

Exit code `0` (firmware), `4` (application), or `8` (both) indicates success.
A reboot is required after install before the update takes effect.

## Commit an update after reboot

After rebooting into the new slot, confirm the update is healthy:

```bash
fs-updater --commit_update
```

Exit code `16` = committed. Exit code `17` = nothing to commit. With nothing
pending, `16` means the running slot's boot budget was restored, and after an
interrupted install it can mean the update was discarded; see
[CLI Reference — `--commit_update`](reference/cli.md#--commit_update).

## Check the current update state

```bash
fs-updater --update_reboot_state
```

The exit code names the stored state and the next step to take; see
[CLI Reference — `--update_reboot_state`](reference/cli.md#--update_reboot_state)
for every code. A failed install (20, 21, 22) is settled with
`--commit_update`, not rolled back.

## Rollback to the previous version

```bash
# Roll back the firmware, the application or both, whichever is pending
fs-updater --rollback_update

# Read what the rollback left behind
fs-updater --update_reboot_state
```

Exit 27 means the device is already back on the proven slot. Exit 23 means a
bootloader fallback had already undone the firmware and the rollback changed
nothing: commit instead, see
[CLI Reference — `--rollback_update`](reference/cli.md#--rollback_update).
Exit 28–30 means a reboot is still owed:

```bash
fs-updater --apply_update     # reboots
# after the reboot:
fs-updater --commit_update
```

A combined firmware and application update rolled back before its reboot is
the exception; see [CLI Reference — `--rollback_update`](reference/cli.md#--rollback_update).

## Query installed versions

```bash
fs-updater --firmware_version      # prints to stdout
fs-updater --application_version   # prints to stdout
fs-updater --version               # prints CLI version + build date
```

Each exits 0 on success; see
[Return Codes — Query success](reference/return-codes.md#query-success).

## Enable debug logging

Add `--debug` to any command to log debug output to stdout (or, with
`--serial`, to the serial console):

```bash
fs-updater --debug --install_update /mnt/usb/firmware.fs
```

`--debug` and `--serial` combine with any action; `--detach` combines with
`--install_update` only. All other action flags are mutually exclusive.

## Next steps

- [CLI Reference](reference/cli.md) — full argument descriptions and return-code ranges
- [Return Codes](reference/return-codes.md) — scripting guide
- [D-Bus Session Protocol](integration/dbus-session-protocol.md) — the `de.fsembedded.fsupdate1` call sequence behind the network-update, local-install, and rollback flags
