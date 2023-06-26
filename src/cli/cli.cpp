#include "cli.h"
#include <cstdlib>
#include <iostream>
#include "fs_updater_error.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
		cmd("F&S Update Framework CLI", ' ', VERSION, false),
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
		download_firmware_update("",
					 "download_firmware_update",
					 "download the available firmware update"
					 ),
		install_firmware_update("",
					"install_firmware_update",
					"install downloaded firmware update"
					),
		apply_update("",
			     "apply_update",
			     "do a reboot into the updated partition (do not forget to commit afterwards)"
			     ),
		download_progress("",
				  "download_progress",
				  "show the progress of the current update"
				  ),
		download_application_update("",
					    "download_application_update",
					    "download the available application update"
					    ),
		install_application_update("",
					   "install_application_update",
					   "install downloaded application update"
					   ),
		get_version("",
			    "version",
			    "print cli version"
			    ),
		return_code(0)
{
	this->cmd.add(arg_app);
	this->cmd.add(arg_fw);
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
	this->cmd.add(install_application_update);
	this->cmd.add(install_firmware_update);
	this->cmd.add(download_progress);
	this->cmd.add(download_application_update);
	this->cmd.add(download_firmware_update);
	this->cmd.add(notice_update_available);

	this->parse_input(argc, argv);
}

