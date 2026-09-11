#include "cli_classify.h"

#include "fs_updater_error.h"

#include <cerrno>
#include <cstring>

namespace cli
{
namespace
{
    /* Must match the service's D-Bus error names (service.cpp). Duplicated
     * across the repo boundary by necessity; the wire contract is pinned by
     * both this unit's tests and the service's contract tests. */
    constexpr const char* ERROR_BUSY          = "de.fsembedded.fsupdate1.Error.Busy";
    constexpr const char* ERROR_NO_UPDATER    = "de.fsembedded.fsupdate1.Error.NoUpdater";
    constexpr const char* ERROR_ACCESS_DENIED = "org.freedesktop.DBus.Error.AccessDenied";
}

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
            out.kind  = ParseOutcome::Kind::bad_path;
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

    CallError classify_call_error(int r, const char* error_name)
    {
        if (r >= 0)
            return CallError::none;

        /* Name first: the busy/denied/no-updater names are the only reliable
         * discriminators over the bus. NEVER map -EACCES back to denied — every
         * FAILED error maps to -EACCES on the wire, so that would misreport an
         * unrelated failure as a permission denial. */
        if (error_name != nullptr)
        {
            if (std::strcmp(error_name, ERROR_BUSY) == 0)          return CallError::busy;
            if (std::strcmp(error_name, ERROR_ACCESS_DENIED) == 0) return CallError::denied;
            if (std::strcmp(error_name, ERROR_NO_UPDATER) == 0)    return CallError::no_updater;
        }

        /* errno fallback for a direct peer or version skew where no known name
         * is set (an unregistered custom name maps to -EIO on the wire, so only
         * the raw -EBUSY/-ENOSYS of a same-host peer reach here). */
        if (r == -EBUSY)  return CallError::busy;
        if (r == -ENOSYS) return CallError::no_updater;
        return CallError::other;
    }

    int install_start_error_code(CallError e)
    {
        switch (e)
        {
        case CallError::busy:
            return static_cast<int>(UPDATER_CLI_VALIDATION::INSTALL_BUSY);
        case CallError::denied:
            return static_cast<int>(UPDATER_CLI_VALIDATION::PERMISSION_DENIED);
        case CallError::none:
            /* Call succeeded but the session id was unreadable — the worker is
             * already running, so report in-progress, not a failure. */
            return static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
        case CallError::no_updater:
        case CallError::other:
            break;
        }
        return static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
    }

    int map_slot_switch_errno(int errno_value)
    {
        /* ENOENT: the lib refused the switch because the target slot was never
         * provisioned (no image file installed) — distinct from a bad slot
         * (56, not 54) and from a generic progress failure (13). */
        if (errno_value == ENOENT)
        {
            return static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_UNPROVISIONED);
        }
        /* EPERM/ECANCELED: the lib refused the switch, either because the
         * target slot is marked bad or because a firmware rollback was asked
         * for a slot whose install never reached the boot order (never
         * actionable) — both are state errors the caller must see as such
         * (54), not a generic progress failure (13). */
        if (errno_value == EPERM || errno_value == ECANCELED)
        {
            return static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
        }
        return static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
    }
}
