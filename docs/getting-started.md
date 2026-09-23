# Getting Started

## Prerequisites

- SDK sourced: `/opt/fslc-xwayland/5.15-scarthgap/environment-setup-cortexa53-fslc-linux`
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
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/reference/bundle-format.md)
for the container layout.

```bash
# Install firmware and application bundle from a local path
fs-updater --install_update /mnt/usb/update.fs

# A bare trailing path works the same way
fs-updater /mnt/usb/update.fs
```

Exit code `0` (firmware), `4` (application), or `8` (both) indicates success.
A reboot is required after install before the update takes effect.

## Commit an update after reboot

After rebooting into the new slot, confirm the update is healthy:

```bash
fs-updater --commit_update
```

Exit code `16` = committed. Exit code `17` = nothing to commit (already idle).

## Check the current update state

```bash
fs-updater --update_reboot_state
```

Returns a code in the range 20–33 that maps to the 13-state machine. See
[CLI Reference](reference/cli.md#--update_reboot_state) for the full mapping.

## Rollback to the previous version

```bash
# Rollback both firmware and application (whichever is pending)
fs-updater --rollback_update

# Reboot is required to complete the rollback
fs-updater --apply_update
```

After reboot, commit to finalise:

```bash
fs-updater --commit_update
```

## Query installed versions

```bash
fs-updater --firmware_version      # prints to stdout, exits 0
fs-updater --application_version   # prints to stdout, exits 0
fs-updater --version               # prints CLI version + build date, exits 0
```

## Enable debug logging

Add `--debug` to any command to log verbose output to stderr:

```bash
fs-updater --debug --install_update /mnt/usb/firmware.fs
```

`--debug` and `--serial` combine with any action; `--detach` combines with
`--install_update` only. All other action flags are mutually exclusive.

## Next steps

- [CLI Reference](reference/cli.md) — full argument descriptions and return-code ranges
- [Return Codes](reference/return-codes.md) — scripting guide
- [D-Bus Session Protocol](integration/dbus-session-protocol.md) — the `de.fsembedded.fsupdate1` call sequence behind the network-update, local-install, and rollback flags
