# F&S Updater CLI

`fs-updater` is the command-line interface for the F&S Update Framework. It converts
`FSUpdate` API calls and exceptions into TCLAP arguments and POSIX exit codes.

Binary: `fs-updater`, installed to `/usr/sbin/`.

See [README](README.md) for the full argument reference, U-Boot variables, and signal files.
See [Return Codes](RETURN_CODES.md) for all exit codes and their meanings.

## Key Dependencies

**fs-updater-lib** — core update library (`FSUpdate` API, 13-state machine, RAUC and
application update handlers). The CLI is a thin dispatcher over this library.

**TCLAP** — command-line argument parsing. All 21 arguments are defined in `cli.h`.

## License

Proprietary — F&S Elektronik Systeme GmbH