cli::fs_update_cli::~fs_update_cli()
{

}

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

	if (!((unsigned long) UINT32_MAX > std::stoul(fw_version_env)))
	{
		std::stringstream error_msg;
		error_msg << "System variable \"FW_VERSION\" is bigger than max of uint32_t: " << std::stoul(fw_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}
	const uint32_t fw_version = static_cast<uint32_t>(std::stoul(fw_version_env));

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

	if (!((unsigned long) UINT32_MAX > std::stoul(app_version_env)))
	{
		std::stringstream error_msg;
		error_msg << "System variable \"APP_VERSION\" is bigger than max of uint32_t: " << std::stoul(app_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}
	const uint32_t app_version = static_cast<uint32_t>(std::stoul(app_version_env));

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

	if (!((unsigned long) UINT32_MAX > std::stoul(app_version_env)))
	{
		std::stringstream error_msg;
		error_msg << "System variable \"APP_VERSION\" is bigger than max of uint32_t: " << std::stoul(app_version_env);
		this->serial_cout->write(error_msg.str());
		throw(std::overflow_error(error_msg.str()));
	}

	if (!((unsigned long) UINT32_MAX > std::stoul(fw_version_env)))
	{
		std::stringstream error_msg;
		error_msg << "System variable \"FW_VERSION\" is bigger than max of uint32_t: " << std::stoul(fw_version_env);
		throw(std::overflow_error(error_msg.str()));
	}
	const uint32_t fw_version = static_cast<uint32_t>(std::stoul(fw_version_env));
	const uint32_t app_version = static_cast<uint32_t>(std::stoul(app_version_env));

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
		this->return_code = static_cast<int>(UPDATER_APPLICATION_STATE::ROLLBACK_PROGRESS_ERROR);
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
		std::cout << "Incomplete firmware update" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_FW_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
	{
		std::cout << "Incomplete application update" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
	{
		std::cout << "Incomplete application and firmware update" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::INCOMPLETE_APP_FW_UPDATE);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_FW_REBOOT_PENDING)
	{
		std::cout << "Missing reboot after firmware rollback requested" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_FW_REBOOT_PENDING);
	}
	else if (update_reboot_state == update_definitions::UBootBootstateFlags::ROLLBACK_APP_REBOOT_PENDING)
	{
		std::cout << "Missing reboot after application rollback requested" << std::endl;
		this->return_code = static_cast<int>(UPDATER_UPDATE_REBOOT_STATE::ROLLBACK_APP_REBOOT_PENDING);
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
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
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
		(this->apply_update.isSet() == false)
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
		(this->apply_update.isSet() == false)
		)
	{
		// update firmware and application
		this->update_firmware_application_state();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == true) &&
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
		(this->apply_update.isSet() == false)
		)
	{
		// commit update
		this->commit_update();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == true) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		// print update reboot state
		this->print_update_reboot_state();
	}
	// Automatic mode
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == true) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
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
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == true) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		this->print_current_application_version();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == true) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		char version[100];
		char type[100];
		char size[100];
		int readBytes = 0;
		int type_fd, version_fd, size_fd;
		bool fw_available, app_available;

		//initialize arrays as 0
		for(int i = 0; i < 100; i++)
			version[i] = type[i] = size[i] = 0;

		//check for firmware update
		fw_available = true;
		type_fd = open("/tmp/adu/.work/firmware_type", O_RDONLY);
		version_fd = open("/tmp/adu/.work/firmware_version", O_RDONLY);
		size_fd = open("/tmp/adu/.work/firmware_size", O_RDONLY);
		if(type_fd < 0 || version_fd < 0 || size_fd < 0)
			fw_available = false;

		if(readBytes = read(type_fd, type, 100) < 0)
			memcpy(type, "unknown\0", 9);
		if(read(version_fd, version, 100) < 0)
			memcpy(version, "unknown\0", 9);
		if(read(size_fd, size, 100) < 0)
			memcpy(size, "unknown\0", 9);

		close(type_fd);
		close(version_fd);
		close(size_fd);

		if(strcmp("fus/firmware:1", type) == 0)
			strcpy(type, "firmware\0");

		if(fw_available == true)
		{
			printf("An new firmware update is available on the server\n");
			printf("Type: %s\nVersion: %s\nSize: %s\n", type, version, size);
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_UPDATE_AVAILABLE);
		}

		//check for application update
		app_available = true;
		type_fd = open("/tmp/adu/.work/application_type", O_RDONLY);
		version_fd = open("/tmp/adu/.work/application_version", O_RDONLY);
		size_fd = open("/tmp/adu/.work/application_size", O_RDONLY);
		if(type_fd < 0 || version_fd < 0 || size_fd < 0)
			app_available = false;

		if(readBytes = read(type_fd, type, 100) < 0)
			memcpy(type, "unknown\0", 9);
		if(read(version_fd, version, 100) < 0)
			memcpy(version, "unknown\0", 9);
		if(read(size_fd, size, 100) < 0)
			memcpy(size, "unknown\0", 9);

		close(type_fd);
		close(version_fd);
		close(size_fd);

		if(strcmp("fus/application:1", type) == 0)
			strcpy(type, "application\0");

		if(app_available == true)
		{
			printf("An new application update is available on the server\n");
			printf("Type: %s\nVersion: %s\nSize: %s\n", type, version, size);
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::APPLICATION_UPDATE_AVAILABLE);
		}

		//set return codes
		if(fw_available == true && app_available == true)
		{
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE);
		}
		else if(fw_available == true || app_available == true)
		{
			//do nothing return code already set
		}
		else
		{
			printf("No updates have been found\n");
			this->return_code = static_cast<int>(UPDATER_IS_UPDATE_AVAILABLE_STATE::NO_UPDATE_AVAILABLE);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == false) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == true) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		if(access("/tmp/adu/.work/firmware_type", F_OK) != 0 ||
		   access("/tmp/adu/.work/firmware_version", F_OK) != 0 ||
		   access("/tmp/adu/.work/firmware_size", F_OK) != 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_FIRMWARE_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
		}
		else if(access("/tmp/adu/.work/downloadFirmware", F_OK) == 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_FIRMWARE_UPDATE_STATE::FIRMWARE_DOWNLOAD_STARTED_BEFORE);
		}
		else
		{
			if(fopen("/tmp/adu/.work/downloadFirmware", "a") < 0)
				printf("Could not initiate Download...\n");

			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_FIRMWARE_UPDATE_STATE::FIRMWARE_DOWNLOAD_STARTED);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->download_application_update.isSet() == true) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		if(access("/tmp/adu/.work/application_type", F_OK) != 0 ||
		   access("/tmp/adu/.work/application_version", F_OK) != 0 ||
		   access("/tmp/adu/.work/application_size", F_OK) != 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_APPLICATION_UPDATE_STATE::NO_DOWNLOAD_QUEUED);
		}
		else if(access("/tmp/adu/.work/downloadApplication", F_OK) == 0)
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_APPLICATION_UPDATE_STATE::APPLICATION_DOWNLOAD_STARTED_BEFORE);
		}
		else
		{
			if(fopen("/tmp/adu/.work/downloadApplication", "a") < 0)
				printf("Could not initiate Download...\n");

			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_APPLICATION_UPDATE_STATE::APPLICATION_DOWNLOAD_STARTED);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->install_firmware_update.isSet() == true) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		if(access("/tmp/adu/.work/firmware_location", F_OK) < 0)
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_FIRMWARE_UPDATE_STATE::NO_INSTALLATION_QUEUED);
		}
		else if(access("/tmp/adu/.work/installFirmware", F_OK) == 0)
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_FIRMWARE_UPDATE_STATE::FIRMWARE_INSTALLATION_IN_PROGRESS);
		}
		else if(fopen("/tmp/adu/.work/installFirmware", "a") < 0) {
			this->return_code = static_cast<int>(UPDATER_INSTALL_FIRMWARE_UPDATE_STATE::FIRMWARE_INSTALLATION_FAILED);
			printf("Could not initiate Installation...\n");
		}
		else
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_FIRMWARE_UPDATE_STATE::FIRMWARE_INSTALLATION_IN_PROGRESS);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->install_application_update.isSet() == true) &&
		(this->apply_update.isSet() == false)
		)
	{
		if(access("/tmp/adu/.work/application_location", F_OK) < 0)
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_APPLICATION_UPDATE_STATE::NO_INSTALLATION_QUEUED);
		}
		else if(access("/tmp/adu/.work/installApplication", F_OK) == 0)
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_APPLICATION_UPDATE_STATE::APPLICATION_INSTALLATION_IN_PROGRESS);
		}
		else if(fopen("/tmp/adu/.work/installApplication", "a") < 0) {
			this->return_code = static_cast<int>(UPDATER_INSTALL_APPLICATION_UPDATE_STATE::APPLICATION_INSTALLATION_FAILED);
			printf("Could not initiate Installation...\n");
		}
		else
		{
			this->return_code = static_cast<int>(UPDATER_INSTALL_APPLICATION_UPDATE_STATE::APPLICATION_INSTALLATION_IN_PROGRESS);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->apply_update.isSet() == true)
		)
	{
		if(access("/tmp/adu/.work/applyFirmware", F_OK) < 0)
		{
			this->return_code = this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_FAILED);
		}
		if(fopen("/tmp/adu/.work/applyFirmware", "a") < 0)
			printf("Could not initiate apply...\n");
		this->return_code = static_cast<int>(UPDATER_APPLY_UPDATE_STATE::APPLY_SUCCESSFULL);
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->download_progress.isSet() == true) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		int is = 0;
		int should = 0;

		bool fw_dl, app_dl;
		bool fw_done = false;
		bool app_done = false;

		//check if any download was requested
		if(access("/tmp/adu/.work/downloadFirmware", F_OK) == 0 && access("/tmp/adu/.work/downloadApplication", F_OK) == 0)
		{
			fw_dl = app_dl = true;
		}
		else if(access("/tmp/adu/.work/downloadFirmware", F_OK) == 0)
		{
			fw_dl = true;
			app_dl = false;
		}
		else if(access("/tmp/adu/.work/downloadApplication", F_OK) == 0)
		{
			fw_dl = false;
			app_dl = true;
		}
		else
		{
			fw_dl = app_dl = false;
		}

		//get the progress
		if(fw_dl == true)
		{
			int should_file = open("/tmp/adu/.work/firmware_size", O_RDONLY);
			char should_string[100];
			read(should_file, should_string, 100);
			should = atoi(should_string);

			int fd = open("/tmp/adu/.work/firmware_location", O_RDONLY);
			char download_location[100];
			read(fd, download_location, 100);
			close(fd);

			struct stat is_file;
			if(stat(download_location, &is_file) >= 0)
				is = is_file.st_size;

			if(should == 0 || is == 0)
			{
				fw_dl = false;
			}
			else
			{
				float is_f = (float)is;
				float should_f = (float)should;
				float ratio = is_f/should_f;
				int percent = (int)(ratio*100);
				printf("%d/%d -- %d\%\n", is, should, percent);

				if(percent < 100)
				{
					fw_done = false;
				}
				else if(percent == 100)
				{
					fw_done = true;
				}
			}
		}
		if(app_dl == true)
		{
			int should_file = open("/tmp/adu/.work/application_size", O_RDONLY);
			char should_string[100];
			read(should_file, should_string, 100);
			should = atoi(should_string);

			int fd = open("/tmp/adu/.work/application_location", O_RDONLY);
			char download_location[100];
			read(fd, download_location, 100);
			close(fd);

			struct stat is_file;
			if(stat(download_location, &is_file) >= 0)
				is = is_file.st_size;

			if(should == 0 || is == 0){
				app_dl = false;
			}
			else
			{
				float is_f = (float)is;
				float should_f = (float)should;
				float ratio = is_f/should_f;
				int percent = (int)(ratio*100);
				printf("%d/%d -- %d\%\n", is, should, percent);

				if(percent < 100)
				{
					app_done = false;
				}
				else if(percent == 100)
				{
					app_done = true;
				}
			}
		}

		//set return values
		if(fw_dl == true && app_dl == true)
		{
			if(fw_done == true && app_done == true)
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::ALL_REQUESTED_DOWNLOADS_FINISHED);
			}
			else if(fw_done == true)
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::APPLICATION_DOWNLOAD_IN_PROGRESS);
			}
			else if(app_done == true)
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::FIRMWARE_DOWNLOAD_IN_PROGRESS);
			}
			else
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::FIRMWARE_AND_APPLICATION_DOWNLOAD_IN_PROGRESS);
			}
		}
		else if(fw_dl == true)
		{
			if(fw_done == true)
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::ALL_REQUESTED_DOWNLOADS_FINISHED);
			}
			else
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::FIRMWARE_DOWNLOAD_IN_PROGRESS);
			}
		}
		else if(app_dl == true)
		{
			if(app_done == true)
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::ALL_REQUESTED_DOWNLOADS_FINISHED);
			}
			else
			{
				this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::APPLICATION_DOWNLOAD_IN_PROGRESS);
			}
		}
		else
		{
			this->return_code = static_cast<int>(UPDATER_DOWNLOAD_PROGRESS_STATE::NO_DOWNLOAD_STARTED);
		}
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == false) &&
		(this->arg_commit_update.isSet() == false) &&
		(this->arg_urs.isSet() == false) &&
		(this->arg_automatic.isSet() == false) &&
		(this->get_app_version.isSet() == false) &&
		(this->get_fw_version.isSet() == true) &&
		(this->notice_update_available.isSet() == false) &&
		(this->download_firmware_update.isSet() == false) &&
		(this->download_application_update.isSet() == false) &&
		(this->download_progress.isSet() == false) &&
		(this->install_firmware_update.isSet() == false) &&
		(this->install_application_update.isSet() == false) &&
		(this->apply_update.isSet() == false)
		)
	{
		this->print_current_firmware_version();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == true) &&
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
		(this->apply_update.isSet() == false)
		)
	{
		this->rollback_firmware();
	}
	else if(
		(this->arg_app.isSet() == false) &&
		(this->arg_fw.isSet() == false) &&
		(this->arg_rollback_fw.isSet() == false) &&
		(this->arg_rollback_app.isSet() == true) &&
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
		(this->apply_update.isSet() == false)
		)
	{
		this->rollback_application();
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->get_version.isSet() == true)
		)
	{
		std::cout << "F&S Update Framework CLI Version: " << VERSION << " build at: " <<  __DATE__ << ", " << __TIME__  << "." << std::endl;
	}
	else if(
		(this->arg_app.isSet() == false) &&
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
		(this->get_version.isSet() == false)
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
