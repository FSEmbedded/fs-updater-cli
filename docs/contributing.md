# Contributing

## Build

```bash
./scripts/build.sh debug           # cross-compile Debug
./scripts/build.sh release         # cross-compile Release (-Os, LTO, stripped)
./scripts/build.sh sanitize        # cross-compile Debug with ASan + UBSan
./scripts/build.sh clean

# Use a locally built fs-updater-lib instead of the SDK sysroot version
./scripts/build.sh debug --lib ../fs-updater-lib/build
```

`SDK_ROOT` defaults to `/opt/fslc-xwayland/5.15-scarthgap`. Override with:

```bash
SDK_ROOT=/path/to/sdk ./scripts/build.sh debug
```

Cross-compile output lands in `build/`.

## CMake options

| Option | Values | Default | Effect |
|--------|--------|---------|--------|
| `OPTIMIZE_FOR` | `SIZE` / `SPEED` | `SIZE` | Release optimisation flags (`-Os` vs `-O3`) |
| `update_version_type` | `string` / `uint64` | `string` | Version field type in config header |
| `FUS_LIB_DIR` | path | _(empty)_ | Local `fs-updater-lib` install prefix; overrides SDK sysroot |

## Tests

`fs-updater-cli` has no unit test suite. Functional testing requires a target
device or a QEMU image with U-Boot environment support.

## Coding standard

Targeting C++17.

**Exception to the project-wide rules:** `fs-updater-cli` intentionally enables
RTTI and exceptions (`-frtti`, no `-fno-exceptions`). TCLAP (the argument
parsing library) requires both. All exceptions thrown by `fs-updater-lib` are
caught in `main()` and translated to exit codes; they must not escape.

Rules that apply in full:

- No raw `new`/`delete` — smart pointers and RAII only
- `[[nodiscard]]` on all error-returning functions
- `std::unique_ptr` as the default ownership type; `std::shared_ptr` only
  when ownership is genuinely shared
- Exit codes are **append-only** and must never be renumbered or reused —
  see [Return Codes](reference/return-codes.md)
