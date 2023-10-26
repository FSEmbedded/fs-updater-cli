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

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		cmd("F&S Update Framework CLI", ' ', VERSION, false),
#ifdef USE_OLD_UPDATE_TYPE
		arg_app("",
			"application_file",
			"path to application",
			false,
			"",
			"absolute filesystem path"
			),
		arg_fw("",
		       "firmware_file",
		       "path to RAUC update package",
		       false,
		       "",
		       "absolute filesystem path"
		       ),
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
	this->cmd.add(arg_app);
	this->cmd.add(arg_fw);
#endif
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

#ifdef USE_OLD_UPDATE_TYPE
void cli::fs_update_cli::update_firmware_state()
{
	try
	{
		this->update_handler->update_firmware(
			this->arg_fw.getValue()
						      );

		std::cout << "Firmware update successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::UPDATE_SUCCESSFUL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::cerr << "Firmware update progress error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::cerr << "Firmware update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Firmware update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::update_application_state()
{
	try
	{
		this->update_handler->update_application(
			this->arg_app.getValue()
							 );

		std::cout << "Application update successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::UPDATE_SUCCESSFUL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::cerr << "Application update progress error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::cerr << "Application update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Application update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::update_firmware_application_state()
{
	try
	{
		this->update_handler->update_firmware_and_application(
			this->arg_fw.getValue(),
			this->arg_app.getValue()
								      );

		std::cout << "Application & firmware update successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::cerr << "Application & firmware update progress error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::cerr << "Application & firmware update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Application & firmware update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}
#endif

void cli::fs_update_cli::update_image_state(const char *update_file_env, const std::string &update_stick, bool use_arg)
{
	std::cout << __func__ << std::endl;
	try
	{
		if(use_arg == true)
		{
			this->update_handler->update_image(
			this->arg_update.getValue());
		}
		else
		{
			std::string update_file(update_stick);

			if (update_stick.back() != '/')
			{
				update_file += std::string("/");
			}

			update_file += std::string(update_file_env);
			this->update_handler->update_image(update_file);
		}

		std::cout << "Image update successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFUL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::cerr << "Image update progress error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::cerr << "Image update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Image update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::automatic_update_firmware_state(const char *firmware_file_env,
							 const char *fw_version_env,
							 const std::string &update_stick)
{
	std::string firmware_file(update_stick);

	if (update_stick.back() != '/')
	{
		firmware_file += std::string("/");
	}

	firmware_file += std::string(firmware_file_env);

#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(fw_version_env)))
	{
		std::stringstream error_msg;
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

		std::stringstream out;
		out << "Automatic firmware update successful" << std::endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware update progress error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware update error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware update system error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE::UPDATE_SYSTEM_ERROR);
	}

}

void cli::fs_update_cli::automatic_update_application_state(const char *application_file_env,
							    const char *app_version_env,
							    std::string update_stick)
{
	std::string application_file(update_stick);

	if (update_stick.back() != '/')
	{
		application_file += std::string("/");
	}
	application_file += std::string(application_file_env);
#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(app_version_env)))
	{
		std::stringstream error_msg;
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
		std::stringstream out;
		out << "Automatic application successful" << std::endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic application update progress error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic application update error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic application update system error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::automatic_firmware_application_state(const char *application_file_env, 
							      const char *firmware_file_env, 
							      const char *fw_version_env,
							      const char *app_version_env,
							      const std::string update_stick)
{
	std::string application_file(update_stick);
	std::string firmware_file(update_stick);

	if (update_stick.back() != '/')
	{
		application_file += std::string("/");
		firmware_file += std::string("/");
	}

	application_file += std::string(application_file_env);
	firmware_file += std::string(firmware_file_env);

#ifdef UPDATE_VERSION_TYPE_UINT64
	if (!((unsigned long) UINT64_MAX > std::stoul(app_version_env)))
	{
		std::stringstream error_msg;
		error_msg << "System variable \"APP_VERSION\" is bigger than max of uint64_t: " << std::stoul(app_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}

	if (!((unsigned long) UINT64_MAX > std::stoul(fw_version_env)))
	{
		std::stringstream error_msg;
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

		std::stringstream out;
		out << "Automatic firmware & application successful" << std::endl;
		this->serial_cout->write(out.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_SUCCESSFULL);
	}
	catch(const fs::UpdateInProgress &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware & application update progress error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware & application update error: " << e.what() << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE::UPDATE_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::stringstream error_msg;
		error_msg << "Automatic firmware & application update system error: " << e.what() << std::endl;
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
		std::stringstream error_msg;
		error_msg << "A former application update process failed! \n Re-Plug USB-Drive to start update process" << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_APPLICATION_UPDATE_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
	{
		std::stringstream error_msg;
		error_msg << "A former firmware update process failed! \n Re-Plug USB-Drive to start update process" << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_FIRMWARE_UPDATE_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
	{
		std::stringstream error_msg;
		error_msg << "A former firmware reboot into update failed! \n Re-Plug USB-Drive to start update process" << std::endl;
		this->serial_cout->write(error_msg.str());
		this->return_code = static_cast<int>(UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE::FORMER_FIRMWARE_REBOOT_FAILED);
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
	{
		std::stringstream error_msg;
		error_msg << "A former firmware update processed!" << std::endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
	{
		std::stringstream error_msg;
		error_msg << "A former application update processed!" << std::endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
	{
		std::stringstream error_msg;
		error_msg << "A former application & firmware update processed!" << std::endl;
		this->serial_cout->write(error_msg.str());
		if (this->update_handler->commit_update() == false)
		{
			throw(std::logic_error("This state is not allowed not be reached when a commit is necessary"));
		}

		retValue = this->allow_automatic_update();
	}
	else
	{
		std::stringstream error_msg;
		error_msg << "No former error or update state detected! \n Update now starts: " << std::endl;
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
			std::cout << "Commit update" << std::endl;
			this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::SUCCESSFUL);
		}
		else
		{
			std::cout << "Commit update not needed" << std::endl;
			this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_NEEDED);
		}
	}
	catch(const fs::NotAllowedUpdateState &e)
	{
		std::cerr << "Not allowed update state in UBoot" << std::endl;
		this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_NOT_ALLOWED_UBOOT_STATE);
	}
	catch(const std::exception &e)
	{
		std::cerr << "FS updater cli error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_COMMIT_STATE::UPDATE_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::rollback_firmware()
{
	try
	{
		this->update_handler->rollback_firmware();
		std::cout << "Rollback firmware successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_SUCCESSFUL);
	}
	catch(const fs::RollbackFirmware &e)
	{
		std::cerr << "Rollback firmware progress error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_PROGRESS_ERROR);
	}
	catch(const fs::BaseFSUpdateException &e)
	{
		std::cerr << "Rollback firmware update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Rollback firmware update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_FIRMWARE_STATE::ROLLBACK_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::rollback_application()
{
	try
	{
		this->update_handler->rollback_application();
		std::cout << "Rollback application successful" << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_SUCCESSFUL);
	}
	catch(const fs::RollbackApplication &e)
	{
		std::cerr << "Rollback application progress error: " << e.what() << std::endl;

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
		std::cerr << "Rollback application update error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_INTERNAL_ERROR);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Rollback application update system error: " << e.what() << std::endl;
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_SYSTEM_ERROR);
	}
}

void cli::fs_update_cli::print_update_reboot_state()
{
	const update_definitions::UBootBootstateFlags update_reboot_state = this->update_handler->get_update_reboot_state();

	if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE)
	{
		std::cout << "Application update failed" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_APP_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
	{
		std::cout << "Firmware update failed" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FAILED_FW_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
	{
		std::cout << "Firmware reboot update failed" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FW_UPDATE_REBOOT_FAILED);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
	{
		if(this->update_handler->is_reboot_complete(true))
		{
			std::cout << "Incomplete firmware update. Commit required." << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
		} else
		{
			std::cout << "Missing reboot after firmware update requested" << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::FW_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
	{
		if(this->update_handler->is_reboot_complete(false))
		{
			std::cout << "Incomplete application update" << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
		} else
		{
			std::cout << "Missing reboot after application update requested" << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::APP_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
	{
		std::cout << "Incomplete application and firmware update" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
	{
		if(this->update_handler->is_reboot_complete(true))
		{
			//this->update_handler->update_reboot_state(update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE);
			std::cout << "Incomplete firwmware rollback. Commit requested." << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_ROLLBACK);
		} else
		{
			std::cout << "Missing reboot after firmware rollback requested" << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING);
		}
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
	{
		if(this->update_handler->is_reboot_complete(false))
		{
			//fsthis->update_handler->update_reboot_state(update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE);
			std::cout << "Incomplete application rollback. Commit requested." << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_ROLLBACK);
		} else
		{
			std::cout << "Missing reboot after application rollback requested" << std::endl;
			this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
		}
	}
	else
	{
		std::cout << "No update pending" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::NO_UPDATE_REBOOT_PENDING);
	}
}

void cli::fs_update_cli::print_current_application_version()
{
	std::cout << this->update_handler->get_application_version() << std::endl;
}

void cli::fs_update_cli::print_current_firmware_version()
{
	std::cout << this->update_handler->get_firmware_version() << std::endl;
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
		std::cout << "X" << std::endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
	}else {
		std::cout << this->update_handler->is_update_state_bad(state, APPLICATION_UPDATE_STATE) << std::endl;
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
		std::cout << "X" << std::endl;
        this->return_code = static_cast<int>(UPDATER_SETGET_UPDATE_STATE::PASSING_PARAM_UPDATE_STATE_WRONG);
	}else {
		std::cout << this->update_handler->is_update_state_bad(state, FIRMWARE_UPDATE_STATE) << std::endl;
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == true) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
	    )
	{
		this->update_firmware_state();
	}
	else if (
		(this->arg_app.isSet() == true) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		 )
	{
		// update application
		this->update_application_state();
	}
	else if(
		(this->arg_app.isSet() == true) &&
		(this->arg_fw.isSet() == true) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false) &&
		(this->set_app_state_bad.isSet() == false) &&
		(this->is_app_state_bad.isSet() == false) &&
		(this->set_fw_state_bad.isSet() == false) &&
		(this->is_fw_state_bad.isSet() == false)
		)
	{
		// update firmware and application
		this->update_firmware_application_state();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		const char *update_stick_env            = std::getenv("UPDATE_STICK");
		const char *application_file_env        = std::getenv("APPLICATION_FILE");
		const char *firmware_file_env           = std::getenv("FIRMWARE_FILE");
		const char *app_version_env             = std::getenv("APP_VERSION");
		const char *fw_version_env              = std::getenv("FW_VERSION");

		if (update_stick_env == nullptr)
		{
			std::stringstream out;
			out << "Environment variable \"UPDATE_STICK\" is no set" << std::endl;
			this->serial_cout->write(out.str());
			this->return_code = 2;
			return;
		}
		const std::string update_stick(update_stick_env);

		// Automatic application & firmware update
		if ((firmware_file_env != nullptr) && (fw_version_env != nullptr) &&
		    (application_file_env != nullptr) && (app_version_env != nullptr))
		{
			this->automatic_firmware_application_state(application_file_env, firmware_file_env, fw_version_env, app_version_env, update_stick);
		}
		// Automatic firmware update
		else if ((firmware_file_env != nullptr) && (fw_version_env != nullptr))
		{
			this->automatic_update_firmware_state(firmware_file_env, fw_version_env, update_stick);
		}
		// Automatic application update
		else if ((application_file_env != nullptr) && (app_version_env != nullptr))
		{
			this->automatic_update_application_state(application_file_env, app_version_env, update_stick);
		}
		// No correct pair of env variables found
		else
		{
			std::stringstream out;
			out << "Not correct pairs of FIRMWARE_FILE & FW_VERSION and/or APPLICATION_FILE & APP_VERSION system variables set" << std::endl;
			if (firmware_file_env == nullptr)
			{
				out << "\"FIRMWARE_FILE\" env variable -- not set" << std::endl;
			}
			else
			{
				out << "\"FIRMWARE_FILE\" env variable -- set" << std::endl;
			}

			if (application_file_env == nullptr)
			{
				out << "\"APPLICATION_FILE\" env variable -- not set" << std::endl;
			}
			else
			{
				out << "\"APPLICATION_FILE\" env variable -- set" << std::endl;
			}

			if (app_version_env == nullptr)
			{
				out << "\"APP_VERSION\" env variable -- not set" << std::endl;
			}
			else
			{
				out << "\"APP_VERSION\" env variable -- set" << std::endl;
			}

			if (fw_version_env == nullptr)
			{
				out << "\"FW_VERSION\" env variable -- not set" << std::endl;
			}
			else
			{
				out << "\"FW_VERSION\" env variable -- set" << std::endl;
			}
			this->serial_cout->write(out.str());
			this->return_code = 5;
			return;
		}
#else
		const char *update_stick_env = std::getenv("UPDATE_STICK");
		const char *update_file_env = std::getenv("UPDATE_FILE");

		if (update_stick_env == nullptr)
		{
			std::stringstream out;
			out << "Environment variable \"UPDATE_STICK\" is no set" << std::endl;
			this->serial_cout->write(out.str());
			this->return_code = 2;
			return;
		}
		const std::string update_stick(update_stick_env);
		if (update_file_env != nullptr)
		{
			this->update_image_state(update_file_env, update_stick, false);
		}
		else
		{
			std::stringstream out;
			out << "Not correct UPDATE_FILE system variables set" << std::endl;
			if (update_file_env == nullptr)
			{
				out << "\"UPDATE_FILE\" env variable -- not set" << std::endl;
			}
			this->serial_cout->write(out.str());
			this->return_code = 5;
			return;

		}
#endif
	}
	else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
		char version[100];
		char type[100];
		char size[100];
		int readBytes = 0;
		int type_fd, version_fd, size_fd;
		bool update_available;

		//initialize arrays as 0
		for(int i = 0; i < 100; i++)
			version[i] = type[i] = size[i] = 0;

		//check for firmware update
		update_available = true;
		type_fd = open("/tmp/adu/.work/update_type", O_RDONLY);
		version_fd = open("/tmp/adu/.work/update_version", O_RDONLY);
		size_fd = open("/tmp/adu/.work/update_size", O_RDONLY);
		if(type_fd < 0 || version_fd < 0 || size_fd < 0)
			update_available = false;

		if(readBytes = read(type_fd, type, 100) < 0)
			memcpy(type, "unknown\0", 9);
		if(read(version_fd, version, 100) < 0)
			memcpy(version, "unknown\0", 9);
		if(read(size_fd, size, 100) < 0)
			memcpy(size, "unknown\0", 9);

		close(type_fd);
		close(version_fd);
		close(size_fd);

		if(update_available == true)
		{
			printf("An new update is available on the server\n");
			printf("Type: %s\nVersion: %s\nSize: %s\n", type, version, size);
			if(strcmp("firmware", type) == 0)
			{
				this->proceeded_update_type = UPDATE_FIRMWARE;
				this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
			}
			else if(strcmp("application", type) == 0)
			{
				this->proceeded_update_type = UPDATE_APPLICATION;
				this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE);
			}
			else
			{
				this->proceeded_update_type = UPDATE_COMMON;
				this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE);
			}
		} else {
			printf("No updates have been found\n");
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
		}
	}
	else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
			if(fopen("/tmp/adu/.work/downloadUpdate", "a") < 0)
				printf("Could not initiate update download...\n");

			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_UPDATE_STATE::UPDATE_DOWNLOAD_STARTED);
		}
	}
	else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
      /* check if file applicationInstalled available */
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
          else if (fopen ("/tmp/adu/.work/installUpdate", "a") < 0)
            {
              this->return_code = static_cast<int> (
                  UPDATER_INSTALL_UPDATE_STATE::UPDATE_INSTALLATION_FAILED);
              printf ("Could not initiate Installation...\n");
            }
          else
            {
              this->return_code
                  = static_cast<int> (UPDATER_INSTALL_UPDATE_STATE::
                                          UPDATE_INSTALLATION_IN_PROGRESS);
            }
        }
    }
    else if (
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
        std::ifstream installed_state;
        /* check if file updateInstalled available */
        installed_state.open ("/tmp/adu/.work/updateInstalled");
        if (installed_state)
          {
            /* firmware installed successful */
            if (access ("/tmp/adu/.work/applyUpdate", F_OK) < 0)
              {
                if (access ("/tmp/adu/.work/downloadUpdate", F_OK) < 0)
                  {
                    /* Local installation process. Force reboot immediately.
                     */
                    const int ret
                        = ::system ("/sbin/reboot --reboot --no-wall");
                  }
              }
            if (fopen ("/tmp/adu/.work/applyUpdate", "a") < 0)
              {
                printf ("Could not initiate apply...\n");
                this->return_code = this->return_code = static_cast<int> (
                    UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
              }
            else
              {
                this->return_code = static_cast<int> (
                    UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
              }
          }
        else
          {
            printf ("Nothing to apply...\n");
            this->return_code
                = static_cast<int> (UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
          }
      }
    else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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

			dynamic_buffer_size = loaded_update_size = update_size = 0;

            /* get update size */
            FILE *file_descriptor = fopen ("/tmp/adu/.work/update_size", "r");
            /* go end of file */
            fseek (file_descriptor, 0L, SEEK_END);
            /* get file size */
            dynamic_buffer_size = ftell (file_descriptor);
            dynamic_buffer = static_cast<char *> (
                malloc (sizeof (char) * (dynamic_buffer_size)));
            /* go to file begin */
            fseek (file_descriptor, 0L, SEEK_SET);
            /* read update size as string */
            fread (dynamic_buffer, sizeof (char), dynamic_buffer_size,
                   file_descriptor);
            update_size = atol (dynamic_buffer);
            fclose (file_descriptor);
            /* free allocated memory because not needed. */
            free (dynamic_buffer);

            printf ("Update_Size : %ld...\n", update_size);

            file_descriptor = fopen ("/tmp/adu/.work/update_location", "r");
            /* go end of file */
            fseek (file_descriptor, 0L, SEEK_END);
            /* get file size */
            dynamic_buffer_size = ftell (file_descriptor);
            /* add one byte more for end of string character*/
            dynamic_buffer = static_cast<char *> (
                malloc (sizeof (char) * (dynamic_buffer_size + 1)));
            /* go to begin of file */
            fseek (file_descriptor, 0L, SEEK_SET);
            /* read update location */
            fread (dynamic_buffer, sizeof (char), dynamic_buffer_size,
                   file_descriptor);
            fclose (file_descriptor);
            /* set end of string */
            dynamic_buffer[dynamic_buffer_size] = '\0';

            printf ("download_location : %s ...\n", dynamic_buffer);

            struct stat is_file;
            if (stat (dynamic_buffer, &is_file) == 0)
              {
                loaded_update_size = is_file.st_size;
                printf ("Size of loaded update: %ld...\n", loaded_update_size);
              }
            /* free allocated memory because not needed. */
            free (dynamic_buffer);

            if (update_size == 0 || loaded_update_size == 0)
              {
                this->return_code = static_cast<int> (
                    UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
              }
            else
              {
                float is_f = (float)loaded_update_size;
                float should_f = (float)update_size;
                float ratio = is_f / should_f;
                int percent = (int)(ratio * 100);
                printf ("%d/%d -- %d\%\n", loaded_update_size, update_size,
                        percent);

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
        else
          this->return_code = static_cast<int> (
              UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
      }
    else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
		std::cout << "F&S Update Framework CLI Version: " << VERSION << " build at: " <<  __DATE__ << ", " << __TIME__  << "." << std::endl;
	}
	else if(
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
#ifdef USE_OLD_UPDATE_TYPE
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
#endif
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
		std::cout << "F&S Update Framework CLI Version: " << VERSION << " build at: " <<  __DATE__ << ", " << __TIME__  << "." << std::endl;
		std::cout << "No argument given, nothing done. Use --help to get all commands." << std::endl;
	}
	else
	{
		std::cerr << "Wrong combination or set of variables. Pleas refer --help or manual" << std::endl;
	}
}

int cli::fs_update_cli::getReturnCode() const
{
	return this->return_code;
}
