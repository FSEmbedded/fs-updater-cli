#pragma once
#include <tclap/CmdLine.h>
#include <fs_update_framework/handle_update/fsupdate.h>

#include <fs_update_framework/logger/LoggerHandler.h>
#include <fs_update_framework/logger/LoggerSinkStdout.h>
#include <fs_update_framework/logger/LoggerSinkEmpty.h>

#include "SynchronizedSerial.h"
#include "../logger/LoggerSinkSerial.h"

#include <string>
#include <stdexcept>
#include <climits>
#include <filesystem>
#include <exception>
#include <vector>
#include <iostream>
#include <fstream>

#define VERSION "0.0.1"

/**
 * Namespace contain all CLI related classes of functionality
 */
namespace cli
{
    class fs_update_cli
    {
        private:
            TCLAP::CmdLine cmd;
            TCLAP::ValueArg<std::string> arg_app;
            TCLAP::ValueArg<std::string> arg_fw;
            TCLAP::SwitchArg arg_commit_update;
            TCLAP::SwitchArg arg_urs;
            TCLAP::SwitchArg arg_automatic;
            TCLAP::SwitchArg arg_debug;
            
            std::unique_ptr<fs::FSUpdate> update_handler;
            std::shared_ptr<SynchronizedSerial> serial_cout;
            std::shared_ptr<logger::LoggerSinkBase> logger_sink;
            std::shared_ptr<logger::LoggerHandler> logger_handler;

            int return_code;

            void update_firmware_state();
            void update_application_state();
            void update_firmware_application_state();
            void get_update_reboot_state();

            void automatic_update_firmware_state(const char * firmware_file_env, const char * fw_version_env, const std::filesystem::path &update_stick);
            void automatic_update_application_state(const char *application_file_env,
                                                    const char *app_version_env,
                                                    std::filesystem::path update_stick);
            void automatic_firmware_application_state(const char *application_file_env, 
                                                      const char *firmware_file_env, 
                                                      const char *fw_version_env,
                                                      const char *app_version_env,
                                                      const std::filesystem::path update_stick);
            void commit_update();


        public:
            fs_update_cli();
            ~fs_update_cli();

            fs_update_cli(const fs_update_cli &) = delete;
            fs_update_cli &operator=(const fs_update_cli &) = delete;
            fs_update_cli(fs_update_cli &&) = delete;
            fs_update_cli &operator=(fs_update_cli &&) = delete;
            /**
             * Parse input and run as described in commands.
             * @param argc Number of arguments.
             * @param argv List of all commands
             * @throw ErrorNotSystemVariable
             */
            void parse_input(int argc, const char ** argv);

            /**
             * Get return code of application.
             * @return Returncode for application.
             */
            int getReturnCode() const; 
    };  
}
