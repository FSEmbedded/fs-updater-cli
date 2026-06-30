#pragma once
#include <tclap/CmdLine.h>
#include <fs_update_framework/handle_update/fsupdate.h>

#include <fs_update_framework/logger/LoggerHandler.h>
#include <fs_update_framework/logger/LoggerSinkStdout.h>
#include <fs_update_framework/logger/LoggerSinkEmpty.h>

#include "SynchronizedSerial.h"
#include "../logger/LoggerSinkSerial.h"
#include "cli_classify.h"

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
		TCLAP::CmdLine cmd;
		TCLAP::ValueArg<std::string> arg_update;
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
		TCLAP::SwitchArg arg_help;
		TCLAP::SwitchArg notice_update_available;
		TCLAP::SwitchArg apply_update;
		TCLAP::SwitchArg download_progress;
		TCLAP::SwitchArg download_update;
		TCLAP::SwitchArg install_update;
		/* Unified install (v2): --install_update [<path>]. With a path the
		 * positional install_path selects a local install via the service
		 * (InstallLocal, blocking); without it, --install_update advances an
		 * ADU-staged download (StartInstall). install_path is an optional
		 * global positional — the bare-path rule (parse_input) rejects it unless
		 * --install_update is also set. */
		TCLAP::UnlabeledValueArg<std::string> install_path;
		/* Async install opt-in: kick InstallLocal and return immediately
		 * with the session_id instead of blocking on the terminal verdict. */
		TCLAP::SwitchArg arg_detach;
		/* Serial-console log sink modifier (orthogonal, like --debug). */
		TCLAP::SwitchArg arg_serial;
		/* Unified-install entry point — local flow. The path is required;
		 * the cloud-flow entry point uses --install_update, matching the
		 * existing one-flag-per-trigger pattern. */
		TCLAP::ValueArg<std::string> update_install;
		/* One-shot query of InstallProgress / InstallState, mirrors
		 * --download_progress in shape and exit-code semantics. */
		TCLAP::SwitchArg install_progress;
		/* Request best-effort cancel of the install for a given
		 * session_id. The actual outcome is observable via
		 * --install_progress and the InstallCompleted signal. Named
		 * with the _arg suffix to disambiguate from the
		 * fus_dbus::cancel_install() free function. */
		TCLAP::ValueArg<uint32_t> cancel_install_arg;
		TCLAP::ValueArg<char> set_app_state_bad;
		TCLAP::ValueArg<char> is_app_state_bad;
		TCLAP::ValueArg<char> set_fw_state_bad;
		TCLAP::ValueArg<char> is_fw_state_bad;

		std::unique_ptr<fs::FSUpdate> update_handler;
		std::shared_ptr<SynchronizedSerial> serial_cout;
		std::shared_ptr<logger::LoggerSinkBase> logger_sink;
		std::shared_ptr<logger::LoggerHandler> logger_handler;

		int return_code;

		/* Install-surface decision resolved by classify() in parse_input;
		 * consumed by handle_install_update. */
		cli::ParseOutcome install_outcome;

		/**
		 * Configure logger sink based on --debug and --automatic flags.
		 */
		void setup_logging();

		/**
		 * Internal function to run update and handle errors as return_value:
		 * @param update_file Path to update package (fully resolved)
		 */
		void update_image_state(const std::string &update_file);

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
		void handle_update_file();
		void handle_automatic();
		void handle_print_version();
		void handle_print_help();
		void handle_is_update_available();
		void handle_download_update();
		void handle_download_progress();
		void handle_install_update();
		void handle_update_install();
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
