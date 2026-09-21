#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "cli_classify.h"

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
	 * Owns one getopt_long parse. Sequential reuse (tests parsing more than
	 * once) is safe, since parse() resets its own state at entry. This does
	 * NOT extend to concurrent or interleaved use: getopt_long itself drives
	 * process-global state (optind/opterr), so two parse() calls racing on
	 * different threads, or any other in-process getopt_long/getopt caller
	 * active at the same time, are not safe.
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

		/* The usage block. */
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
		/* Only what a caller can still ask for after parse() lives here; the
		 * install surface leaves through ParseResult::install_outcome and is
		 * parse()'s own business. */
		std::string m_banner;

		bool m_debug = false;
		bool m_serial = false;

		std::uint32_t m_cancel_session_id = 0;
		char m_app_state_to_set = 'c';
		char m_app_state_to_query = 'a';
		char m_fw_state_to_set = 'c';
		char m_fw_state_to_query = 'a';
	};
}
