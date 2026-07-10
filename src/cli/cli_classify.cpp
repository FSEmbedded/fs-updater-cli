#include "cli_classify.h"

#include "fs_updater_error.h"

namespace cli
{
    int install_terminal_code(bool success, const std::string& type)
    {
        if (type == "fw")
            return static_cast<int>(success
                ? UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL
                : UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR);
        if (type == "app")
            return static_cast<int>(success
                ? UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL
                : UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
        if (type == "fw+app")
            return static_cast<int>(success
                ? UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL
                : UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
        /* Unknown/empty type: cannot classify into a per-type family — return
         * the type-agnostic install-family terminal code rather than guessing
         * firmware (which would misreport an app success as 0 instead of 4). */
        return static_cast<int>(success
            ? UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED
            : UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
    }

    ParseOutcome classify(const RawFlags& flags)
    {
        ParseOutcome out;

        /* The install positional is global, so a bare path is only
         * legal together with --install_update. Otherwise reject, keeping the
         * "unknown input is rejected" contract intact. */
        if (flags.install_path_set && !flags.install_update_set)
        {
            out.kind  = ParseOutcome::Kind::parse_error;
            out.error = "a path argument is only valid with --install_update";
            return out;
        }

        /* An empty positional cannot name a bundle; reject it up front with a
         * clear message instead of letting realpath("") fail downstream. */
        if (flags.install_path_set && flags.install_path.empty())
        {
            out.kind  = ParseOutcome::Kind::parse_error;
            out.error = "the install path must not be empty";
            return out;
        }

        /* --detach only qualifies a local install (a path). The cloud-advance
         * form is already non-blocking, so --detach there is meaningless. */
        if (flags.detach && !flags.install_path_set)
        {
            out.kind  = ParseOutcome::Kind::parse_error;
            out.error = "--detach requires --install_update with a path";
            return out;
        }

        if (flags.install_update_set)
        {
            out.mode = flags.install_path_set ? InstallMode::local
                                              : InstallMode::cloud;
            out.install_path = flags.install_path;
            out.detach       = flags.detach;
        }

        return out; /* kind = ok */
    }

    DispatchVerdict dispatch_verdict(std::size_t action_count)
    {
        if (action_count == 0)
            return DispatchVerdict::version_only;
        if (action_count == 1)
            return DispatchVerdict::run;
        return DispatchVerdict::combo_error;
    }
}
