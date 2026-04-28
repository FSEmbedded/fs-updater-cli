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

Two procedures are supported. The new procedure (`.fs` bundle) is preferred
for new integrations; the old procedure (component files + `--update_type`)
remains fully supported for existing pipelines. See
[CLI Reference — `--update_file`](reference/cli.md#--update_file-path) for
details, and
[Bundle Format](https://github.com/fsembedded/fs-updater-lib/blob/main/docs/reference/bundle-format.md)
for both file layouts.

### New update procedure
```bash
# Install firmware and application bundle from a local path
fs-updater --update_file /mnt/usb/update.fs

# Install firmware bundle from a local path
fs-updater --update_file /mnt/usb/firmware.fs

# Install application only
fs-updater --update_file /mnt/usb/app.fs
```

### Old update procedure
```bash
# Install firmware only
fs-updater --update_file /mnt/usb/firmware.raucb --update_type fw

# Install application only
fs-updater --update_file /mnt/usb/application_signed --update_type app
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
[CLI Reference](reference/cli.md#update_reboot_state) for the full mapping.

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
fs-updater --debug --update_file /mnt/usb/firmware.fs
```

`--debug` is the only flag combinable with another action flag.

## Next steps

- [CLI Reference](reference/cli.md) — full argument descriptions and return-code ranges
- [Return Codes](reference/return-codes.md) — scripting guide
- [Signal Files](integration/signal-files.md) — ADU agent IPC protocol
