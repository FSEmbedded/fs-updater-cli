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
        out.detach = flags.detach;
        out.serial = flags.serial;
        out.debug  = flags.debug;

        /* The install positional is global, so a bare path is only
         * legal together with --install_update. Otherwise reject, keeping the
         * "unknown input is rejected" contract intact. */
        if (flags.install_path_set && !flags.install_update_set)
        {
            out.kind  = ParseOutcome::Kind::parse_error;
            out.error = "a path argument is only valid with --install_update";
            return out;
        }

        /* --detach only qualifies an --install_update; alone it is meaningless. */
        if (flags.detach && !flags.install_update_set)
        {
            out.kind  = ParseOutcome::Kind::parse_error;
            out.error = "--detach requires --install_update";
            return out;
        }

        int action_count = 0;
        if (flags.install_update_set) ++action_count;
        if (flags.commit_update_set)  ++action_count;

        if (action_count > 1)
        {
            out.kind  = ParseOutcome::Kind::incompatible_combo;
            out.error = "only one action flag may be given";
            return out;
        }

        if (action_count == 0)
        {
            out.kind = ParseOutcome::Kind::no_action;
            return out;
        }

        out.kind = ParseOutcome::Kind::action;

        if (flags.install_update_set)
        {
            out.action = ActionId::install_update;
            out.mode = flags.install_path_set ? InstallMode::local
                                              : InstallMode::cloud;
            out.install_path = flags.install_path;
        }
        else /* flags.commit_update_set */
        {
            out.action = ActionId::commit_update;
        }

        return out;
    }
}
