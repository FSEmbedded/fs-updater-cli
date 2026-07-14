#pragma once

#include <tclap/CmdLine.h>
#include <tclap/StdOutput.h>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "cli_classify.h"
#include "non_empty_value_arg.h"
#include "path_value_arg.h"

/* The argument surface, separated from the command implementations.
 *
 * Everything here is decidable from argv alone: which command was asked for,
 * how many action flags competed for it, whether a surface guard rejected the
 * combination, and what the usage text says. None of it needs the updater, so
 * it can be exercised without a board — which the class holding the handlers
 * cannot, since constructing it opens the boot environment.
 *
 * tests/golden/argv_corpus.txt is the contract. */
namespace cli
{
	/* The action flags, in dispatch-table order. Modifiers (--debug, --serial,
	 * --detach) and the install positional are not actions and are absent. */
	enum class Command
	{
		none,
		commit_update,
		update_reboot_state,
		application_version,
		firmware_version,
		print_version,
		is_update_available,
		download_update,
		download_progress,
		install_update,
		install_progress,
		cancel_install,
		apply_update,
		rollback_update,
		switch_fw_slot,
		switch_app_slot,
		set_app_state_bad,
		is_app_state_bad,
		set_fw_state_bad,
		is_fw_state_bad,
	};

	struct ParseResult
	{
		enum class Kind
		{
			ok,           /* command is dispatchable */
			help,         /* usage requested */
			version_only, /* no action flag given */
			parse_error,  /* the parser rejected the input */
			guard_error,  /* the install surface rejected it after a clean parse */
			combo_error,  /* more than one action flag */
		};

		Kind kind = Kind::version_only;
		Command command = Command::none;
		std::size_t action_count = 0;

		/* Install-surface outcome (path/detach/mode), consumed by the install
		 * handler. Carried here because the handler has no other route to it. */
		ParseOutcome install_outcome{};

		/* Exit code for every kind the parse layer settles on its own. Zero for
		 * Kind::ok — there the handler owns the outcome. */
		int rc = 0;
	};

	/**
	 * Renders usage into a string rather than stdout.
	 *
	 * The stock failure() calls usage() virtually, so overriding usage() alone
	 * would silently divert the parse-error usage block away from stdout too.
	 * Both are overridden and the bytes are re-emitted by the caller, which also
	 * takes back the decision to terminate: the stock failure() ends in exit(),
	 * which makes the parser untestable and the exit code unreachable from the
	 * error table.
	 */
	class UsageCapture : public TCLAP::StdOutput
	{
		public:
		void usage(TCLAP::CmdLineInterface &c) override
		{
			/* Byte-for-byte the stock layout — tests/golden/help.txt is taken
			 * from the shipped binary and must keep matching. */
			this->usage_text << std::endl << "USAGE: " << std::endl << std::endl;
			this->_shortUsage(c, this->usage_text);
			this->usage_text << std::endl << std::endl << "Where: " << std::endl << std::endl;
			this->_longUsage(c, this->usage_text);
			this->usage_text << std::endl;
		}

		void failure(TCLAP::CmdLineInterface &c, TCLAP::ArgException &e) override
		{
			this->banner << "PARSE ERROR: " << e.argId() << std::endl
						 << "             " << e.error() << std::endl
						 << std::endl;
			this->usage(c);
			this->failed = true;
			/* Deliberately no ExitException: parse() returns and the caller
			 * turns this into a ParseResult. */
		}

		void reset()
		{
			this->usage_text.str("");
			this->banner.str("");
			this->failed = false;
		}

		std::ostringstream usage_text; /* stdout in production */
		std::ostringstream banner;     /* stderr in production */
		bool failed = false;
	};

	/**
	 * One instance per process — not by choice.
	 *
	 * The argument library tracks "an optional unlabeled argument has been
	 * registered" in a process-global static, so constructing a second instance
	 * throws even after the first is gone. parse() therefore resets the existing
	 * arguments rather than expecting a fresh object, and callers that parse more
	 * than once (tests) must share one instance.
	 *
	 * Production is unaffected: one process, one parse. It is recorded here
	 * because it is invisible at the call site and surprising when hit.
	 */
	class CliArgs
	{
		public:
		CliArgs();

		CliArgs(const CliArgs &) = delete;
		CliArgs &operator=(const CliArgs &) = delete;
		CliArgs(CliArgs &&) = delete;
		CliArgs &operator=(CliArgs &&) = delete;

		[[nodiscard]] ParseResult parse(int argc, const char **argv);

		/* The usage block, identical to what the parser would have printed. */
		[[nodiscard]] std::string usage_text();

		/* Populated only when parse() returned Kind::parse_error. */
		[[nodiscard]] std::string error_banner() const;

		/* Values the handlers need. Meaningful only for their own command. */
		[[nodiscard]] bool debug() const;
		[[nodiscard]] bool serial() const;
		[[nodiscard]] std::uint32_t cancel_session_id() const;
		[[nodiscard]] char app_state_to_set() const;
		[[nodiscard]] char app_state_to_query() const;
		[[nodiscard]] char fw_state_to_set() const;
		[[nodiscard]] char fw_state_to_query() const;

		private:
		[[nodiscard]] Command matched_action(std::size_t &action_count) const;

		UsageCapture output;

		TCLAP::CmdLine cmd;
		TCLAP::SwitchArg arg_switch_fw_slot;
		TCLAP::SwitchArg arg_switch_app_slot;
		TCLAP::SwitchArg arg_rollback_update;
		TCLAP::SwitchArg arg_commit_update;
		TCLAP::SwitchArg arg_urs;
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
		cli::PathValueArg install_path;
		TCLAP::SwitchArg arg_detach;
		TCLAP::SwitchArg arg_serial;
		TCLAP::SwitchArg install_progress;
		cli::NonEmptyValueArg<std::uint32_t> cancel_install_arg;
		cli::NonEmptyValueArg<char> set_app_state_bad;
		cli::NonEmptyValueArg<char> is_app_state_bad;
		cli::NonEmptyValueArg<char> set_fw_state_bad;
		cli::NonEmptyValueArg<char> is_fw_state_bad;
	};
}
