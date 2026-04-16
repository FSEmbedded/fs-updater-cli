#include "cli.h"
#include "fs_updater_error.h"
#include "fs_updater_types.h"
#include "posix_helpers.h"
#include "cli_io.h"
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>

constexpr char FSCLI_DOMAIN[] = "cli";

constexpr uint32_t firmware_update_state = 0;
constexpr uint32_t application_update_state = 1;

using std::string;

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		cmd("F&S Update Framework CLI", ' ', FUS_CLI_PROJECT_VERSION, false),
		arg_update("",
		       "update_file",
		       "Path to update package",
		       false,
		       "",
		       "absolute filesystem path"
		       ),
		arg_update_type("",
		       "update_type",
		       "Update type firmware or application",
		       false,
		       "",
		       "accepted values: fw or app"
		),
		arg_switch_fw_slot("",
				"switch_fw_slot",
				"Switch from active firmware slot to the inactive "\
				"(apply update required)"
				),
		arg_switch_app_slot("",
				 "switch_app_slot",
				 "Switch from active to the inactive application slot. "\
				 "(apply update required)"
				 ),
		arg_rollback_update("",
				 "rollback_update",
				 "Rollback of the last installed update "\
				 "(must be started before commit update)"
				 ),
		arg_commit_update("",
				  "commit_update",
				  "Confirm success of installation, rollback, switch or fail. "\
				  "Run after boot and waits for application response"
				  ),
		arg_urs("",
			"update_reboot_state",
			"Get state of update"
			),
		arg_automatic("",
			    "automatic",
				"Automatic update modus"
			    ),
		arg_debug("",
			  "debug",
			  "Enable debug output"
			  ),
		get_fw_version("",
			       "firmware_version",
			       "Show current firmware version"
			       ),
		get_app_version("",
				"application_version",
				"Show current application version"
				),
		get_version("",
			    "version",
			    "Print cli version"
			    ),
		notice_update_available("",
					"is_update_available",
					"Check update available on the server"
					),
		apply_update("",
			     "apply_update",
			     "Apply update installation, rollback or switch to other slot. "\
				 "Reboot to the updated slot."
			     ),
		download_progress("",
				  "download_progress",
				  "Show the progress of the current update"
				  ),
		download_update("",
					    "download_update",
					    "Download the available update"
					    ),
		install_update("",
					   "install_update",
					   "Install downloaded update"
					   ),
		set_app_state_bad("",
			    "set_app_state_bad",
				"Mark application A or B bad",
				false,
			    'c',
				"accepted states: A or B"
			    ),
		is_app_state_bad("",
			    "is_app_state_bad",
				"Check application state for bad",
				false,
			    'a',
				"accepted states: A or B"
			    ),
		set_fw_state_bad("",
			    "set_fw_state_bad",
				"Mark firmware A or B bad",
				false,
			    'c',
				"accepted states: A or B"
			    ),
		is_fw_state_bad("",
			    "is_fw_state_bad",
				"Check firmware state for bad",
				false,
			    'a',
				"accepted states: A or B"
			    ),
		return_code(0)
{
    this->cmd.add(arg_update);
    this->cmd.add(arg_update_type);
    this->cmd.add(arg_rollback_update);
    this->cmd.add(arg_switch_fw_slot);
    this->cmd.add(arg_switch_app_slot);
    this->cmd.add(arg_commit_update);
    this->cmd.add(arg_urs);
    this->cmd.add(arg_automatic);
    this->cmd.add(arg_debug);
    this->cmd.add(get_fw_version);
    this->cmd.add(get_app_version);
    this->cmd.add(get_version);
    this->cmd.add(apply_update);
    this->cmd.add(install_update);
    this->cmd.add(download_progress);
    this->cmd.add(download_update);
    this->cmd.add(notice_update_available);
    this->cmd.add(set_app_state_bad);
    this->cmd.add(is_app_state_bad);
    this->cmd.add(set_fw_state_bad);
    this->cmd.add(is_fw_state_bad);

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
    const bool is_automatic = this->arg_automatic.isSet();
    const auto level = this->arg_debug.isSet()
        ? logger::logLevel::DEBUG
        : logger::logLevel::WARNING;

    if (is_automatic)
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

bool cli::fs_update_cli::create_rollback_marker()
{
    const string work_dir = this->update_handler->get_work_dir().string();
    const string marker = posix_helpers::path_join(work_dir, "rollbackUpdate");
    if (!posix_helpers::create_marker_file(marker.c_str()))
    {
        cli_io::write_stderr("Failed to create rollback marker file\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Update execution
// ---------------------------------------------------------------------------

void cli::fs_update_cli::update_image_state(const string &update_file)
{
    try
    {
        cli_io::write_stdout("Update started\n");
        uint8_t installed_update_type = 0;
        string update_type;
        if (this->arg_update_type.isSet())
        {
            update_type = this->arg_update_type.getValue();
            if ((update_type.compare("app") != 0) && (update_type.compare("fw") != 0))
            {
                cli_io::write_stderr("Update type: " + update_type + " does not exist.\n");
                this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::INVALID_UPDATE_TYPE);
                return;
            }
        }

        string mutable_file = update_file;
        this->update_handler->update_image(mutable_file, update_type, installed_update_type);

        switch(installed_update_type)
        {
            case 1:
            this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL);
            break;
            case 2:
            this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL);
            break;
            case 3:
            this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL);
            break;
            default:
            this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
        }

        cli_io::write_stdout("Image update successful\n");
    }
    catch (const fs::UpdateInProgress &e)
    {
        cli_io::write_stderr(string("Image update progress error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string(e.what()) + " errno: " + std::to_string(e.errorno) + "\n");
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        const string tmp_app = this->update_handler->getTempAppPath().string();
        static_cast<void>(posix_helpers::remove_file(tmp_app.c_str()));
        cli_io::write_stderr(string("Image update error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cli_io::write_stderr(string("Image update system error: ") + e.what() + "\n");
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
    }
}

// ---------------------------------------------------------------------------
// Commit
// ---------------------------------------------------------------------------

void cli::fs_update_cli::commit_update()
{
    try
    {
        if (this->update_handler->commit_update() == true)
        {
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

        if (!this->create_rollback_marker())
        {
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
            return;
        }
        cli_io::write_stdout("Rollback finished successful. Reboot required.\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback update progress error: ") + e.what() + " errno: " + std::to_string(e.errorno) + "\n");
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
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
            if (!this->create_rollback_marker())
            {
                this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
                return;
            }
            cli_io::write_stdout("Switch firmware slot successful\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback firmware progress error: ") + e.what() + " errno : " + std::to_string(e.errorno) + "\n");

        switch (e.errorno)
        {
        case EPERM:
            this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
            break;
        case ECANCELED:
            this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
            break;
        default:
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
        }
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
            if (!this->create_rollback_marker())
            {
                this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
                return;
            }
            cli_io::write_stdout("Switch application slot successful\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cli_io::write_stderr(string("Rollback application progress error: ") + e.what() + " errno : " + std::to_string(e.errorno) + "\n");

        switch (e.errorno)
        {
        case EPERM:
            this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
            break;
        case ECANCELED:
            this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
            break;
        default:
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
        }
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
        if (this->update_handler->is_reboot_complete(true))
        {
            cli_io::write_stdout("Incomplete firmware update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
        }
        else
        {
            cli_io::write_stdout("Missing reboot after firmware update requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
    {
        if (this->update_handler->is_reboot_complete(false))
        {
            cli_io::write_stdout("Incomplete application update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
        }
        else
        {
            cli_io::write_stdout("Missing reboot after application update requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
    {
        if (this->update_handler->is_reboot_complete(true))
        {
            cli_io::write_stdout("Incomplete application and firmware update. Commit required.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
        }
        else
        {
            cli_io::write_stdout("Missing reboot after application and firmware update\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
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
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cli_io::write_stdout("Missing reboot after application rollback requested\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
        }
        else
        {
            cli_io::write_stdout("Incomplete application rollback. Commit requested.\n");
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
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
        cli_io::write_stdout(std::to_string(this->update_handler->is_update_state_bad(state, application_update_state)) + "\n");
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
        cli_io::write_stdout(std::to_string(this->update_handler->is_update_state_bad(state, firmware_update_state)) + "\n");
    }
}

// ---------------------------------------------------------------------------
// Command handlers (dispatched from parse_input)
// ---------------------------------------------------------------------------

void cli::fs_update_cli::handle_update_file()
{
    const string update_location = this->arg_update.getValue();

    if (!posix_helpers::path_exists(update_location.c_str()))
    {
        cli_io::write_stderr("Update file: " + update_location + " does not exist.\n");
        this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND);
        return;
    }
    this->update_image_state(update_location);
}

void cli::fs_update_cli::handle_automatic()
{
    const char *update_stick_env = std::getenv("UPDATE_STICK");
    const char *update_file_env = std::getenv("UPDATE_FILE");

    if (update_stick_env == nullptr)
    {
        this->serial_cout->write("Environment variable \"UPDATE_STICK\" is not set\n");
        this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_STICK);
        return;
    }

    if (update_file_env == nullptr)
    {
        this->serial_cout->write("\"UPDATE_FILE\" env variable -- not set\n");
        this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::MISSING_ENV_UPDATE_FILE);
        return;
    }

    const string update_stick(update_stick_env);
    string update_file = update_stick;
    if (update_stick.back() != '/')
    {
        update_file += "/";
    }
    update_file += update_file_env;

    this->update_image_state(update_file);
}

void cli::fs_update_cli::handle_print_version()
{
    cli_io::write_stdout(string("F&S Update Framework CLI Version: ") + FUS_CLI_PROJECT_VERSION
        + " build at: " + __DATE__ + ", " + __TIME__ + ".\n");
}

void cli::fs_update_cli::handle_is_update_available()
{
    const string work_dir = this->update_handler->get_work_dir().string();

    string updateType;
    if (!posix_helpers::read_file(posix_helpers::path_join(work_dir, "update_type").c_str(), updateType))
    {
        cli_io::write_stdout("No updates have been found\n");
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
        return;
    }

    string updateVersion;
    if (!posix_helpers::read_file(posix_helpers::path_join(work_dir, "update_version").c_str(), updateVersion))
    {
        cli_io::write_stdout("No updates have been found\n");
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
        return;
    }

    string updateSize;
    if (!posix_helpers::read_file(posix_helpers::path_join(work_dir, "update_size").c_str(), updateSize))
    {
        cli_io::write_stdout("No updates have been found\n");
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
        return;
    }

    cli_io::write_stdout("A new update is available on the server\n");
    cli_io::write_stdout("Type: " + updateType + "\n");
    cli_io::write_stdout("Version: " + updateVersion + "\n");
    cli_io::write_stdout("Size: " + updateSize + "\n");

    if (updateType == "firmware")
    {
        this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
    }
    else if (updateType == "application")
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
    const string work_dir = this->update_handler->get_work_dir().string();
    if (!posix_helpers::path_exists(posix_helpers::path_join(work_dir, "update_type").c_str()) ||
        !posix_helpers::path_exists(posix_helpers::path_join(work_dir, "update_version").c_str()) ||
        !posix_helpers::path_exists(posix_helpers::path_join(work_dir, "update_size").c_str()))
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
    }
    else if (posix_helpers::path_exists(posix_helpers::path_join(work_dir, "downloadUpdate").c_str()))
    {
        cli_io::write_stdout("Download in progress...\n");
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE);
    }
    else
    {
        const string marker = posix_helpers::path_join(work_dir, "downloadUpdate");
        if (!posix_helpers::create_marker_file(marker.c_str()))
        {
            cli_io::write_stdout("Could not initiate update download...\n");
            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED);
        }
        else
        {
            cli_io::write_stdout("Download started...\n");
            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED);
        }
    }
}

void cli::fs_update_cli::handle_download_progress()
{
    const string work_dir = this->update_handler->get_work_dir().string();
    if (!posix_helpers::path_exists(posix_helpers::path_join(work_dir, "downloadUpdate").c_str()))
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }

    uint64_t update_size = 0;
    string size_str;
    const string size_path = posix_helpers::path_join(work_dir, "update_size");
    if (!posix_helpers::read_file(size_path.c_str(), size_str))
    {
        cli_io::write_stdout("Update size not available: " + std::to_string(errno) + "\n");
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }
    try
    {
        update_size = std::stoull(size_str);
    }
    catch (const std::exception &)
    {
        cli_io::write_stderr("Update size file contains invalid data\n");
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }

    if (update_size == 0)
    {
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }

    const string update_location = posix_helpers::path_join(work_dir, "update_location");
    const ssize_t loc_size = posix_helpers::file_size(update_location.c_str());

    if (loc_size < 0 || loc_size <= 9)
    {
        cli_io::write_stdout("Waiting to start download.\n");
        this->return_code =
            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START);
        return;
    }

    string update_file_path;
    if (!posix_helpers::read_file(update_location.c_str(), update_file_path))
    {
        this->return_code =
            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START);
        return;
    }

    if (!posix_helpers::path_exists(update_file_path.c_str()))
    {
        cli_io::write_stderr("Update file: " + update_file_path + " does not exist.\n");
        this->return_code =
            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START);
        return;
    }

    const ssize_t filesize_s = posix_helpers::file_size(update_file_path.c_str());
    if (filesize_s <= 0)
    {
        if (filesize_s == 0)
            cli_io::write_stdout("Size of loaded update: 0...\n");
        this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
        return;
    }

    const uint64_t filesize = static_cast<uint64_t>(filesize_s);
    cli_io::write_stdout("Size of loaded update: " + std::to_string(filesize) + "...\n");

    const int percent = static_cast<int>((filesize * 100U) / update_size);

    cli_io::write_stdout(std::to_string(filesize) + "/" + std::to_string(update_size) + " -- " + std::to_string(percent) + "%\n");

    if (percent < 100)
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

void cli::fs_update_cli::handle_install_update()
{
    const string work_dir = this->update_handler->get_work_dir().string();

    if (posix_helpers::path_exists(posix_helpers::path_join(work_dir, "updateInstalled").c_str()))
    {
        cli_io::write_stdout("Update installation finished.\n");
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED);
    }
    else if (!posix_helpers::path_exists(posix_helpers::path_join(work_dir, "update_location").c_str()))
    {
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
    }
    else if (posix_helpers::path_exists(posix_helpers::path_join(work_dir, "installUpdate").c_str()))
    {
        cli_io::write_stdout("Update installation in progress.\n");
        this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
    }
    else
    {
        const string marker = posix_helpers::path_join(work_dir, "installUpdate");
        if (!posix_helpers::create_marker_file(marker.c_str()))
        {
            cli_io::write_stdout("Could not initiate Installation...\n");
            this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
        }
        else
        {
            cli_io::write_stdout("Update installation started.\n");
            this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
        }
    }
}

void cli::fs_update_cli::handle_apply_update()
{
    const string work_dir = this->update_handler->get_work_dir().string();
    const string installed_path = posix_helpers::path_join(work_dir, "updateInstalled");
    const string rollback_path = posix_helpers::path_join(work_dir, "rollbackUpdate");

    if (posix_helpers::path_exists(installed_path.c_str()))
    {
        if (!posix_helpers::path_exists(posix_helpers::path_join(work_dir, "applyUpdate").c_str()) &&
            !posix_helpers::path_exists(posix_helpers::path_join(work_dir, "downloadUpdate").c_str()))
        {
            cli_io::write_stdout("Apply update...\n");
            if(this->reboot() != 0) {
                const int saved = errno;
                cli_io::write_stderr(string("Failed to reboot system: ") + strerror(saved) + "\n");
                this->return_code = static_cast<int>(UPDATER_SYSTEM::REBOOT_FAILED);
            } else {
                this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL);
            }
        }
        else
        {
            const string apply_marker = posix_helpers::path_join(work_dir, "applyUpdate");
            if (!posix_helpers::create_marker_file(apply_marker.c_str()))
            {
                // errno is clobbered by create_marker_file's internal close();
                // reading it here would be meaningless.
                cli_io::write_stdout("Initiate of update apply fails...\n");
                this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
            }
            else
            {
                cli_io::write_stdout("Apply update...\n");
                this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL);
            }
        }
    }
    else if (posix_helpers::path_exists(rollback_path.c_str()))
    {
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();

        if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_FW_REBOOT_PENDING)
        {
            this->update_handler->update_reboot_state(
                update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_ROLLBACK);
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
        {
            this->update_handler->update_reboot_state(
                update_definitions::UBootBootstateFlags::INCOMPLETE_FW_ROLLBACK);
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
        {
            this->update_handler->update_reboot_state(
                update_definitions::UBootBootstateFlags::INCOMPLETE_APP_ROLLBACK);
        }

        cli_io::write_stdout("Apply rollback update...\n");

        if(this->reboot() != 0) {
            const int saved = errno;
            cli_io::write_stderr(string("Failed to reboot system: ") + strerror(saved) + "\n");
            this->return_code = static_cast<int>(UPDATER_SYSTEM::REBOOT_FAILED);
        } else {
            this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL);
        }

        if (this->return_code != static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFUL))
        {
            this->update_handler->update_reboot_state(update_reboot_state);
        }
    }
    else
    {
        cli_io::write_stdout("Nothing to apply...\n");
        this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
    }
}

void cli::fs_update_cli::handle_set_app_state_bad()
{
    this->set_application_state_bad(this->set_app_state_bad.getValue());
}

void cli::fs_update_cli::handle_is_app_state_bad()
{
    this->is_application_state_bad(this->is_app_state_bad.getValue());
}

void cli::fs_update_cli::handle_set_fw_state_bad()
{
    this->set_firmware_state_bad(this->set_fw_state_bad.getValue());
}

void cli::fs_update_cli::handle_is_fw_state_bad()
{
    this->is_firmware_state_bad(this->is_fw_state_bad.getValue());
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

void cli::fs_update_cli::parse_input(int argc, const char **argv)
{
    this->cmd.parse(argc, argv);

    this->setup_logging();

    /* Dispatch table: maps each action flag to its handler.
     * --debug and --update_type are modifiers, not actions.
     * All action flags are mutually exclusive.
     */
    struct ActionEntry {
        TCLAP::Arg* arg;
        void (fs_update_cli::*handler)();
    };

    const std::array<ActionEntry, 19> actions = {{
        {&arg_update,              &fs_update_cli::handle_update_file},
        {&arg_commit_update,       &fs_update_cli::commit_update},
        {&arg_urs,                 &fs_update_cli::print_update_reboot_state},
        {&arg_automatic,           &fs_update_cli::handle_automatic},
        {&get_app_version,         &fs_update_cli::print_current_application_version},
        {&get_fw_version,          &fs_update_cli::print_current_firmware_version},
        {&get_version,             &fs_update_cli::handle_print_version},
        {&notice_update_available, &fs_update_cli::handle_is_update_available},
        {&download_update,         &fs_update_cli::handle_download_update},
        {&download_progress,       &fs_update_cli::handle_download_progress},
        {&install_update,          &fs_update_cli::handle_install_update},
        {&apply_update,            &fs_update_cli::handle_apply_update},
        {&arg_rollback_update,     &fs_update_cli::rollback_update},
        {&arg_switch_fw_slot,      &fs_update_cli::switch_firmware_slot},
        {&arg_switch_app_slot,     &fs_update_cli::switch_application_slot},
        {&set_app_state_bad,       &fs_update_cli::handle_set_app_state_bad},
        {&is_app_state_bad,        &fs_update_cli::handle_is_app_state_bad},
        {&set_fw_state_bad,        &fs_update_cli::handle_set_fw_state_bad},
        {&is_fw_state_bad,         &fs_update_cli::handle_is_fw_state_bad},
    }};

    void (fs_update_cli::*matched_handler)() = nullptr;
    int action_count = 0;

    for (const auto& entry : actions)
    {
        if (entry.arg->isSet())
        {
            matched_handler = entry.handler;
            ++action_count;
        }
    }

    /* --update_type is only valid with --update_file */
    if (this->arg_update_type.isSet() && !this->arg_update.isSet())
    {
        cli_io::write_stderr("--update_type can only be used with --update_file\n");
        this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::UPDATE_TYPE_WITHOUT_FILE);
        return;
    }

    if (action_count == 0)
    {
        this->handle_print_version();
        cli_io::write_stdout("No argument given, nothing done. Use --help to get all commands.\n");
    }
    else if (action_count == 1)
    {
        (this->*matched_handler)();
    }
    else
    {
        cli_io::write_stderr("Wrong combination or set of variables. Please refer --help or manual\n");
        this->return_code = static_cast<int>(UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO);
    }
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

int cli::fs_update_cli::getReturnCode() const
{
    return this->return_code;
}

int cli::fs_update_cli::reboot() const
{
    /* Trigger systemd's orderly shutdown via SIGINT to PID 1.
     * SIGINT → ctrl-alt-del.target → reboot.target → graceful unit stop + reboot.
     * Uses kill(2) directly: POSIX syscall, no fork/exec/system (MISRA-compliant).
     * ::sync() flushes dirty buffers before systemd begins stopping services.
     */
    ::sync();
    return ::kill(1, SIGINT);
}
