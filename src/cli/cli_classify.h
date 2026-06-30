#pragma once

#include <string>

/* HW-free classifier for the install surface.
 *
 * classify() turns the raw, already-parsed flag state (RawFlags, produced by
 * the TCLAP front-end in parse_input) into a ParseOutcome that the dispatcher
 * acts on. It contains NO TCLAP and NO FSUpdate/HW dependency so the install
 * surface decisions are natively unit-testable. */
namespace cli
{
    enum class ActionId
    {
        none,
        install_update, /* unified install: local (with path) or cloud-advance */
        commit_update,  /* representative second action for mutual-exclusion */
    };

    enum class InstallMode
    {
        none,
        local, /* --install_update <path> → InstallLocal, blocking */
        cloud, /* --install_update        → StartInstall (ADU-staged) */
    };

    /* Raw flag state extracted from TCLAP after cmd.parse(). One bool per
     * action flag plus the optional install positional and the modifiers. */
    struct RawFlags
    {
        bool install_update_set = false; /* --install_update present */
        bool install_path_set   = false; /* optional positional path present */
        std::string install_path;

        bool commit_update_set = false; /* --commit_update */

        bool detach = false; /* --detach: async install opt-in */
        bool serial = false; /* --serial: serial log sink modifier */
        bool debug  = false; /* --debug:  debug log level modifier */
    };

    struct ParseOutcome
    {
        enum class Kind
        {
            action,            /* a single valid action was selected */
            no_action,         /* nothing to do (print version + hint) */
            parse_error,       /* bare-path / malformed combination */
            incompatible_combo /* more than one action flag set */
        };

        Kind kind = Kind::no_action;
        ActionId action = ActionId::none;
        InstallMode mode = InstallMode::none;
        std::string install_path;

        bool detach = false;
        bool serial = false;
        bool debug  = false;

        std::string error;
    };

    [[nodiscard]] ParseOutcome classify(const RawFlags& flags);

    /* Map a terminal install verdict to the CLI return code by update type:
     * fw→0/3, app→4/7, fw+app→8/11 (success/failure). An unknown or empty type
     * cannot be classified into a per-type family, so it yields the
     * type-agnostic install-family terminal code (48 finished / 49 failed) —
     * never a misclassified firmware code. */
    [[nodiscard]] int install_terminal_code(bool success, const std::string& type);
}
