#include "cli.h"
#include <cstdlib>
#include <iostream>
#include "fs_updater_error.h"
#include "fs_updater_types.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define FIRMWARE_UPDATE_STATE 0
#define APPLICATION_UPDATE_STATE 1

/* declaration block for std namespace */
using std::cout;
using std::endl;
using std::string;
using std::cerr;
using std::stringstream;
using std::ifstream;

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		cmd("F&S Update Framework CLI", ' ', VERSION, false),
		arg_update("",
		       "update_file",
		       "path to update package",
		       false,
		       "",
		       "absolute filesystem path"
		       ),
		arg_rollback_fw("",
				"rollback_firmware",
				"run after update your firmware, but before you have commited"
				),
		arg_rollback_app("",
				 "rollback_application",
				 "run after update your application, but before you have commited"
				 ),
		arg_commit_update("",
				  "commit_update",
				  "run after boot and waits for application response"
				  ),
		arg_urs("",
			"update_reboot_state",
			"get state of update"
			),
		arg_automatic("",
			      "automatic",
			      "automatic update modus"
			      ),
		arg_debug("",
			  "debug",
			  "activate debug output"
			  ),
		get_fw_version("",
			       "firmware_version",
			       "print current firmware version"
			       ),
		get_app_version("",
				"application_version",
				"print current application version"
				),
		notice_update_available("",
					"is_update_available",
					"checks if an update is available on the server"
					),
		apply_update("",
			     "apply_update",
			     "do a reboot into the updated partition (do not forget to commit afterwards)"
			     ),
		download_progress("",
				  "download_progress",
				  "show the progress of the current update"
				  ),
		download_update("",
					    "download_update",
					    "download the available update"
					    ),
		install_update("",
					   "install_update",
					   "install downloaded update"
					   ),
		get_version("",
			    "version",
			    "print cli version"
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
				"Check applcition state for bad",
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
	this->cmd.add(arg_rollback_fw);
	this->cmd.add(arg_rollback_app);
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
		cout << "Update started." << endl;
		if(use_arg == true)
		{
			this->update_handler->update_image(
			this->arg_update.getValue());
		}
		else
		{
			string update_file(update_stick);

			if (update_stick.back() != '/')
			{
				update_file += string("/");
			}

			update_file += string(update_file_env);
			this->update_handler->update_image(update_file);
		}

		cout << "Image update successful." << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		cerr << "Image update progress error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::GenericException &e)
	{
		cerr << e.what() << endl;
		this->return_code = static_cast<int>(e.errorno);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		cerr << "Image update error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		cerr << "Image update system error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::automatic_update_firmware_state(const char *firmware_file_env,
							 const char *fw_version_env,
							 const string &update_stick)
{
	string firmware_file(update_stick);

	if (update_stick.back() != '/')
	{
		firmware_file += string("/");
	}

	firmware_file += string(firmware_file_env);

#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(fw_version_env)))
	{
		stringstream error_msg;
		error_msg << "System variable \"FW_VERSION\" is bigger than max of uint64_t: " << std::stoul(fw_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}
	const version_t fw_version = static_cast<uint32_t>(std::stoul(fw_version_env));
#elif UPDATE_VERSION_TYPE_STRING
	const version_t fw_version = fw_version_env;
#endif

	try
	{
		this->update_handler->automatic_update_firmware(firmware_file, fw_version);

		stringstream out;
		out << "Automatic firmware update successful" << endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware update progress error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware update error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware update system error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR);
	}

}

void cli::fs_update_cli::automatic_update_application_state(const char *application_file_env,
							    const char *app_version_env,
							    string update_stick)
{
	string application_file(update_stick);

	if (update_stick.back() != '/')
	{
		application_file += string("/");
	}
	application_file += string(application_file_env);
#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(app_version_env)))
	{
		stringstream error_msg;
		error_msg << "System variable \"APP_VERSION\" is bigger than max of uint64_t: " << std::stoul(app_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}
	const version_t app_version = static_cast<version_t>(std::stoul(app_version_env));
#elif UPDATE_VERSION_TYPE_STRING
	const version_t app_version = app_version_env;
#endif

	try
	{
		this->update_handler->automatic_update_application(application_file, app_version);
		stringstream out;
		out << "Automatic application successful" << endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		stringstream error_msg;
		error_msg << "Automatic application update progress error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		stringstream error_msg;
		error_msg << "Automatic application update error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		stringstream error_msg;
		error_msg << "Automatic application update system error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::automatic_firmware_application_state(const char *application_file_env, 
							      const char *firmware_file_env, 
							      const char *fw_version_env,
							      const char *app_version_env,
							      const string update_stick)
{
	string application_file(update_stick);
	string firmware_file(update_stick);

	if (update_stick.back() != '/')
	{
		application_file += string("/");
		firmware_file += string("/");
	}

	application_file += string(application_file_env);
	firmware_file += string(firmware_file_env);

#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(app_version_env)))
	{
		stringstream error_msg;
		error_msg << "System variable \"APP_VERSION\" is bigger than max of uint64_t: " << std::stoul(app_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}

	if (!((unsigned long) UINT64_MAX > std::stoul(fw_version_env)))
	{
		stringstream error_msg;
		error_msg << "System variable \"FW_VERSION\" is bigger than max of uint64_t: " << std::stoul(fw_version_env);
		throw(std::overflow_error(error_msg.str()));
	}
	const version_t fw_version = static_cast<version_t>(std::stoul(fw_version_env));
	const version_t app_version = static_cast<version_t>(std::stoul(app_version_env));
#elif UPDATE_VERSION_TYPE_STRING
	const version_t fw_version = fw_version_env;
	const version_t app_version = app_version_env;
#endif
	try
	{
		this->update_handler->automatic_update_firmware_and_application(firmware_file, application_file, app_version, fw_version);

		stringstream out;
		out << "Automatic firmware & application successful" << endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware & application update progress error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware & application update error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		stringstream error_msg;
		error_msg << "Automatic firmware & application update system error: " << e.what() << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

bool cli::fs_update_cli::allow_automatic_update()
{
	const update_definitions::UBootBootstateFlags update_reboot_state = this->update_handler->get_update_reboot_state();
	bool retValue = false;

	if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE)
	{
		stringstream error_msg;
		error_msg << "A former application update process failed! \n Re-Plug USB-Drive to start update process" << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_APPLICATION_UPDATE_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
	{
		stringstream error_msg;
		error_msg << "A former firmware update process failed! \n Re-Plug USB-Drive to start update process" << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_FIRMWARE_UPDATE_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
	{
		stringstream error_msg;
		error_msg << "A former firmware reboot into update failed! \n Re-Plug USB-Drive to start update process" << endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_FIRMWARE_REBOOT_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
	{
		stringstream error_msg;
		error_msg << "A former firmware update processed!" << endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
	{
		stringstream error_msg;
		error_msg << "A former application update processed!" << endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
	{
		stringstream error_msg;
		error_msg << "A former application & firmware update processed!" << endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else
	{
		stringstream error_msg;
		error_msg << "No former error or update state detected! \n Update now starts: " << endl;
		this->serial_cout->write(error_msg.str());
		retValue = true;
	}

	return retValue;
}

void cli::fs_update_cli::commit_update()
{
	try
	{
		if (this->update_handler->commit_update() == true)
		{
			cout << "Commit update" << endl;
			this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::SUCCESSFUL);
		}
		else
		{
			cout << "Commit update not needed" << endl;
			this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED);
		}
	}
	catch(const fs::NotAllowedUpdateState &e)
	{
		cerr << "Not allowed update state in UBoot" << endl;
		this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE);
	}
	catch(const std::exception &e)
	{
		cerr << "FS updater cli error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::rollback_firmware()
{
	try
	{
		this->update_handler->rollback_firmware();
		cout << "Rollback firmware successful" << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_SUCCESSFUL);
	}
	catch(const fs::GenericException &e)
	{
		cerr << "Rollback firmware progress error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		cerr << "Rollback firmware update error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		cerr << "Rollback firmware update system error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::rollback_application()
{
	try
	{
		this->update_handler->rollback_application();
		cout << "Rollback application successful" << endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_SUCCESSFUL);
	}
	catch(const fs::GenericException &e)
	{
		cerr << "Rollback application progress error: " << e.what() << endl;

		switch(e.errorno)
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
				this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_PROGRESS_ERROR);
		}
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		cerr << "Rollback application update error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		cerr << "Rollback application update system error: " << e.what() << endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_SYSTEM_ERROR);
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
		if(this->update_handler->is_reboot_complete(true))
		{
			cout << "Incomplete firmware update. Commit required." << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
		} else
		{
			cout << "Missing reboot after firmware update requested" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
	{
		if(this->update_handler->is_reboot_complete(false))
		{
			cout << "Incomplete application update" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
		} else
		{
			cout << "Missing reboot after application update requested" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
	{
		if(this->update_handler->is_reboot_complete(true))
		{
			cout << "Incomplete application and firmware update. Commit required." << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
		} else
		{
			cout << "Missing reboot after application and firmware update" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::UPDATE_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
	{
		if(this->update_handler->is_reboot_complete(true))
		{
			//this->update_handler->update_reboot_state(update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE);
			cout << "Incomplete firwmware rollback. Commit requested." << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
		} else
		{
			cout << "Missing reboot after firmware rollback requested" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
	{
		if(this->update_handler->is_reboot_complete(false))
		{
			//fsthis->update_handler->update_reboot_state(update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE);
			cout << "Incomplete application rollback. Commit requested." << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
		} else
		{
			cout << "Missing reboot after application rollback requested" << endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
		}
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

void cli::fs_update_cli::set_application_state_bad(const char & state)
{
	this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFULL);
	if(this->update_handler->set_update_state_bad(state, APPLICATION_UPDATE_STATE) == EINVAL)
		this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_application_state_bad(const char & state)
{
	if(state != 'a' && state != 'A' && state != 'b' && state != 'B')
	{
		cout << "X" << endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
	}else {
		cout << this->update_handler->is_update_state_bad(state, APPLICATION_UPDATE_STATE) << endl;
	}
}


void cli::fs_update_cli::set_firmware_state_bad(const char & state)
{
	this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::GETSET_STATE_SUCCESSFULL);
	if(this->update_handler->set_update_state_bad(state, FIRMWARE_UPDATE_STATE) == EINVAL)
		this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
}

void cli::fs_update_cli::is_firmware_state_bad(const char & state)
{
	if(state != 'a' && state != 'A' && state != 'b' && state != 'B')
	{
		cout << "X" << endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
	}else {
		cout << this->update_handler->is_update_state_bad(state, FIRMWARE_UPDATE_STATE) << endl;
	}
}

void cli::fs_update_cli::parse_input(int argc, const char ** argv)
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
	if (
		(this->arg_update.isSet() == true) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		// update firmware, application or both
		// use path from argument
		this->update_image_state("", "", true);
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == true) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		// commit update
		this->commit_update();
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == true) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		// print update reboot state
		this->print_update_reboot_state();
	}
	// Automatic mode
	else if(
        (this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == true) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		const char *update_stick_env = std::getenv("UPDATE_STICK");
		const char *update_file_env = std::getenv("UPDATE_FILE");

		if (update_stick_env == nullptr)
		{
			stringstream out;
			out << "Environment variable \"UPDATE_STICK\" is no set" << endl;
			this->serial_cout->write(out.str());
			this->return_code = 2;
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
			this->return_code = 5;
			return;

		}
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == true) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->print_current_application_version();
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == true) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		ifstream update_type_stream("/tmp/adu/.work/update_type");
		if(!update_type_stream.is_open())
		{
			cout << "No updates have been found" << endl;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
			return;
		}

		ifstream update_version_stream("/tmp/adu/.work/update_version");
		if(!update_version_stream.is_open())
		{
			cout << "No updates have been found" << endl;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
			return;
		}

		ifstream update_size_stream("/tmp/adu/.work/update_size");
		if(!update_size_stream.is_open())
		{
			cout << "No updates have been found" << endl;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
			return;
		}
		/* read update type */
		std::filebuf *pbuf = update_type_stream.rdbuf();
		std::size_t buf_size = pbuf->pubseekoff(0, update_type_stream.end, update_type_stream.in);
		pbuf->pubseekpos (0, update_type_stream.in);
		char* pchUpdateType = new char[buf_size+1];
		pbuf->sgetn(pchUpdateType, buf_size);
		update_type_stream.close();
		pchUpdateType[buf_size] = '\0';

		/* read update version */
		pbuf = update_version_stream.rdbuf();
		buf_size = pbuf->pubseekoff(0, update_version_stream.end, update_version_stream.in);
		pbuf->pubseekpos (0, update_version_stream.in);
		char* pchUpdateVersion = new char[buf_size+1];
		pbuf->sgetn(pchUpdateVersion, buf_size);
		update_version_stream.close();
		pchUpdateVersion[buf_size] = '\0';

		/* read update size */
		pbuf = update_size_stream.rdbuf();
		buf_size = pbuf->pubseekoff(0, update_size_stream.end, update_size_stream.in);
		pbuf->pubseekpos (0, update_size_stream.in);
		char* pchUpdateSize = new char[buf_size+1];
		pbuf->sgetn(pchUpdateSize, buf_size);
		update_size_stream.close();
		pchUpdateSize[buf_size] = '\0';

		cout << "An new update is available on the server" << endl;
		cout << "Type: " << pchUpdateType << endl;
		cout << "Version: " << pchUpdateVersion << endl;
		cout << "Size: " << pchUpdateSize << endl;

		delete[] pchUpdateVersion;
		delete[] pchUpdateSize;

		if(strcmp("firmware", pchUpdateType) == 0)
		{
			this->proceeded_update_type = UPDATE_FIRMWARE;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
		}
		else if(strcmp("application", pchUpdateType) == 0)
		{
			this->proceeded_update_type = UPDATE_APPLICATION;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE);
		}
		else
		{
			this->proceeded_update_type = UPDATE_COMMON;
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE);
		}

		delete[] pchUpdateType;
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == true) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		// TODO::
		if(access("/tmp/adu/.work/update_type", F_OK) != 0 ||
		   access("/tmp/adu/.work/update_version", F_OK) != 0 ||
		   access("/tmp/adu/.work/update_size", F_OK) != 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
		}
		else if(access("/tmp/adu/.work/downloadUpdate", F_OK) == 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED_BEFORE);
		}
		else
		{
			FILE *pFile = fopen("/tmp/adu/.work/downloadUpdate", "a");
			if(pFile == NULL)
			{
				cout << "Could not initiate update download..." << endl;
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_FAILED);
			}
			else
			{
				fclose(pFile);
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED);
			}
		}
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == true) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
    {
      std::ifstream installed_state;
      /* check if file updateInstalled available */
      installed_state.open ("/tmp/adu/.work/updateInstalled");
      if (installed_state)
        {
          this->return_code = static_cast<int> (
              UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FINISHED);
        }
      else
        {
          if (access ("/tmp/adu/.work/update_location", F_OK) < 0)
            {
              this->return_code = static_cast<int> (
                  UPDATER_INSTALL_UPDATE_STATE::NO_INSTALLATION_QUEUED);
            }
          else if (access ("/tmp/adu/.work/installUpdate", F_OK) == 0)
            {
              this->return_code
                  = static_cast<int> (UPDATER_INSTALL_UPDATE_STATE::
                                          UPDATE_INSTALLATION_IN_PROGRESS);
            }
          else
            {
              FILE *pFile = fopen ("/tmp/adu/.work/installUpdate", "a");
              if (pFile == NULL)
                {
                  cout << "Could not initiate Installation..." << endl;
                  this->return_code
                      = static_cast<int> (UPDATER_INSTALL_UPDATE_STATE::
                                              UPDATE_INSTALLATION_FAILED);
                }
              else
                {
				  fclose(pFile);
                  this->return_code
                      = static_cast<int> (UPDATER_INSTALL_UPDATE_STATE::
                                              UPDATE_INSTALLATION_IN_PROGRESS);
                }
            }
        }
    }
    else if (
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == true) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
      {
        /* The content handler have to create updateInstalled file
         * in the workflow state install. This means that overloaded class
         * member install(...) creates the file.
         */
        std::ifstream installed_state ("/tmp/adu/.work/updateInstalled");
        /* check if file updateInstalled available
         *  yes: update installed successful
         *  no: update installation fails
         */
        if (installed_state.is_open ())
          {
            /* close the updateInstalled file */
            installed_state.close ();
            /* Check if it is local or network installation.
             * In case of local installation the files applyUpdate
             * and downloadUpdate are not available.
             */
            if ((access ("/tmp/adu/.work/applyUpdate", F_OK) == -1)
                && (access ("/tmp/adu/.work/downloadUpdate", F_OK) == -1))
              {
                /* Local installation process. Force reboot immediately.
                 */
                const int ret = ::system ("/sbin/reboot --reboot --no-wall");
                /* reboot command can fails*/
                switch (ret)
                  {
                  case -1:
                    cout <<"Create process for command reboot fails. Manual "
                            "reboot required." << endl;
                    this->return_code = errno;
                    break;
                  case -127:
                    cout << "Execute command reboot fails. Manual reboot "
                            "required." << endl;
                    this->return_code = ret;
                    break;
                  default:
                    this->return_code = static_cast<int> (
                        UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
                  }
              }
            else
              {
				/* Network installation process.
				*  Create applyUpdate because ADU agent is waiting
				*  in apply state.
				*/
				FILE *pFile = fopen ("/tmp/adu/.work/applyUpdate", "a");
                if (pFile == NULL)
                  {
                    cout << "Initiate of update apply fails..." << endl;
                    this->return_code = this->return_code = errno;
                  }
                else
                  {
					fclose(pFile);
                    this->return_code = static_cast<int> (
                        UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
                  }
              }
          }
        else
          {
            cout <<"Nothing to apply..." << endl;
            this->return_code
                = static_cast<int> (UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
          }
      }
    else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == true) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
      {
        // check if any download was requested
        if (access ("/tmp/adu/.work/downloadUpdate", F_OK) == 0)
          {
            char *dynamic_buffer;
            long int dynamic_buffer_size, loaded_update_size, update_size;
            int tryAgain = 5;
            FILE *pFile;

            dynamic_buffer_size = loaded_update_size = update_size = 0;
            /* First check if update size available */
            /* get update size */
            pFile = fopen ("/tmp/adu/.work/update_size", "r");
			if(pFile == NULL)
			{
				cout << "Update size not available: " << errno << endl;
				this->return_code = static_cast<int> (
					UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
				return;
			}
            /* go end of file */
            fseek (pFile, 0L, SEEK_END);
            /* get file size */
            dynamic_buffer_size = ftell (pFile);
            dynamic_buffer = static_cast<char *> (
            malloc (sizeof (char) * (dynamic_buffer_size + 1)));
            /* go to file begin */
            fseek (pFile, 0L, SEEK_SET);
            /* read update size as string */
            fread (dynamic_buffer, sizeof (char), dynamic_buffer_size,
                   pFile);
            dynamic_buffer[dynamic_buffer_size] = '\0';
            update_size = atol (dynamic_buffer);
            fclose (pFile);
            /* free allocated memory because not needed. */
            free (dynamic_buffer);

            if (update_size == 0)
            {
                this->return_code = static_cast<int> (
                    UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
            }
            else
              {
                /* Wait for 5 seconds because adu agent add path
                 *  path for update location.
                 * TODO: This should be overworked.
                 */
                while (tryAgain--)
                  {
                    /* First check for update location because it can be
                     * empty.*/
                    pFile = fopen ("/tmp/adu/.work/update_location", "r");
		    if(pFile == NULL)
		    {
		        cout << "Update location errno: " << errno << endl;
			sleep (1);
			continue;
		    }
                    /* go end of file */
                    fseek (pFile, 0L, SEEK_END);
                    /* get file size */
                    dynamic_buffer_size = ftell (pFile);
                    /* smallest size 9 chars file update.fs */
                    if (dynamic_buffer_size == 0 || dynamic_buffer_size < 9)
                      {
                        fclose (pFile);
                        sleep (1);
                      }
                    else
                      break;
                  }

                if (tryAgain == 0)
                  {
                    cout << "Waiting to start download." << endl;
                    this->return_code = static_cast<int> (
                        UPDATER_DOWNLOAD_PROGRESS_STATE::
                            UPDATE_DOWNLOAD_WAITING_TO_START);
                  }
                else
                  {
                    /* add one byte more for end of string character*/
                    dynamic_buffer = static_cast<char *> (
                        malloc (sizeof (char) * (dynamic_buffer_size + 1)));
                    /* go to begin of file */
                    fseek (pFile, 0L, SEEK_SET);
                    /* read update location */
                    fread (dynamic_buffer, sizeof (char), dynamic_buffer_size,
                           pFile);
                    fclose (pFile);
                    /* set end of string */
                    dynamic_buffer[dynamic_buffer_size] = '\0';

                    struct stat is_file;
                    if (stat (dynamic_buffer, &is_file) == 0)
                      {
                        loaded_update_size = is_file.st_size;
                        cout << "Size of loaded update: " << loaded_update_size
                             << "..." << endl;
                      }
                    /* free allocated memory because not needed. */
                    free (dynamic_buffer);

                    if (loaded_update_size == 0)
                      {
                        this->return_code = static_cast<int> (
                            UPDATER_DOWNLOAD_PROGRESS_STATE::
                                NO_DOWNLOAD_STARTED);
                      }
                    else
                      {
                        float is_f = (float)loaded_update_size;
                        float should_f = (float)update_size;
                        float ratio = is_f / should_f;
                        int percent = (int)(ratio * 100);

                        cout << loaded_update_size << "/";
                        cout << update_size << " -- ";
                        cout << percent << "\%" << endl;

                        if (percent < 100)
                          {
                            this->return_code = static_cast<int> (
                                UPDATER_DOWNLOAD_PROGRESS_STATE::
                                    UPDATE_DOWNLOAD_IN_PROGRESS);
                          }
                        else if (percent == 100)
                          {
                            this->return_code = static_cast<int> (
                                UPDATER_DOWNLOAD_PROGRESS_STATE::
                                    UPDATE_DOWNLOAD_FINISHED);
                          }
                      }
                  }
              }
          }
        else
          this->return_code = static_cast<int> (
              UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
      }
    else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == true) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->print_current_firmware_version();
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == true) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false)  &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->rollback_firmware();
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == true) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->rollback_application();
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == true)  &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		cout << "F&S Update Framework CLI Version: " << VERSION << " build at: " <<  __DATE__ << ", " << __TIME__  << "." << endl;
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == false)  &&
		(this->set_app_state_bad.isSet() == true) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->set_application_state_bad(this->set_app_state_bad.getValue());
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == true) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->is_application_state_bad(this->is_app_state_bad.getValue());
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == false)  &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == true) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		this->set_firmware_state_bad(this->set_fw_state_bad.getValue());
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == true)
		)
	{
		this->is_firmware_state_bad(this->is_fw_state_bad.getValue());
	}
	else if(
		(this->arg_update.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->get_version.isSet() == false)  &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		cout << "F&S Update Framework CLI Version: " << VERSION << " build at: " <<  __DATE__ << ", " << __TIME__  << "." << endl;
		cout << "No argument given, nothing done. Use --help to get all commands." << endl;
	}
	else
	{
		cerr << "Wrong combination or set of variables. Pleas refer --help or manual" << endl;
	}
}

int cli::fs_update_cli::getReturnCode() const
{
	return this->return_code;
}
