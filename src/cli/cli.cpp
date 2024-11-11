#include "cli.h"
#include "fs_updater_error.h"
#include "fs_updater_types.h"
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/reboot.h>

constexpr char FSCLI_DOMAIN[] = "cli";

#define FIRMWARE_UPDATE_STATE 0
#define APPLICATION_UPDATE_STATE 1

/* declaration block for std namespace */
using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::string;
using std::stringstream;

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		cmd("F&S Update Framework CLI", ' ', VERSION, false),
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
		arg_rollback_update("",
				 "rollback_update",
				 "Rollback of the last installed update "\
				 "(must be started before commit update)"
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
		get_version("",
			    "version",
			    "Print cli version"
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
				"Check firwmare state for bad",
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
    proceeded_update_type = UPDATE_UNKNOWN;
}

cli::fs_update_cli::~fs_update_cli()
{
}

void cli::fs_update_cli::update_image_state(const char *update_file_env, const string &update_stick, bool use_arg)
{
    try
    {
        cout << "Update started" << endl;
        /* get update image type: fw, app or fw and app */
        uint8_t installed_update_type = 0;
        string update_file;
        /* update type in case second parameter for installation process */
        string update_type;
        bool isUpdateTypeAvailable = this->arg_update_type.isSet();
        if (isUpdateTypeAvailable == true)
        {
            update_type = this->arg_update_type.getValue();
            if ((update_type.compare("app") != 0) && (update_type.compare("fw") != 0))
            {
                cerr << "Update type: " << update_type << "does not exists." << endl;
                this->return_code = EPERM;
                return;
            }
        }
        /* Build string for update file with full path.  */
        if (use_arg == true)
        {
            update_file = this->arg_update.getValue();
        }
        else
        {
            update_file = update_stick;
            if (update_stick.back() != '/')
            {
                update_file += string("/");
            }

            update_file += string(update_file_env);
        }
        /* start update */
        this->update_handler->update_image(update_file, update_type, installed_update_type);

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

        cout << "Image update successful" << endl;
    }
    catch (const fs::UpdateInProgress &e)
    {
        cerr << "Image update progress error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
    }
    catch (const fs::GenericException &e)
    {
        cerr << e.what() << " errno: " << e.errorno << endl;
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        /* Remove tmp.app in case the file tmp.app is available
         * This is the application update temporary file before rename to
         * updated application image. In case update fails old version should be
         * removed.
         */
        std::filesystem::path tmp_app = this->update_handler->getTempAppPath();
        std::filesystem::remove(tmp_app);
        cerr << "Image update error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cerr << "Image update system error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::commit_update()
{
    try
    {
        if (this->update_handler->commit_update() == true)
        {
            cout << "Commit update" << endl;
            this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_COMMIT_SUCCESSFUL);
        }
        else
        {
            cout << "Commit update not needed" << endl;
            this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED);
        }
    }
    catch (const fs::NotAllowedUpdateState &e)
    {
        cerr << "Not allowed update state in UBoot" << endl;
        this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE);
    }
    catch (const std::exception &e)
    {
        cerr << "FS updater cli error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::rollback_update()
{
    try
    {
        /* get last update reboot state */
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();
        /* Create directory if not exists.
         * GenericException is possible if not available directory
         * can't be created.
         */
        this->update_handler->create_work_dir();

        if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
        {
            cout << "Start application and firmware rollback" << endl;
            /* do rollback application */
            this->update_handler->rollback_application();
            /* do rollback firmware */
            this->update_handler->rollback_firmware();
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ofstream rollback((work_dir / "rollbackUpdate"));
            rollback.close();
            cout << "Rollback finished successful. Reboot required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
        {
            cout << "Start firmware rollback" << endl;
            /* do rollback firmware */
            this->update_handler->rollback_firmware();
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ofstream rollback((work_dir / "rollbackUpdate"));
            rollback.close();
            cout << "Rollback finished successful. Reboot required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
        else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
        {
            cout << "Rollback application start" << endl;
            /* do rollback firmware */
            this->update_handler->rollback_application();
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ofstream rollback((work_dir / "rollbackUpdate"));
            rollback.close();
            cout << "Rollback finished successful. Reboot required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
        else
        {
            cout << "Rollback is not allowed because update reboot state is wrong." << endl;
            /* return_code would be setted by the function */
            this->print_update_reboot_state();
        }
    }
    catch (const fs::GenericException &e)
    {
        cerr << "Rollback update progress error: " << e.what() << " errno: " << e.errorno << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cerr << "Rollback update error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cerr << "Rollback firmware update system error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::switch_firmware_slot()
{
    try
    {
        /* get last update reboot state */
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();
        if (update_reboot_state != update_definitions::UBootBootstateFlags::NO_UPDATE_REBOOT_PENDING)
        {
            cout << "Switch firmware slot is not allowed because update reboot state is wrong." << endl;
            /* return_code would be setted by the function */
            this->print_update_reboot_state();
        }
        else
        {
            cout << "Start switch firmware slot" << endl;
            /* get last update reboot state */
            const update_definitions::UBootBootstateFlags update_reboot_state =
                this->update_handler->get_update_reboot_state();
            /* Create directory if not exists.
             * GenericException is possible if directory does not exists and
             * can not be created.
             */
            this->update_handler->create_work_dir();
            /* try to switch to next fw. slot  */
            this->update_handler->rollback_firmware();
            /* create rollbackUpdate to apply update */
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ofstream rollback((work_dir / "rollbackUpdate"));
            rollback.close();
            cout << "Switch firmware slot successful" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cerr << "Rollback firmware progress error: " << e.what()  << " errno : " << e.errorno << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cerr << "Rollback firmware update error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cerr << "Rollback firmware update system error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::switch_application_slot()
{
    try
    {
        /* get last update reboot state */
        const update_definitions::UBootBootstateFlags update_reboot_state =
            this->update_handler->get_update_reboot_state();
        if (update_reboot_state != update_definitions::UBootBootstateFlags::NO_UPDATE_REBOOT_PENDING)
        {
            cout << "Switch firmware slot is not allowed because update reboot state is wrong." << endl;
            /* return_code would be setted by the function */
            this->print_update_reboot_state();
        }
        else
        {
            cout << "Start switch application slot" << endl;
            /* get last update reboot state */
            const update_definitions::UBootBootstateFlags update_reboot_state =
                this->update_handler->get_update_reboot_state();
            /* Create directory if not exists.
             * GenericException is possible if directory does not exists and
             * can not be created.
             */
            this->update_handler->create_work_dir();
            /* try to switch to next app. slot  */
            this->update_handler->rollback_application();
            /* create rollbackUpdate to apply update */
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ofstream rollback((work_dir / "rollbackUpdate"));
            rollback.close();
            cout << "Switch application slot successful" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SUCCESSFUL);
        }
    }
    catch (const fs::GenericException &e)
    {
        cerr << "Rollback application progress error: " << e.what() << " errno : " << e.errorno << endl;

        switch (e.errorno)
        {
        case EPERM:
            /* application update is not commited */
            this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::UPDATE_STATE_BAD);
            break;
        case ECANCELED:
            /* application update is not commited */
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
            break;
        default:
            this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_PROGRESS_ERROR);
        }
    }
    catch (const fs::BaseFSUpdateException &e)
    {
        cerr << "Rollback application update error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_INTERNAL_ERROR);
    }
    catch (const std::exception &e)
    {
        cerr << "Rollback application update system error: " << e.what() << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_ROLLBACK_STATE::UPDATE_ROLLBACK_SYSTEM_ERROR);
    }
}

void cli::fs_update_cli::print_update_reboot_state()
{
    const update_definitions::UBootBootstateFlags update_reboot_state = this->update_handler->get_update_reboot_state();

    if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE)
    {
        cout << "Application update failed" << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
    {
        cout << "Firmware update failed" << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
    {
        cout << "Firmware reboot update failed" << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
    {
        if (this->update_handler->is_reboot_complete(true))
        {
            cout << "Incomplete firmware update. Commit required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
        }
        else
        {
            cout << "Missing reboot after firmware update requested" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
    {
        if (this->update_handler->is_reboot_complete(false))
        {
            cout << "Incomplete application update. Commit required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
        }
        else
        {
            cout << "Missing reboot after application update requested" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
    {
        if (this->update_handler->is_reboot_complete(true))
        {
            cout << "Incomplete application and firmware update. Commit required." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
        }
        else
        {
            cout << "Missing reboot after application and firmware update" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
    {
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cout << "Missing reboot after firmware rollback requested" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING);
        }
        else
        {
            cout << "Incomplete firwmware rollback. Commit requested." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
    {
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cout << "Missing reboot after application rollback requested" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
        }
        else
        {
            cout << "Incomplete application rollback. Commit requested." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_FW_REBOOT_PENDING)
    {
        if (this->update_handler->pendingUpdateRollback() == false)
        {
            cout << "Missing reboot after firmware and application rollback requested" << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_FW_REBOOT_PENDING);
        }
        else
        {
            cout << "Incomplete firmware and application rollback. Commit requested." << endl;
            this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK);
        }
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_ROLLBACK)
    {
        cout << "Incomplete firwmware rollback. Commit requested." << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_ROLLBACK)
    {
        cout << "Incomplete application rollback. Commit requested." << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_ROLLBACK)
    {
        cout << "Incomplete firmware and application rollback. Commit requested." << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_ROLLBACK);
    }
    else
    {
        cout << "No update pending" << endl;
        this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING);
    }
}

void cli::fs_update_cli::print_current_application_version()
{
    cout << this->update_handler->get_application_version() << endl;
}

void cli::fs_update_cli::print_current_firmware_version()
{
    cout << this->update_handler->get_firmware_version() << endl;
}

void cli::fs_update_cli::set_application_state_bad(const char &state)
{
    this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFULL);
    if (this->update_handler->set_update_state_bad(state, APPLICATION_UPDATE_STATE) == EINVAL)
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_application_state_bad(const char &state)
{
    if (state != 'a' && state != 'A' && state != 'b' && state != 'B')
    {
        cout << "X" << endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
    }
    else
    {
        cout << this->update_handler->is_update_state_bad(state, APPLICATION_UPDATE_STATE) << endl;
    }
}

void cli::fs_update_cli::set_firmware_state_bad(const char &state)
{
    this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFULL);
    if (this->update_handler->set_update_state_bad(state, FIRMWARE_UPDATE_STATE) == EINVAL)
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_firmware_state_bad(const char &state)
{
    if (state != 'a' && state != 'A' && state != 'b' && state != 'B')
    {
        cout << "X" << endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
    }
    else
    {
        cout << this->update_handler->is_update_state_bad(state, FIRMWARE_UPDATE_STATE) << endl;
    }
}

void cli::fs_update_cli::parse_input(int argc, const char **argv)
{
    this->cmd.parse(argc, argv);

    if (this->arg_debug.isSet())
    {
        if (this->arg_automatic.isSet() == false)
        {
            this->logger_sink = std::make_shared<logger::LoggerSinkStdout>(logger::logLevel::DEBUG);
        }
        else
        {
            this->serial_cout = std::make_shared<SynchronizedSerial>();
            this->logger_sink = std::make_unique<logger::LoggerSinkSerial>(logger::logLevel::DEBUG, serial_cout);
        }
    }
    else
    {
        if (this->arg_automatic.isSet() == false)
        {
            this->logger_sink = std::make_shared<logger::LoggerSinkStdout>(logger::logLevel::WARNING);
        }
        else
        {
            this->serial_cout = std::make_shared<SynchronizedSerial>();
            this->logger_sink = std::make_unique<logger::LoggerSinkSerial>(logger::logLevel::WARNING, serial_cout);
        }
    }

    this->logger_handler = logger::LoggerHandler::initLogger(this->logger_sink);
    this->update_handler = std::make_unique<fs::FSUpdate>(logger_handler);

    // Manual mode
    if ((this->arg_update.isSet() == true) && (this->arg_rollback_update.isSet() == false) &&
        (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
        (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
        (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
        (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
        (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
        (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
        (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false))
    {
        // update firmware, application or both
        // use path from argument
        // check paramter
        std::filesystem::path update_location = this->arg_update.getValue();

        if (std::filesystem::exists(update_location) == false)
        {
            cerr << "Update file: " << update_location << "does not exists." << endl;
            this->return_code = EPERM;
            return;
        }
        this->update_image_state("", "", true);
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == true) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_progress.isSet() == false) && (this->download_update.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        // commit update
        this->commit_update();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == true) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        // print update reboot state
        this->print_update_reboot_state();
    }
    // Automatic mode
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == true) &&
             (this->get_app_version.isSet() == false) && (this->get_fw_version.isSet() == false) &&
             (this->notice_update_available.isSet() == false) && (this->download_update.isSet() == false) &&
             (this->download_progress.isSet() == false) && (this->install_update.isSet() == false) &&
             (this->apply_update.isSet() == false) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == false))
    {
        const char *update_stick_env = std::getenv("UPDATE_STICK");
        const char *update_file_env = std::getenv("UPDATE_FILE");

        if (update_stick_env == nullptr)
        {
            stringstream out;
            out << "Environment variable \"UPDATE_STICK\" is no set" << endl;
            this->serial_cout->write(out.str());
            this->return_code = EPERM;
            return;
        }
        const string update_stick(update_stick_env);
        if (update_file_env != nullptr)
        {
            this->update_image_state(update_file_env, update_stick, false);
        }
        else
        {
            stringstream out;
            out << "Not correct UPDATE_FILE system variables set" << endl;
            if (update_file_env == nullptr)
            {
                out << "\"UPDATE_FILE\" env variable -- not set" << endl;
            }
            this->serial_cout->write(out.str());
            this->return_code = EPERM;
            return;
        }
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == true) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        this->print_current_application_version();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == true) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        std::filesystem::path work_dir = this->update_handler->get_work_dir();
        ifstream update_type_stream(work_dir / "update_type");
        if (!update_type_stream.is_open())
        {
            cout << "No updates have been found" << endl;
            this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
            return;
        }

        ifstream update_version_stream(work_dir / "update_version");
        if (!update_version_stream.is_open())
        {
            update_type_stream.close();
            cout << "No updates have been found" << endl;
            this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
            return;
        }

        ifstream update_size_stream(work_dir / "update_size");
        if (!update_size_stream.is_open())
        {
            update_type_stream.close();
            update_version_stream.close();
            cout << "No updates have been found" << endl;
            this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
            return;
        }
        /* read update type */
        std::filebuf *pbuf = update_type_stream.rdbuf();
        std::size_t buf_size = pbuf->pubseekoff(0, update_type_stream.end, update_type_stream.in);
        pbuf->pubseekpos(0, update_type_stream.in);
        char *pchUpdateType = new char[buf_size + 1];
        pbuf->sgetn(pchUpdateType, buf_size);
        update_type_stream.close();
        pchUpdateType[buf_size] = '\0';

        /* read update version */
        pbuf = update_version_stream.rdbuf();
        buf_size = pbuf->pubseekoff(0, update_version_stream.end, update_version_stream.in);
        pbuf->pubseekpos(0, update_version_stream.in);
        char *pchUpdateVersion = new char[buf_size + 1];
        pbuf->sgetn(pchUpdateVersion, buf_size);
        update_version_stream.close();
        pchUpdateVersion[buf_size] = '\0';

        /* read update size */
        pbuf = update_size_stream.rdbuf();
        buf_size = pbuf->pubseekoff(0, update_size_stream.end, update_size_stream.in);
        pbuf->pubseekpos(0, update_size_stream.in);
        char *pchUpdateSize = new char[buf_size + 1];
        pbuf->sgetn(pchUpdateSize, buf_size);
        update_size_stream.close();
        pchUpdateSize[buf_size] = '\0';

        cout << "An new update is available on the server" << endl;
        cout << "Type: " << pchUpdateType << endl;
        cout << "Version: " << pchUpdateVersion << endl;
        cout << "Size: " << pchUpdateSize << endl;

        delete[] pchUpdateVersion;
        delete[] pchUpdateSize;

        if (strcmp("firmware", pchUpdateType) == 0)
        {
            this->proceeded_update_type = UPDATE_FIRMWARE;
            this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
        }
        else if (strcmp("application", pchUpdateType) == 0)
        {
            this->proceeded_update_type = UPDATE_APPLICATION;
            this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE);
        }
        else
        {
            this->proceeded_update_type = UPDATE_COMMON;
            this->return_code =
                static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE);
        }

        delete[] pchUpdateType;
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == true) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        std::filesystem::path work_dir = this->update_handler->get_work_dir();
        if (std::filesystem::exists(work_dir / "update_type") == false ||
            std::filesystem::exists(work_dir / "update_version") == false ||
            std::filesystem::exists(work_dir / "update_size") == false)
        {
            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
        }
        else if (std::filesystem::exists(work_dir / "downloadUpdate") == true)
        {
            cout << "Download in progress..." << endl;
            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE);
        }
        else
        {
            ofstream download_update_stream(work_dir / "downloadUpdate");
            if (!download_update_stream.is_open())
            {
                cout << "Could not initiate update download..." << endl;
                this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED);
            }
            else
            {
                download_update_stream.close();
                cout << "Download started..." << endl;
                this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED);
            }
        }
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == true) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        std::filesystem::path work_dir = this->update_handler->get_work_dir();
        /* check if file updateInstalled available */
        ifstream installed_state(work_dir / "updateInstalled");
        if (installed_state)
        {
            cout << "Update installation finished." << endl;
            this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED);
        }
        else
        {
            installed_state.close();

            if (std::filesystem::exists(work_dir / "update_location") == false)
            {
                this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
            }
            else if (std::filesystem::exists(work_dir / "installUpdate") == true)
            {
                cout << "Update installation in progress." << endl;
                this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
            }
            else
            {
                /* Create file installUpdate to signal fsupdate handler
                *  to start installation.
                */
                ofstream install_update_stream(work_dir / "installUpdate");
                if (!install_update_stream.is_open())
                {
                    cout << "Could not initiate Installation..." << endl;
                    this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
                }
                else
                {
                    install_update_stream.close();
                    cout << "Update installation started." << endl;
                    this->return_code = static_cast<int>(UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_IN_PROGRESS);
                }
            }
        }
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == true) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        /* The updateInstalled file reflects successful installation state.
         */
        std::filesystem::path work_dir = this->update_handler->get_work_dir();
        ifstream installed_state(work_dir / "updateInstalled");
        /* check if file updateInstalled available
         *  yes: update installed successful
         *  no: update installation fails
         */
        if (installed_state.is_open())
        {
            /* close the updateInstalled file */
            installed_state.close();
            /* Check if it is local or network installation.
             * In case of local installation the files applyUpdate
             * and downloadUpdate are not available.
             */
            if (std::filesystem::exists(work_dir / "applyUpdate") == false &&
                std::filesystem::exists(work_dir / "downloadUpdate") == false)
            {
                cout << "Apply update..." << endl;
                /* Local installation process. Force reboot immediately.
                 */
                if(this->reboot() != 0) {
                    /* reboot command fails*/
                    cerr << "Failed to reboot system: " << strerror(errno) << endl;
                    this->return_code = errno;
                } else {
                    this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
                }
            }
            else
            {
                /* Network installation process.
                 *  Create applyUpdate because ADU agent is waiting
                 *  in apply state.
                 */
                ofstream apply_update_stream(work_dir / "applyUpdate");
                if (!apply_update_stream.is_open())
                {
                    cout << "Initiate of update apply fails..." << endl;
                    this->return_code = errno;
                }
                else
                {
                    apply_update_stream.close();
                    cout << "Apply update..." << endl;
                    this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
                }
            }
        }
        else
        {
            /* In case of rollback /tmp/adu/.work/rollbackUpdate
             * should created.
             */
            std::filesystem::path work_dir = this->update_handler->get_work_dir();
            ifstream rollback_state(work_dir / "rollbackUpdate");
            if (rollback_state.is_open())
            {
                /* close the rollbackUpdate file */
                rollback_state.close();
                /* rollback in progress */

                /* set update rollback state from reboot to incomplete */
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

                cout << "Apply rollback update..." << endl;

                /* Local installation process. Force reboot immediately. */
                if(this->reboot() != 0) {
                    /* reboot command fails*/
                    cerr << "Failed to reboot system: " << strerror(errno) << endl;
                    this->return_code = errno;
                } else {
                    this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
                }

                if (this->return_code != static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL))
                {
                    /* restore old state */
                    this->update_handler->update_reboot_state(update_reboot_state);
                }
            }
            else
            {
                cout << "Nothing to apply..." << endl;
                this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
            }
        }
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == true) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        std::filesystem::path work_dir = this->update_handler->get_work_dir();
        // check if any download was requested
        if (std::filesystem::exists(work_dir / "downloadUpdate") == true)
        {
            uint64_t update_size = 0;
            /* First check if update size available */
            /* get update size */
            ifstream update_size_stream(work_dir / "update_size");
            if (!update_size_stream.is_open())
            {
                cout << "Update size not available: " << errno << endl;
                this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
                return;
            }
            /* get file size */
            update_size_stream >> update_size;
            update_size_stream.close();
            /* check for zero */
            if (update_size == 0)
            {
                this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
            }
            else
            {
                int tryAgain = 5;
                std::filesystem::path update_location = work_dir / "update_location";
                size_t filesize = 0;
                /* Wait for 5 seconds because adu agent add path
                 * path for update location.
                 * TODO: Waiting for 5 seconds may be overworked...
                 */
                while (tryAgain--)
                {
                    /* First check for update location because it can be
                     * empty.*/
                    if (!std::filesystem::exists(update_location))
                    {
                        sleep(1);
                        continue;
                    }
                    filesize = std::filesystem::file_size(update_location);
                    /* smallest size 9 chars file update.fs */
                    if (filesize > 9)
                    {
                        break;
                    }
                    sleep(1);
                }

                if (tryAgain == 0)
                {
                    cout << "Waiting to start download." << endl;
                    this->return_code =
                        static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START);
                }
                else
                {
                    std::filesystem::path update_file_path;
                    ifstream path_file_stream(update_location);
                    /* get path to update file */
                    path_file_stream >> update_file_path;
                    path_file_stream.close();
                    /* Is update file exits */
                    if (!std::filesystem::exists(update_file_path))
                    {
                        cerr << "Update file: " << update_file_path << "does not exists." << endl;
                        this->return_code =
                            static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_WAITING_TO_START);
                    }
                    else
                    {
                        /* get current update file size */
                        filesize = std::filesystem::file_size(update_file_path);
                        cout << "Size of loaded update: " << filesize << "..." << endl;

                        if (filesize == 0)
                        {
                            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
                        }
                        else
                        {
                            float is_f = (float)filesize;
                            float should_f = (float)update_size;
                            float ratio = is_f / should_f;
                            int percent = (int)(ratio * 100);

                            cout << filesize << "/";
                            cout << update_size << " -- ";
                            cout << percent << "\%" << endl;

                            if (percent < 100)
                            {
                                this->return_code =
                                    static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_IN_PROGRESS);
                            }
                            else if (percent == 100)
                            {
                                this->return_code =
                                    static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::UPDATE_DOWNLOAD_FINISHED);
                            }
                        }
                    }
                }
            }
        }
        else
            this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == true) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        this->print_current_firmware_version();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == true) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        this->rollback_update();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == true) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        this->switch_firmware_slot();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == true) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->set_app_state_bad.isSet() == false) && (this->is_app_state_bad.isSet() == false) &&
             (this->set_fw_state_bad.isSet() == false) && (this->is_fw_state_bad.isSet() == false) &&
             (this->arg_update_type.isSet() == false))
    {
        this->switch_application_slot();
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == true) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == false) && (this->arg_update_type.isSet() == false))
    {
        cout << "F&S Update Framework CLI Version: " << VERSION;
        cout << " build at: " << __DATE__ << ", " << __TIME__ << "." << endl;
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == false) && (this->set_app_state_bad.isSet() == true) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == false) && (this->arg_update_type.isSet() == false))
    {
        this->set_application_state_bad(this->set_app_state_bad.getValue());
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == false) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == true) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == false) && (this->arg_update_type.isSet() == false))
    {
        this->is_application_state_bad(this->is_app_state_bad.getValue());
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == false) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == true) &&
             (this->is_fw_state_bad.isSet() == false) && (this->arg_update_type.isSet() == false))
    {
        this->set_firmware_state_bad(this->set_fw_state_bad.getValue());
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == false) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == true) && (this->arg_update_type.isSet() == false))
    {
        this->is_firmware_state_bad(this->is_fw_state_bad.getValue());
    }
    else if ((this->arg_update.isSet() == false) && (this->arg_rollback_update.isSet() == false) &&
             (this->arg_switch_fw_slot.isSet() == false) && (this->arg_switch_app_slot.isSet() == false) &&
             (this->arg_commit_update.isSet() == false) && (this->arg_urs.isSet() == false) &&
             (this->arg_automatic.isSet() == false) && (this->get_app_version.isSet() == false) &&
             (this->get_fw_version.isSet() == false) && (this->notice_update_available.isSet() == false) &&
             (this->download_update.isSet() == false) && (this->download_progress.isSet() == false) &&
             (this->install_update.isSet() == false) && (this->apply_update.isSet() == false) &&
             (this->get_version.isSet() == false) && (this->set_app_state_bad.isSet() == false) &&
             (this->is_app_state_bad.isSet() == false) && (this->set_fw_state_bad.isSet() == false) &&
             (this->is_fw_state_bad.isSet() == false) && (this->arg_update_type.isSet() == false))
    {
        cout << "F&S Update Framework CLI Version: " << VERSION;
        cout << " build at: " << __DATE__ << ", " << __TIME__ << "." << endl;
        cout << "No argument given, nothing done. Use --help to get all commands." << endl;
    }
    else
    {
        cerr << "Wrong combination or set of variables. Please refer --help or manual" << endl;
    }
}

int cli::fs_update_cli::getReturnCode() const
{
    return this->return_code;
}

int cli::fs_update_cli::reboot() const
{
    /* TODO: Check if direct reboot without shell call
     * would be better. Steps:
     *  - sync filesystem buffers
     *  - reboot per message RB_AUTOBOOT
     */
#if 0 // TODO:
    sync();
    return ::reboot(RB_AUTOBOOT);
#endif
    return ::system("/sbin/reboot --reboot --no-wall");
}
