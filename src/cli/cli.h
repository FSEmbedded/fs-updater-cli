#pragma once
#include <fs_update_framework/handle_update/fsupdate.h>

#include <fs_update_framework/logger/LoggerHandler.h>
#include <fs_update_framework/logger/LoggerSinkStdout.h>
#include <fs_update_framework/logger/LoggerSinkEmpty.h>

#include "SynchronizedSerial.h"
#include "../logger/LoggerSinkSerial.h"
#include "cli_classify.h"
#include "cli_args.h"

#include <string>
#include <stdexcept>
#include <climits>
#include <exception>
#include <vector>
#include <array>

#include "config.h"

/**
 * Namespace contain all CLI related classes of functionality
 */
namespace cli
{
	class fs_update_cli
	{
        private:
		/* The whole argument surface. Separated so it can be exercised without
		 * a board: constructing this class opens the boot environment, so any
		 * test of parsing or dispatch selection would need hardware otherwise. */
		cli::CliArgs args;

		std::unique_ptr<fs::FSUpdate> update_handler;
		std::shared_ptr<SynchronizedSerial> serial_cout;
		std::shared_ptr<logger::LoggerSinkBase> logger_sink;
		std::shared_ptr<logger::LoggerHandler> logger_handler;

		int return_code;

		/* Install-surface decision resolved by classify() in parse_input;
		 * consumed by handle_install_update. */
		cli::ParseOutcome install_outcome;

		/**
		 * Configure logger sink based on --debug and --serial flags.
		 */
		void setup_logging();

		/**
		 * Create rollback marker file in work directory.
		 * @return true on success, false on failure
		 */
		bool create_rollback_marker();

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

		/* Command handlers dispatched from parse_input */
		void handle_print_version();
		void handle_print_help();
		void handle_is_update_available();
		void handle_download_update();
		void handle_download_progress();
		void handle_install_update();
		void handle_install_progress();
		void handle_cancel_install();
		void handle_apply_update();
		void handle_set_app_state_bad();
		void handle_is_app_state_bad();
		void handle_set_fw_state_bad();
		void handle_is_fw_state_bad();

		/**
		 * Parse input and run as described in commands.
		 * @param argc Number of arguments.
		 * @param argv List of all commands
		 * @throw ErrorNotSystemVariable
		 */
		void parse_input(int argc, const char ** argv);

		/* Run the action the parse layer selected. */
		void dispatch(cli::Command command);
		int reboot() const;

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
