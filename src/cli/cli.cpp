#include "cli.h"
#include <fs_update_framework/library_source_id.h>
#include "fs_updater_error.h"
#include "fs_updater_types.h"
#include "cli_io.h"
#include "fus_dbus_client.h"
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>

constexpr char FSCLI_DOMAIN[] = "cli";

namespace {

/* Idle watchdog for the blocking local install (ms with no progress nor
 * completion before giving up). Overridable for slow media / CI. */
int install_wait_idle_ms()
{
    const char* env = std::getenv("FSUP_INSTALL_WAIT_MS");
    if (env != nullptr)
    {
        const long v = std::strtol(env, nullptr, 10);
        if (v > 0 && v <= 3600000) return static_cast<int>(v);
    }
    return 120000; /* 2 min */
}

/* Live progress line during a blocking install; called only on change. */
void render_install_progress(int pct)
{
    cli_io::write_stdout("\rInstalling: " + std::to_string(pct) + "%");
}

} // namespace

constexpr uint32_t firmware_update_state = 0;
constexpr uint32_t application_update_state = 1;

using std::string;

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		return_code(0)
{
    this->parse_input(argc, argv);
}

cli::fs_update_cli::~fs_update_cli()
{
}

// ---------------------------------------------------------------------------
// Logging setup
// ---------------------------------------------------------------------------

