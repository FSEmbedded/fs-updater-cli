# Contributing

## Build

```bash
./scripts/build.sh debug           # cross-compile Debug (default)
./scripts/build.sh release         # cross-compile Release (-Os, LTO, stripped)
./scripts/build.sh sanitize        # cross-compile Debug with ASan + UBSan
./scripts/build.sh test            # native build + unit tests
./scripts/build.sh fuzz            # native libFuzzer build (clang) + short smoke run
./scripts/build.sh clean

# Use a locally built fs-updater-lib instead of the SDK sysroot version
./scripts/build.sh debug --lib ../fs-updater-lib/build
```

`SDK_ROOT` defaults to `/opt/fslc-xwayland/5.15-scarthgap`. Override with:

```bash
SDK_ROOT=/path/to/sdk ./scripts/build.sh debug
```

Cross-compile output lands in `build/` (`build_san/` for `sanitize`); the
native targets use `build_test/`, `build_test_san/` and `build_fuzz/`.

| `build.sh` option | Effect |
|-------------------|--------|
| `--speed` | Release optimisation for speed (`-O2`) instead of size (`-Os`) |
| `--uint64` | `uint64` version type instead of `string` |
| `--lib <build_dir>` | Link a locally built `fs-updater-lib`; use its `build_san/` for the `sanitize` target |
| `--sanitize` | With `test`: run the native suite under ASan/UBSan |
| `--env-config <path>` | Compile another `fw_env.config` path into the binary, for test harnesses |

## CMake options

| Option | Values | Default | Effect |
|--------|--------|---------|--------|
| `OPTIMIZE_FOR` | `SIZE` / `SPEED` | `SIZE` | Release optimisation flags (`-Os` vs `-O2`) |
| `update_version_type` | `string` / `uint64` | `string` | Version field type in config header |
| `FUS_LIB_DIR` | path | _(empty)_ | Local `fs-updater-lib` install prefix; overrides SDK sysroot |
| `FUS_SOURCE_ID` | string | _(empty: `git describe`, else `unknown`)_ | Source revision reported by `--version` |
| `UBOOT_CONFIG_PATH` | path | _(empty: the library's default)_ | `fw_env.config` path compiled into the binary |
| `BUILD_DBUS_SUPPORT` | `ON` / `OFF` | `ON` | Must stay `ON` for the executable; only the fuzz build sets it `OFF` |
| `BUILD_TESTING` | `ON` / `OFF` | `OFF` | Build the unit tests |
| `BUILD_MAIN_TARGET` | `ON` / `OFF` | `ON` | Build the executable (`OFF`: tests or fuzz targets only) |
| `BUILD_FUZZING` | `ON` / `OFF` | `OFF` | Build the libFuzzer targets (clang) |
| `ENABLE_SANITIZERS` | `ON` / `OFF` | `OFF` | ASan + UBSan |
| `FSUP_WERROR` | `ON` / `OFF` | `ON` | Treat compiler warnings as errors |

## Tests

`./scripts/build.sh test` builds and runs the native unit tests under `tests/`
(argument parsing and exit-code classification among them). They need
no device. Functional testing of the commands requires a target device or a
QEMU image with U-Boot environment support.

## Coding standard

Targeting C++17.

**Exceptions and RTTI are enabled.** `fs-updater-lib` reports errors by
throwing; every exception is caught in `main()` and translated to an exit
code, and none may escape.

Rules that apply in full:

- No raw `new`/`delete` — smart pointers and RAII only
- `[[nodiscard]]` on all error-returning functions
- `std::unique_ptr` as the default ownership type; `std::shared_ptr` only
  when ownership is genuinely shared
- Exit codes are **append-only** and must never be renumbered or reused —
  see [Return Codes](reference/return-codes.md)
