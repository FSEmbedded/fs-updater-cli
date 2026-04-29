# fs-updater CLI

Command-line interface for the F&S Update Framework. Converts
[`fs-updater-lib`](https://github.com/fsembedded/fs-updater-lib/blob/master/README.md)
API calls and exceptions into TCLAP arguments and POSIX exit codes.

Binary: `fs-updater`, installed to `/usr/sbin/`.

## Build

```bash
./scripts/build.sh debug           # cross-compile Debug (default)
./scripts/build.sh release         # cross-compile Release (-Os, LTO, stripped)
./scripts/build.sh sanitize        # cross-compile Debug with ASan + UBSan
./scripts/build.sh clean

# Link against a locally built fs-updater-lib instead of the SDK version
./scripts/build.sh debug --lib ../fs-updater-lib/build
```

SDK defaults to `/opt/fslc-xwayland/5.15-scarthgap`. Override with
`SDK_ROOT=/path/to/sdk ./scripts/build.sh debug`.

The `--lib <build_dir>` flag installs `fs-updater-lib` into a local staging
prefix (`build/fus_lib_install`) and overrides the SDK sysroot version.

## Documentation

| Document | Content |
|----------|---------|
| [Getting Started](docs/getting-started.md) | First-use walkthrough |
| [CLI Reference](docs/reference/cli.md) | All 21 arguments, grouped by function |
| [Return Codes](docs/reference/return-codes.md) | All exit codes (0–124) |
| [Signal Files](docs/integration/signal-files.md) | Work-dir IPC protocol for ADU agent |
| [Azure Device Update Integration](docs/integration/azure-device-update.md) | ADU handler + adu-shell call chain |
| [Contributing](docs/contributing.md) | Build, coding standard |

## Related components

| Component | Purpose |
|-----------|---------|
| [fs-updater-lib](https://github.com/fsembedded/fs-updater-lib/blob/master/README.md) | Core update library consumed by this CLI |
| [dynamic-overlay](https://github.com/fsembedded/dynamic-overlay/blob/master/README.md) | Preinit overlay mounting; reads the U-Boot state this CLI writes |

## Return-code stability contract

The return-code contract is **append-only**. Existing values must never be
renumbered or reused. Scripts and tools that check exit codes depend on
stable numbers. See [Return Codes](docs/reference/return-codes.md) for the
extension rules.

## License

MIT License
