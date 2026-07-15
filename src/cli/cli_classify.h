#pragma once

#include <cstddef>
#include <string>

/* HW-free classifier for the install surface.
 *
 * classify() turns the raw, already-parsed install flags (RawFlags, from the
 * CliArgs front-end in parse_input) into a ParseOutcome the dispatcher and the
 * install handler act on. It owns exactly the install-surface decisions —
 * the bare-path rule, the --detach guard, and the local/cloud mode selection — with NO
 * parser and NO FSUpdate/HW dependency, so they are natively unit-testable.
 *
 * It deliberately does NOT enforce mutual exclusion across all actions: that
 * stays with the dispatch loop in parse_input, which counts every action flag.
 * Global modifiers (--serial/--debug) are not install-surface decisions and
 * are read directly where they apply (setup_logging). */
namespace cli
{
    enum class InstallMode
    {
        none,  /* not an install invocation */
        local, /* --install_update <path> → InstallLocal, blocking */
        cloud, /* --install_update        → StartInstall (ADU-staged) */
    };

    /* Install flags extracted from CliArgs after parse(). */
    struct RawFlags
    {
        bool install_update_set = false; /* --install_update present */
        bool install_path_set   = false; /* optional positional path present */
        std::string install_path;
        bool detach = false;             /* --detach: async install opt-in */
    };

    struct ParseOutcome
    {
        enum class Kind
        {
            ok,          /* no guard violation; mode/path/detach are usable */
            parse_error, /* bare-path / --detach rule violated → combo-error code */
            bad_path     /* install path unusable (empty) → file-not-found code */
        };

        Kind kind = Kind::ok;
        InstallMode mode = InstallMode::none; /* local/cloud iff --install_update set */
        std::string install_path;             /* the resolved positional, if any */
        bool detach = false;                  /* honoured only for a local install */
        std::string error;
    };

    [[nodiscard]] ParseOutcome classify(const RawFlags& flags);

    /* True for a token that looks like an option rather than a path.
     *
     * Historical note: this guarded the previous parser's install positional,
     * which accepted any token its labelled args declined — without
     * it, an unknown flag was absorbed as the path and surfaced as a bare-path
     * violation instead of an unknown option. The getopt_long parser gets
     * that protection for free from the option/operand distinction
     * getopt_long itself makes, so this function is no longer called from the
     * parsing path; it is kept (untouched, cli_classify.* stays byte-identical
     * across the cut) for its own unit coverage.
     *
     * A path that genuinely starts with '-' must be given as "./-name", the
     * usual convention. */
    [[nodiscard]] bool is_option_like(const std::string& token);

    /* Verdict of the action dispatch: action flags are mutually exclusive,
     * modifiers (--debug/--serial/--detach) never count as actions. */
    enum class DispatchVerdict
    {
        version_only, /* no action given: print version, return code stays 0 */
        run,          /* exactly one action: dispatch its handler */
        combo_error   /* more than one action: INCOMPATIBLE_ARG_COMBO */
    };

    [[nodiscard]] DispatchVerdict dispatch_verdict(std::size_t action_count);

    /* Why a privileged D-Bus install call failed, derived from the error name
     * (authoritative) then errno. Kept here, not in the sd-bus client, so the
     * classification is natively unit-testable without a bus. */
    enum class CallError
    {
        none,       /* the call succeeded */
        busy,       /* de.fsembedded.fsupdate1.Error.Busy / -EBUSY */
        denied,     /* org.freedesktop.DBus.Error.AccessDenied (polkit / bus policy) */
        no_updater, /* de.fsembedded.fsupdate1.Error.NoUpdater / -ENOSYS */
        other       /* anything else (bad args, I/O, bus not reachable, …) */
    };

    /* Classify a failed call by its D-Bus error name first (the name crosses
     * the bus; a handler-return errno does not once a name is set), then by
     * errno as a direct-peer / version-skew fallback. error_name may be null.
     * Pure — no sd-bus dependency. */
    [[nodiscard]] CallError classify_call_error(int r, const char* error_name);

    /* Map an install-start failure to the CLI return code: busy→66, denied→67,
     * a succeeded-but-unreadable call (none)→47 in-progress, everything else
     * (no_updater/other)→49 failed. */
    [[nodiscard]] int install_start_error_code(CallError e);

    /* Map a terminal install verdict to the CLI return code by update type:
     * fw→0/3, app→4/7, fw+app→8/11 (success/failure). An unknown or empty type
     * cannot be classified into a per-type family, so it yields the
     * type-agnostic install-family terminal code (48 finished / 49 failed) —
     * never a misclassified firmware code. */
    [[nodiscard]] int install_terminal_code(bool success, const std::string& type);
}
