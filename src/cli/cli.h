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
#include <exception>
#include <vector>
#include <iostream>
#include <fstream>

#define VERSION "1.0.0"

typedef enum UpdateTypeT
{
  UPDATE_FIRMWARE = 0,
  UPDATE_APPLICATION,
  UPDATE_COMMON,
  UPDATE_UNKNOWN = -1,
} update_type_t;

/**
 * Namespace contain all CLI related classes of functionality
 */
namespace cli
{
	class fs_update_cli
	{
        private:
		TCLAP::CmdLine cmd;
		TCLAP::ValueArg<std::string> arg_update;
		TCLAP::ValueArg<std::string> arg_update_type;
		TCLAP::SwitchArg arg_switch_fw_slot;
		TCLAP::SwitchArg arg_switch_app_slot;
		TCLAP::SwitchArg arg_rollback_update;
		TCLAP::SwitchArg arg_commit_update;
		TCLAP::SwitchArg arg_urs;
		TCLAP::SwitchArg arg_automatic;
		TCLAP::SwitchArg arg_debug;
		TCLAP::SwitchArg get_fw_version;
		TCLAP::SwitchArg get_app_version;
		TCLAP::SwitchArg get_version;
		TCLAP::SwitchArg notice_update_available;
		TCLAP::SwitchArg apply_update;
		TCLAP::SwitchArg download_progress;
		TCLAP::SwitchArg download_update;
		TCLAP::SwitchArg install_update;
		TCLAP::ValueArg<char> set_app_state_bad;
		TCLAP::ValueArg<char> is_app_state_bad;
		TCLAP::ValueArg<char> set_fw_state_bad;
		TCLAP::ValueArg<char> is_fw_state_bad;

		std::unique_ptr<fs::FSUpdate> update_handler;
		std::shared_ptr<SynchronizedSerial> serial_cout;
		std::shared_ptr<logger::LoggerSinkBase> logger_sink;
		std::shared_ptr<logger::LoggerHandler> logger_handler;

		int return_code;
		update_type_t proceeded_update_type;

		/**
		 * Internal function to run update and handle errors as return_value:
		 * @param update_file_env Zero terminated char array of update package
		 * @param update_stick Path to update stick directory
		 * @param use_arg true - uses arg_update, false - use passing parameter
		 */
		void update_image_state(const char *update_file_env, const std::string &update_stick, bool use_arg);

		/**
		 * Internal function which map the update state to a string.
		 */
		void print_update_reboot_state();

		/**
		 * Print current installed firmware version.
		 */
		void print_current_firmware_version();

		/**
		 * Print current installed application version.
		 */
		void print_current_application_version();

		/**
		 * Internal function to commit update and print a string of performed commit or not performed commit.
		 */
		void commit_update();

		/**
		 * Internal function to switch the current slot of the firmware.
		 * E.g. A->B or B->A
		 */
		void switch_firmware_slot();

		/**
		 * Internal function to switch the current slot of the application.
		 * E.g. A->B or B->A
		 */
		void switch_application_slot();

		/**
		 * Internal function to rollback update.
		 */
		void rollback_update();

		/**
		 * Internal function to mark application state bad.
		 * Rollback to this state is not available.
		 * @param state Application state A or B.
		 */
		void set_application_state_bad(const char & state);

		/**
		 * Get application state
		 * @param state Application state A or B.
		 * "X" - wrong parameter
		 * 1 (true) - bad
		 * 0 (false) - not bad
		 */
		void is_application_state_bad(const char & state);

		/**
		 * Internal function to mark firmware state bad.
		 * Rollback to this state is not available.
		 * @param state Firmware state A or B.
		 */
		void set_firmware_state_bad(const char & state);

		/**
		 * Get firmware state
		 * @param state Firmware state A or B.
		 * "X" - wrong parameter
		 * 1 (true) - bad
		 * 0 (false) - not bad
		 */
		void is_firmware_state_bad(const char & state);

		/**
		 * Parse input and run as described in commands.
		 * @param argc Number of arguments.
		 * @param argv List of all commands
		 * @throw ErrorNotSystemVariable
		 */
		void parse_input(int argc, const char ** argv);

        public:
		fs_update_cli(int argc, const char ** argv);
		~fs_update_cli();

		fs_update_cli(const fs_update_cli &) = delete;
		fs_update_cli &operator=(const fs_update_cli &) = delete;
		fs_update_cli(fs_update_cli &&) = delete;
		fs_update_cli &operator=(fs_update_cli &&) = delete;


		/**
		 * Get return code of application.
		 * @return Returncode for application.
		 */
		int getReturnCode() const;
	};
};
