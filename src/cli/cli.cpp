#include "cli.h"
#include <cstdlib>

cli::fs_update_cli::fs_update_cli(int argc, const char ** argv):
    cmd("F&S Update Framework CLI", ' ', VERSION, true),
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
    return_code(0)
{
    this->cmd.add(arg_app);
    this->cmd.add(arg_fw);
    this->cmd.add(arg_commit_update);
    this->cmd.add(arg_urs);
    this->cmd.add(arg_automatic);
    this->cmd.add(arg_debug);

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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::cerr << "Firmware update progress error: " << e.what() << std::endl;
        this->return_code = 3;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::cerr << "Firmware update error: " << e.what() << std::endl;
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Firmware update system error: " << e.what() << std::endl;
        this->return_code = 1;
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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::cerr << "Application update progress error: " << e.what() << std::endl;
        this->return_code = 3;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::cerr << "Application update error: " << e.what() << std::endl;
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Application update system error: " << e.what() << std::endl;
        this->return_code = 1;
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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::cerr << "Application & firmware update progress error: " << e.what() << std::endl;
        this->return_code = 3;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::cerr << "Application & firmware update error: " << e.what() << std::endl;
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Application & firmware update system error: " << e.what() << std::endl;
        this->return_code = 1;
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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware update progress error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 1;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware update error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware update system error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 3;
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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic application update progress error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 1;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic application update error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic application update system error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 3;
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
    }
    catch(const fs::UpdateInProgress &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware & application update progress error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 1;
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware & application update error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 2;
    }
    catch (const std::exception &e)
    {
        std::stringstream error_msg;
        error_msg << "Automatic firmware & application update system error: " << e.what() << std::endl;
        this->serial_cout->write(error_msg.str());
        this->return_code = 3;
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
        this->return_code = 10;
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
        this->return_code = 11;
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
        this->return_code = 12;
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
            this->return_code = 110;
        }
        else
        {
            std::cout << "Commit update not needed" << std::endl;
            this->return_code = 111;
        }
    }
    catch(const fs::NotAllowedUpdateState &e)
    {
        std::cerr << "Not allowed update state in UBoot" << std::endl;
        this->return_code = 1;
    }  
    catch(const std::exception &e)
    {
        std::cerr << "FS updater cli error: " << e.what() << std::endl;
        this->return_code = 2;
    }
}

void cli::fs_update_cli::print_update_reboot_state()
{
    const update_definitions::UBootBootstateFlags update_reboot_state = this->update_handler->get_update_reboot_state();

    if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE)
    {
        std::cout << "Application update failed" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::FAILED_APP_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE)
    {
        std::cout << "Firmware update failed" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::FAILED_FW_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED)
    {
        std::cout << "Firmware reboot update failed" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::FW_UPDATE_REBOOT_FAILED);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE)
    {
        std::cout << "Incomplete firmware update" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::INCOMPLETE_FW_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE)
    {
        std::cout << "Incomplete application update" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::INCOMPLETE_APP_UPDATE);
    }
    else if (update_reboot_state == update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE)
    {
        std::cout << "Incomplete application and firmware update" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::INCOMPLETE_APP_FW_UPDATE);
    }
    else
    {
        std::cout << "No update pending" << std::endl;
        this->return_code = 100 + int(update_definitions::UBootBootstateFlags::NO_UPDATE_REBOOT_PENDING);
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
        (this->arg_app.isSet() == false) &&
        (this->arg_fw.isSet() == true) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        this->update_firmware_state();
    }
    else if (
        (this->arg_app.isSet() == true) &&
        (this->arg_fw.isSet() == false) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        // update application
        this->update_application_state();
    }
    else if(
        (this->arg_app.isSet() == true) &&
        (this->arg_fw.isSet() == true) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        // update firmware and application
        this->update_firmware_application_state();
    }
    else if(
        (this->arg_app.isSet() == false) &&
        (this->arg_fw.isSet() == false) &&
        (this->arg_commit_update.isSet() == true) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        // commit update
        this->commit_update();
    }
    else if(
        (this->arg_app.isSet() == false) &&
        (this->arg_fw.isSet() == false) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == true) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        // print update reboot state
        this->print_update_reboot_state();
    }
    // Automatic mode
    else if(
        (this->arg_app.isSet() == false) &&
        (this->arg_fw.isSet() == false) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == true)
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

        // Automatic firmware update
        if ((firmware_file_env != nullptr) && (fw_version_env != nullptr) &&
            (application_file_env == nullptr) && (fw_version_env == nullptr))
        {
            if (this->allow_automatic_update())
                this->automatic_update_firmware_state(firmware_file_env, fw_version_env, update_stick);
        }
        // Automatic application update
        else if ((firmware_file_env == nullptr) && (fw_version_env == nullptr) &&
                 (application_file_env != nullptr) && (fw_version_env != nullptr))
        {
            if (this->allow_automatic_update())
                this->automatic_update_application_state(application_file_env, app_version_env, update_stick);
        }
        // Automatic application & firmware update
        else if ((firmware_file_env != nullptr) && (fw_version_env != nullptr) &&
                 (application_file_env != nullptr) && (fw_version_env != nullptr))
        {
            if (this->allow_automatic_update())
                this->automatic_firmware_application_state(application_file_env, firmware_file_env, fw_version_env, app_version_env, update_stick);
        }
                // No correct pair of env variables found
        else
        {
            std::stringstream out;
            out << "Not correct pairs of FIRMWARE_FILE & FW_VERSION and/or APPLICATION_FILE & APP_VERSION" << std::endl;
            this->serial_cout->write(out.str());
            this->return_code = 5;
            return;
        }
    }    
    else if(
        (this->arg_app.isSet() == false) &&
        (this->arg_fw.isSet() == false) &&
        (this->arg_commit_update.isSet() == false) &&
        (this->arg_urs.isSet() == false) &&
        (this->arg_automatic.isSet() == false)
    )
    {
        std::cerr << "No argument given, nothing done. Use --help to get all commands." << std::endl;
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
