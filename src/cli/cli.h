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
/* This define enables old update process */
//#define USE_OLD_UPDATE_TYPE

	typedef enum UpdateTypeT {
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
#ifdef USE_OLD_UPDATE_TYPE
		TCLAP::ValueArg<std::string> arg_app;
		TCLAP::ValueArg<std::string> arg_fw;
#endif
		TCLAP::ValueArg<std::string> arg_update;
		TCLAP::SwitchArg arg_rollback_fw;
		TCLAP::SwitchArg arg_rollback_app;
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
#ifdef USE_OLD_UPDATE_TYPE
		/**
		 * Internal function to run update and handle errors as return_value:
		 * Firmware update progress error: 3
		 * Firmware update error: 2
		 * Firmware update system error: 1
		 */
		void update_firmware_state();

		/**
		 * Internal function to run update and handle errors as return_value:
		 * Application update progress error: 3
		 * Application update error: 2
		 * Application update system error: 1
		 */
		void update_application_state();

		/**
		 * Internal function to run update and handle errors as return_value:
		 * Application & firmware update progress error: 3
		 * Application & firmware update error: 2
		 * Application & firmware update system error: 1
		 */
		void update_firmware_application_state();
#endif

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
		 * Internal function which will test against the current version of firmware and run update.
		 * Print state and errors to serial output.
		 * @param firmware_file_env Zero terminated char array of firmware update package
		 * @param fw_version_env Zero terminated char array of firmware version
		 * @param update_stick Path to update stick directory
		 * @throw std::overflow_error Firmware version string is a number which is bigger than the maximum allowed size.
		 * @throw std::logic_error Is thrown when a commit must set but nothing has been done. That's incorrect in a logical view.
		 */
		void automatic_update_firmware_state(const char * firmware_file_env,
						     const char * fw_version_env,
						     const std::string &update_stick);

		/**
		 * Internal function which will test against the current version of application and run update.
		 * Print state and errors to serial output.
		 * @param application_file_env Zero terminated char array of application update package
		 * @param app_version_env Zero terminated char array of of application version
		 * @param update_stick Path to update stick directory
		 * @throw std::overflow_error Application version string is a number which is bigger than the maximum allowed size.
		 * @throw std::logic_error Is thrown when a commit must set but nothing has been done. That's incorrect in a logical view.
		 */
		void automatic_update_application_state(const char *application_file_env,
							const char *app_version_env,
							std::string update_stick);

		/**
		 * Internal function which will test against the current version of application & firmware and run update.
		 * Print state and errors to serial output.
		 * @param application_file_env Zero terminated char array of application update package
		 * @param firmware_file_env Zero terminated char array of firmware update package
		 * @param fw_version_env Zero terminated char array of firmware version
		 * @param app_version_env Zero terminated char array of of application version
		 * @throw std::overflow_error Application or firmware version string is a number which is bigger than the maximum allowed size.
		 * @throw std::logic_error Is thrown when a commit must set but nothing has been done. That's incorrect in a logical view.
		 */
		void automatic_firmware_application_state(const char *application_file_env,
							  const char *firmware_file_env,
							  const char *fw_version_env,
							  const char *app_version_env,
							  const std::string update_stick);
		/**
		 * Helper function to get the state that allow automatic update.
		 */
		bool allow_automatic_update();

		/**
		 * Internal function to commit update and print a string of performed commit or not performed commit.
		 */
		void commit_update();

		/**
		 * Internal function to rollback the firmware of the current installation.
		 */
		void rollback_firmware();

		/**
		 * Internal function to rollback the application of the current installation.
		 */
		void rollback_application();

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
