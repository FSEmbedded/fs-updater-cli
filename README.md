# fs-updater CLI

Command-line interface for the F&S Update Framework. Thin dispatcher over
[`fs-updater-lib`](../fs-updater-lib/); converts `FSUpdate` API calls and
exceptions into TCLAP arguments and POSIX exit codes.

Binary: `fs-updater`, installed to `/usr/sbin/`.

## Requirements

Formal requirements and traceability matrix: [prd.md §11.6](../prd.md#116-fs-updater-cli--fr-cli).
The return-code contract is append-only — existing values must never be renumbered or reused.

## Documentation

| Document | Content |
|----------|---------|
| [CLI Reference](docs/README.md) | Parameter tables (Categories A–F), U-Boot variables, signal files |
| [Return Codes](docs/RETURN_CODES.md) | All 55 exit codes (0–54 + 60–65, 70, 124) |
| [Signal Files](docs/diagrams/signal-files.md) | Work-dir signal protocol for ADU agent communication |
| [Workflows](../docs/cli/workflows/) | Per-argument workflow docs (superproject) |
| [ADU Integration](../docs/cli/ADU_INTEGRATION.md) | Azure Device Update handler + `adu-shell` dispatch (superproject) |
| [RAUC D-Bus Plan](../docs/cli/RAUC_DBUS_PLAN.md) | Planned replacement of subprocess RAUC with `sd-bus` (superproject) |

## Related components

- [fs-updater-lib](../fs-updater-lib/) — core update library (C++ API consumed by this CLI)
- [dynamic-overlay](../dynamic-overlay/) — preinit overlay mounting that reads the
  U-Boot state this CLI writes

## Source layout

| File | Content |
|------|---------|
| `src/main.cpp` | Entry point |
| `src/cli/cli.h`, `cli.cpp` | 21 TCLAP arguments, dispatch logic |
| `src/cli/fs_updater_error.h` | Return-code enums (12 categories, 55 codes) |
| `src/cli/fs_updater_types.h` | `version_t` typedef |
| `src/cli/SynchronizedSerial.cpp` | Serial output sync for `--automatic` mode |

## Build

Cross-compile with the F&S Yocto SDK. See the [superproject build recipe](../CLAUDE.md#build).

## License

Proprietary — F&S Elektronik Systeme GmbH.