void cli::fs_update_cli::setup_logging()
{
    const bool use_serial = this->args.serial();
    const auto level = this->args.debug()
        ? logger::logLevel::DEBUG
        : logger::logLevel::WARNING;

    if (use_serial)
    {
        this->serial_cout = std::make_shared<SynchronizedSerial>();
        this->logger_sink = std::make_unique<logger::LoggerSinkSerial>(level, serial_cout);
    }
    else
    {
        this->logger_sink = std::make_shared<logger::LoggerSinkStdout>(level);
    }

    this->logger_handler = logger::LoggerHandler::initLogger(this->logger_sink);
    this->update_handler = std::make_unique<fs::FSUpdate>(logger_handler);
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Update execution
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Commit
// ---------------------------------------------------------------------------

void cli::fs_update_cli::commit_update()
{
    try
    {
        /* Asked before the commit: afterwards the state is settled and this
         * window is no longer distinguishable from an ordinary confirmation. */
        const bool was_stalled = this->update_handler->has_stalled_install();
        /* Same reason, for the same window: a value no current flow writes is
         * gone once the commit has consumed it, and a caller told only that the
         * commit succeeded cannot tell a migrated device from a confirmed one. */
        const bool was_legacy_state = this->update_handler->get_update_reboot_state()
                                      == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED;

        if (this->update_handler->commit_update() == true)
        {
            if (was_stalled)
            {
                cli_io::write_stdout("Settled an install that was never activated; still running the previous firmware\n");
                this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::STALLED_INSTALL_SETTLED);
                return;
            }
            if (was_legacy_state)
            {
                cli_io::write_stdout("Migrated a durable state no current flow writes; the device is settled\n");
                this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::LEGACY_STATE_MIGRATED);
                return;
            }
            cli_io::write_stdout("Commit update\n");
            this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL);
        }
        else
        {
            cli_io::write_stdout("Commit update not needed\n");
            this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED);
        }
    }
    catch (const fs::NotAllowedUpdateState &e)
    {
        cli_io::write_stderr("Not allowed update state in UBoot\n");
        this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE);
    }
    catch (const std::exception &e)
    {
        cli_io::write_stderr(string("FS updater cli error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR);
    }
}

// ---------------------------------------------------------------------------
// Rollback
// ---------------------------------------------------------------------------

void cli::fs_update_cli::rollback_update()
{
    try
    {
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();

        this->update_handler->create_work_dir();

        if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
        {
            cli_io::write_stdout("Start application and firmware rollback\n");
            this->update_handler->rollback_application();
            this->update_handler->rollback_firmware();
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
        {
            cli_io::write_stdout("Start firmware rollback\n");
            this->update_handler->rollback_firmware();
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
        {
            cli_io::write_stdout("Rollback application start\n");
            this->update_handler->rollback_application();
        }
        else
        {
            cli_io::write_stdout("Rollback is not allowed because update reboot state is wrong.\n");
            this->print_update_reboot_state();
            return;
        }

        cli_io::write_stdout("Rollback finished successful. Reboot required.\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback update progress error: ") + e.what() + " errno: " + std::to_string(e.errorno) + "\n");
        /* Same mapper as the slot switches, because a rollback reaches the same
         * refusals: a state that says an update is in flight with a bitfield
         * that says nothing is does not take the pending arm, it falls through
         * to the committed-slot-switch verdict. Refusals the mapper does not
         * name still answer 13. */
        this->return_code = cli::map_slot_switch_errno(e.errorno);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cli_io::write_stderr(string("Rollback update error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cli_io::write_stderr(string("Rollback firmware update system error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

// ---------------------------------------------------------------------------
// Slot switch
// ---------------------------------------------------------------------------

void cli::fs_update_cli::switch_firmware_slot()
{
    try
    {
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();
        if (update_reboot_state != update_definitions::UBootBootstateFlags::NO_UPDATE_REBOOT_PENDING)
        {
            cli_io::write_stdout("Switch firmware slot is not allowed because update reboot state is wrong.\n");
            this->print_update_reboot_state();
        }
        else
        {
            cli_io::write_stdout("Start switch firmware slot\n");
            this->update_handler->create_work_dir();
            this->update_handler->rollback_firmware();
            cli_io::write_stdout("Switch firmware slot successful\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback firmware progress error: ") + e.what() + " errno : " + std::to_string(e.errorno) + "\n");

        this->return_code = cli::map_slot_switch_errno(e.errorno);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cli_io::write_stderr(string("Rollback firmware update error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cli_io::write_stderr(string("Rollback firmware update system error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::switch_application_slot()
{
    try
    {
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();
        if (update_reboot_state != update_definitions::UBootBootstateFlags::NO_UPDATE_REBOOT_PENDING)
        {
            cli_io::write_stdout("Switch application slot is not allowed because update reboot state is wrong.\n");
            this->print_update_reboot_state();
        }
        else
        {
            cli_io::write_stdout("Start switch application slot\n");
            this->update_handler->create_work_dir();
            this->update_handler->rollback_application();
            cli_io::write_stdout("Switch application slot successful\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback application progress error: ") + e.what() + " errno : " + std::to_string(e.errorno) + "\n");

        this->return_code = cli::map_slot_switch_errno(e.errorno);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cli_io::write_stderr(string("Rollback application update error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cli_io::write_stderr(string("Rollback application update system error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

void cli::fs_update_cli::print_update_reboot_state()
{
    const update_definitions::UBootBootstateFlags update_reboot_state = this->update_handler->get_update_reboot_state();

    if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE)
    {
        cli_io::write_stdout("Application update failed\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
    {
        cli_io::write_stdout("Firmware update failed\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
    {
        cli_io::write_stdout("Firmware reboot update failed\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
    {
        const fs::RebootCompleteState reboot_state = this->update_handler->is_reboot_complete(true);
        if (reboot_state == fs::RebootCompleteState::COMPLETE)
        {
            cli_io::write_stdout("Incomplete firmware update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
        }
        else if (reboot_state == fs::RebootCompleteState::PENDING)
        {
            cli_io::write_stdout("Missing reboot after firmware update requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
        else
        {
            cli_io::write_stdout("Firmware update pending; the install never reached the boot order — commit settles it and quarantines the slot, no reboot needed\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_STATE_INDETERMINATE);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
    {
        const fs::RebootCompleteState reboot_state = this->update_handler->is_reboot_complete(false);
        if (reboot_state == fs::RebootCompleteState::COMPLETE)
        {
            cli_io::write_stdout("Incomplete application update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
        }
        else if (reboot_state == fs::RebootCompleteState::PENDING)
        {
            cli_io::write_stdout("Missing reboot after application update requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
        else
        {
            cli_io::write_stdout("Application update pending; mounted image state indeterminate (no app image mounted)\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_STATE_INDETERMINATE);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
    {
        const fs::RebootCompleteState reboot_state = this->update_handler->is_reboot_complete(true);
        if (reboot_state == fs::RebootCompleteState::COMPLETE)
        {
            cli_io::write_stdout("Incomplete application and firmware update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
        }
        else if (reboot_state == fs::RebootCompleteState::PENDING)
        {
            cli_io::write_stdout("Missing reboot after application and firmware update\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
        else
        {
            /* No clean remedy for this shape: confirmPendingApplicationFirmwareUpdate()
             * has no never-activated branch (unlike its firmware-only sibling), so
             * commit throws instead of settling and exits 19, not a no-op; rollback
             * still runs the application half before the firmware half is refused,
             * though nothing persists from it. */
            cli_io::write_stdout("Application and firmware update pending; the install never reached the boot order — no clean remedy here: commit fails, rollback is refused for firmware after the application half already ran\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_STATE_INDETERMINATE);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
    {
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cli_io::write_stdout("Missing reboot after firmware rollback requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING);
        }
        else
        {
            cli_io::write_stdout("Incomplete firmware rollback. Commit requested.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
    {
        /* Only this rollback branch probes loop devices; the fw and app+fw
         * branches decide from the boot environment alone. The three outcomes
         * come from the same classification the commit's precondition reads,
         * so the code reported here and the verb that leads out cannot
         * disagree about the same device. */
        try
        {
            switch (this->update_handler->classify_app_rollback())
            {
            case updater::Bootstate::AppRollbackOutcome::REBOOT_OUTSTANDING:
                cli_io::write_stdout("Missing reboot after application rollback requested\n");
                this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
                break;

            case updater::Bootstate::AppRollbackOutcome::COMMIT_REQUESTED:
                cli_io::write_stdout("Incomplete application rollback. Commit requested.\n");
                this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
                break;

            case updater::Bootstate::AppRollbackOutcome::INDETERMINATE:
                /* Reported, not refused: the commit accepts this shape. Its own
                 * code, because a caller that cannot tell it from a landed
                 * reboot cannot tell that the image needs attention. */
                cli_io::write_stdout("Application rollback pending; mounted image state indeterminate (no app image mounted)\n");
                this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_INDETERMINATE);
                break;
            }
        }
        catch (const updater::GetLoopDevices &e)
        {
            /* The evidence could not be read at all -- a different thing from
             * "nothing is mounted", and the same answer, because neither can
             * say whether the reboot happened. */
            cli_io::write_stdout("Application rollback pending; mounted image state indeterminate (loop devices unreadable)\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_INDETERMINATE);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_FW_REBOOT_PENDING)
    {
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cli_io::write_stdout("Missing reboot after firmware and application rollback requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_FW_REBOOT_PENDING);
        }
        else
        {
            cli_io::write_stdout("Incomplete firmware and application rollback. Commit requested.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_ROLLBACK)
    {
        cli_io::write_stdout("Incomplete firmware rollback. Commit requested.\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_ROLLBACK)
    {
        cli_io::write_stdout("Incomplete application rollback. Commit requested.\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_ROLLBACK)
    {
        cli_io::write_stdout("Incomplete firmware and application rollback. Commit requested.\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::UNKNOWN_STATE)
    {
        /* Not idle: the durable state could not be read or holds content this
         * build cannot interpret. Reported on stderr and with the framework
         * exit code, so callers that treat only the idle code as "nothing to
         * do" keep skipping instead of acting on a device whose state is
         * unknown. */
        cli_io::write_stderr("Update reboot state is not interpretable; the device state is unknown\n");
        this->return_code = static_cast<int>(UPDATER_FATAL::UNHANDLED_EXCEPTION);
    }
    else
    {
        cli_io::write_stdout("No update pending\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING);
    }
}

void cli::fs_update_cli::print_current_application_version()
{
#if UPDATE_VERSION_TYPE_UINT64
    cli_io::write_stdout(std::to_string(this->update_handler->get_application_version()) + "\n");
#else
    cli_io::write_stdout(this->update_handler->get_application_version() + "\n");
#endif
}

void cli::fs_update_cli::print_current_firmware_version()
{
#if UPDATE_VERSION_TYPE_UINT64
    cli_io::write_stdout(std::to_string(this->update_handler->get_firmware_version()) + "\n");
#else
    cli_io::write_stdout(this->update_handler->get_firmware_version() + "\n");
#endif
}

// ---------------------------------------------------------------------------
// State bad get/set
// ---------------------------------------------------------------------------

void cli::fs_update_cli::set_application_state_bad(const char &state)
{
    this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL);
    if (this->update_handler->set_update_state_bad(state, application_update_state) == EINVAL)
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_application_state_bad(const char &state)
{
    if (state != 'a' && state != 'A' && state != 'b' && state != 'B')
    {
        cli_io::write_stdout("X\n");
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
    }
    else
    {
        /* The answer is the line on stdout; the code says the query ran, as
         * the setter counterpart does. */
        cli_io::write_stdout(std::to_string(this->update_handler->is_update_state_bad(state, application_update_state)) + "\n");
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL);
    }
}

void cli::fs_update_cli::set_firmware_state_bad(const char &state)
{
    this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL);
    if (this->update_handler->set_update_state_bad(state, firmware_update_state) == EINVAL)
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_firmware_state_bad(const char &state)
{
    if (state != 'a' && state != 'A' && state != 'b' && state != 'B')
    {
        cli_io::write_stdout("X\n");
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
    }
    else
    {
        /* Same contract as the application query above. */
        cli_io::write_stdout(std::to_string(this->update_handler->is_update_state_bad(state, firmware_update_state)) + "\n");
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFUL);
    }
}

// ---------------------------------------------------------------------------
// Command handlers (dispatched from parse_input)
// ---------------------------------------------------------------------------

void cli::fs_update_cli::handle_print_version()
{
    /* The project version marks the generation, not the build. The library's
     * source id is asked for at runtime, because a shared library can be
     * replaced under a CLI compiled against a different one. The timestamp is
     * the source date, not the build clock, wherever the build system pins it. */
    cli_io::write_stdout(string("F&S Update Framework CLI Version: ") + FUS_CLI_PROJECT_VERSION
        + " cli: " + FUS_CLI_SOURCE_ID
        + " lib: " + fs::library_source_id()
        + " build at: " + __DATE__ + ", " + __TIME__ + ".\n");
}

void cli::fs_update_cli::handle_print_help()
{
    /* The parser's own formatter, so --help prints exactly the USAGE block a
     * parse error shows. Rendered to a string first and written here, which is
     * what lets tests/golden/help.txt pin it without a board. */
    cli_io::write_stdout(this->args.usage_text());
    this->return_code = 0;
}

void cli::fs_update_cli::handle_is_update_available()
{
    const fus_dbus::CheckUpdateResult info = fus_dbus::check_update_available();
    if (!info.available)
    {
        cli_io::write_stdout("No updates have been found\n");
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
        return;
    }
    cli_io::write_stdout("A new update is available on the server\n");
    cli_io::write_stdout("Type: " + info.type + "\n");
    cli_io::write_stdout("Version: " + info.version + "\n");
    cli_io::write_stdout("Size: " + std::to_string(info.size) + "\n");
    if (info.type == "fw")
    {
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
    }
    else if (info.type == "app")
    {
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE);
    }
    else
    {
        this->return_code =
            static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE);
    }
}

void cli::fs_update_cli::handle_download_update()
{
    const string dl_state = fus_dbus::get_download_state();
    if (dl_state == "in_progress")
    {
        cli_io::write_stdout("Download in progress...\n");
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE);
        return;
    }

    this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
}

void cli::fs_update_cli::handle_download_progress()
{
    const string dl_state = fus_dbus::get_download_state();
    if (dl_state.empty() || dl_state == "idle")
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }
    if (dl_state == "failed")
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }
    const int pct = fus_dbus::get_download_progress();
    if (pct < 0)
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }
    cli_io::write_stdout(std::to_string(pct) + "%\n");
    if (pct < 100)
    {
        this->return_code =
            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS);
    }
    else
    {
        this->return_code =
            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED);
    }
}

void cli::fs_update_cli::handle_install_progress()
{
    const string install_state = fus_dbus::get_install_state();
    if (install_state.empty() || install_state == "idle") {
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
        return;
    }

    /* Print the final percent before returning, including for the
     * "failed" terminal state — callers want to see how far the
     * install got before it failed. */
    const int pct = fus_dbus::get_install_progress();
    if (pct < 0) {
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
        return;
    }
    cli_io::write_stdout(std::to_string(pct) + "%\n");

    if (install_state == "failed") {
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
    } else if (install_state == "finished") {
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED);
    } else {
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
    }
}

void cli::fs_update_cli::handle_cancel_install()
{
    const uint32_t sid = this->args.cancel_session_id();
    if (sid == 0) {
        cli_io::write_stderr("--cancel_install requires a non-zero session_id\n");
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
        return;
    }
    if (!fus_dbus::cancel_install(sid)) {
        cli_io::write_stderr("CancelInstall D-Bus call failed\n");
        this->return_code =
            static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
        return;
    }
    cli_io::write_stdout("Cancel requested for session " +
                         std::to_string(sid) +
                         "; final outcome via --install_progress.\n");
    this->return_code =
        static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
}

void cli::fs_update_cli::handle_install_update()
{
    /* Local install: --install_update <path> → InstallLocal. Blocking by
     * default (stream progress, return the terminal 0/4/8 ÷ 3/7/11 verdict);
     * --detach returns immediately with the session id. */
    if (this->install_outcome.mode == cli::InstallMode::local)
    {
        const string path = this->install_outcome.install_path;
        /* Resolve to an absolute path in the CLI's working directory: the
         * service receives this path and resolves it against ITS own CWD, so a
         * relative path would point at a different (or missing) file. realpath
         * also verifies existence/accessibility. */
        char* resolved = ::realpath(path.c_str(), nullptr);
        if (resolved == nullptr)
        {
            cli_io::write_stderr("Update file: " + path +
                                 " does not exist or is not accessible.\n");
            this->return_code =
                static_cast<int>(UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND);
            return;
        }
        const std::unique_ptr<char, decltype(&std::free)> resolved_guard(resolved, std::free);
        const string abs_path(resolved);

        cli::CallError cerr = cli::CallError::other;
        const uint32_t sid = fus_dbus::install_local(abs_path, cerr);
        if (sid == 0)
        {
            /* Message only — the return code comes from the classifier seam so
             * the cli/service error contract lives in one natively-tested place. */
            switch (cerr)
            {
            case cli::CallError::busy:
                cli_io::write_stderr("Another install or download is already in progress.\n");
                break;
            case cli::CallError::denied:
                cli_io::write_stderr("Permission denied by policy for the install request.\n");
                break;
            case cli::CallError::no_updater:
                cli_io::write_stderr("Updater is not available on the service.\n");
                break;
            case cli::CallError::none:
                cli_io::write_stdout("Install accepted; session id unreadable — "
                                     "poll --install_progress for status.\n");
                break;
            case cli::CallError::other:
                cli_io::write_stderr("InstallLocal D-Bus call failed\n");
                break;
            }
            this->return_code = cli::install_start_error_code(cerr);
            return;
        }

        if (this->install_outcome.detach)
        {
            cli_io::write_stdout("Install started; session " + std::to_string(sid) +
                                 ". Poll --install_progress for status.\n");
            this->return_code =
                static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
            return;
        }

        const fus_dbus::InstallResult r =
            fus_dbus::wait_for_install(sid, install_wait_idle_ms(),
                                       &render_install_progress);
        cli_io::write_stdout("\n");
        switch (r.status)
        {
        case fus_dbus::InstallResult::Status::completed:
            /* Terminal verdict mapped into the 0/4/8 ÷ 3/7/11 family by type. */
            this->return_code = cli::install_terminal_code(r.success, r.type);
            break;
        case fus_dbus::InstallResult::Status::timed_out:
            /* Install may still be running — report in-progress, not failed. */
            cli_io::write_stderr("Install still running after the no-progress "
                                 "timeout; poll --install_progress.\n");
            this->return_code =
                static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
            break;
        case fus_dbus::InstallResult::Status::observe_error:
            cli_io::write_stderr("Could not observe the install over D-Bus; "
                                 "poll --install_progress.\n");
            this->return_code =
                static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
            break;
        }
        return;
    }

    const string install_state = fus_dbus::get_install_state();
    if (install_state == "finished")
    {
        cli_io::write_stdout("Update installation finished.\n");
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED);
        return;
    }
    if (install_state == "in_progress")
    {
        cli_io::write_stdout("Update installation in progress.\n");
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
        return;
    }
    if (fus_dbus::get_download_state() != "finished")
    {
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
        return;
    }
    const uint32_t sid = fus_dbus::get_session_id();
    if (sid == 0)
    {
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
        return;
    }
    const string type = fus_dbus::get_update_type();
    if (type.empty())
    {
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
        return;
    }
    cli::CallError cloud_err = cli::CallError::other;
    if (!fus_dbus::start_install(sid, type, cloud_err))
    {
        /* Same flag, same service condition as the local form — surface the
         * same distinct busy/denied codes instead of one blanket failure. */
        switch (cloud_err)
        {
        case cli::CallError::busy:
            cli_io::write_stderr("Another install or download is already in progress.\n");
            break;
        case cli::CallError::denied:
            cli_io::write_stderr("Permission denied by policy for the install request.\n");
            break;
        default:
            cli_io::write_stdout("Could not initiate Installation...\n");
            break;
        }
        this->return_code = cli::install_start_error_code(cloud_err);
        return;
    }
    cli_io::write_stdout("Update installation started.\n");
    this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
}

void cli::fs_update_cli::handle_apply_update()
{
    if (fus_dbus::get_install_state() == "finished")
    {
        bool reboot_needed = false;
        if (!fus_dbus::apply(reboot_needed))
        {
            cli_io::write_stdout("Initiate of update apply fails...\n");
            this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
            return;
        }
        cli_io::write_stdout("Apply update...\n");
        if (reboot_needed && this->reboot() != 0)
        {
            const int saved = errno;
            cli_io::write_stderr(string("Failed to reboot system: ") + strerror(saved) + "\n");
            this->return_code = static_cast<int>(UPDATER_SYSTEM::REBOOT_FAILED);
            return;
        }
        this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL);
        return;
    }
    if (fus_dbus::get_install_state() == "in_progress")
    {
        /* Install still running: refuse to apply until it finishes,
         * otherwise the durable-state fallback would reboot mid-write. */
        cli_io::write_stdout("Install in progress; cannot apply yet.\n");
        this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
        return;
    }

    /* No install tracked in this session: consult the durable
     * update_reboot_state via the lib. */
    bool reboot_needed = false;
    try
    {
        reboot_needed = this->update_handler->apply_pending_update();
    }
    catch (const fs::ApplyUpdateInvalidState &)
    {
        cli_io::write_stdout("Nothing to apply...\n");
        this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
        return;
    }
}

void cli::fs_update_cli::handle_set_app_state_bad()
{
    this->set_application_state_bad(this->args.app_state_to_set());
}

void cli::fs_update_cli::handle_is_app_state_bad()
{
    this->is_application_state_bad(this->args.app_state_to_query());
}

void cli::fs_update_cli::handle_set_fw_state_bad()
{
    this->set_firmware_state_bad(this->args.fw_state_to_set());
}

void cli::fs_update_cli::handle_is_fw_state_bad()
{
    this->is_firmware_state_bad(this->args.fw_state_to_query());
}

// ---------------------------------------------------------------------------
// Command dispatch
    catch (const std::exception &e)
    {
        /* Apply failed: report and leave the state machine untouched. */
        cli_io::write_stderr(string("Initiate of update apply fails... ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
        return;
    }

    cli_io::write_stdout("Apply update...\n");
    if (reboot_needed && this->reboot() != 0)
    {
        const int saved = errno;
        cli_io::write_stderr(string("Failed to reboot system: ") + strerror(saved) + "\n");
        this->return_code = static_cast<int>(UPDATER_SYSTEM::REBOOT_FAILED);
        return;
    }
    this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL);
// ---------------------------------------------------------------------------

void cli::fs_update_cli::parse_input(int argc, const char **argv)
{
    const cli::ParseResult parsed = this->args.parse(argc, argv);

    /* Everything up to setup_logging() must stay hardware-free: printing usage
     * or rejecting a bad argument must not construct the updater (FSUpdate ->
     * UBoot), which fails on a board whose boot environment is unreadable. */
    switch (parsed.kind)
    {
    case cli::ParseResult::Kind::parse_error:
        /* The parser's own wording, on the streams it used: banner to stderr,
         * usage to stdout. */
        cli_io::write_stderr(this->args.error_banner());
        cli_io::write_stdout(this->args.usage_text());
        this->return_code = parsed.rc;
        return;

    case cli::ParseResult::Kind::help:
        this->handle_print_help();
        return;

    case cli::ParseResult::Kind::guard_error:
        cli_io::write_stderr(parsed.install_outcome.error + "\n");
        this->return_code = parsed.rc;
        return;

    case cli::ParseResult::Kind::ok:
    case cli::ParseResult::Kind::version_only:
    case cli::ParseResult::Kind::combo_error:
        break;
    }

    this->install_outcome = parsed.install_outcome;

    /* The verdict is known before this point, but acting on it early would move
     * the no-argument and bad-combination paths ahead of updater construction
     * and change what they return on a board that cannot construct it. */
    this->setup_logging();

    switch (parsed.kind)
    {
    case cli::ParseResult::Kind::version_only:
        this->handle_print_version();
        cli_io::write_stdout("No argument given, nothing done. Use --help to get all commands.\n");
        break;

    case cli::ParseResult::Kind::combo_error:
        cli_io::write_stderr("Wrong combination or set of variables. Please refer --help or manual\n");
        this->return_code = parsed.rc;
        break;

    case cli::ParseResult::Kind::ok:
        this->dispatch(parsed.command);
        break;

    case cli::ParseResult::Kind::parse_error:
    case cli::ParseResult::Kind::help:
    case cli::ParseResult::Kind::guard_error:
        break; /* returned above */
    }
}

void cli::fs_update_cli::dispatch(cli::Command command)
{
    switch (command)
    {
    case cli::Command::commit_update:       this->commit_update(); break;
    case cli::Command::update_reboot_state: this->print_update_reboot_state(); break;
    case cli::Command::application_version: this->print_current_application_version(); break;
    case cli::Command::firmware_version:    this->print_current_firmware_version(); break;
    case cli::Command::print_version:       this->handle_print_version(); break;
    case cli::Command::is_update_available: this->handle_is_update_available(); break;
    case cli::Command::download_update:     this->handle_download_update(); break;
    case cli::Command::download_progress:   this->handle_download_progress(); break;
    case cli::Command::install_update:      this->handle_install_update(); break;
    case cli::Command::install_progress:    this->handle_install_progress(); break;
    case cli::Command::cancel_install:      this->handle_cancel_install(); break;
    case cli::Command::apply_update:        this->handle_apply_update(); break;
    case cli::Command::rollback_update:     this->rollback_update(); break;
    case cli::Command::switch_fw_slot:      this->switch_firmware_slot(); break;
    case cli::Command::switch_app_slot:     this->switch_application_slot(); break;
    case cli::Command::set_app_state_bad:   this->handle_set_app_state_bad(); break;
    case cli::Command::is_app_state_bad:    this->handle_is_app_state_bad(); break;
    case cli::Command::set_fw_state_bad:    this->handle_set_fw_state_bad(); break;
    case cli::Command::is_fw_state_bad:     this->handle_is_fw_state_bad(); break;
    case cli::Command::none:                break; /* not reachable: Kind::ok implies an action */
    }
}

int cli::fs_update_cli::getReturnCode() const
{
    return this->return_code;
}

int cli::fs_update_cli::reboot() const
{
    /* Trigger systemd's orderly shutdown via SIGINT to PID 1.
     * SIGINT → ctrl-alt-del.target → reboot.target → graceful unit stop + reboot.
     * Uses kill(2) directly: a POSIX syscall, with no process spawning.
     * ::sync() flushes dirty buffers before systemd begins stopping services.
     */
    ::sync();
    return ::kill(1, SIGINT);
}
