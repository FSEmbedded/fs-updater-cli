#include "cli_args.h"

#include "fs_updater_error.h"
#include "config.h"

#include <array>

/* The argument definitions are the originals, moved verbatim: their description
 * strings are what the usage block is built from, so tests/golden/help.txt pins
 * them character for character. */
cli::CliArgs::CliArgs():
		cmd("F&S Update Framework CLI", ' ', FUS_CLI_PROJECT_VERSION, false),
		arg_switch_fw_slot("",
				"switch_fw_slot",
				"Switch from active firmware slot to the inactive "\
				"(apply update required)"
				),
		arg_switch_app_slot("",
				 "switch_app_slot",
				 "Switch from active to the inactive application slot. "\
				 "(apply update required)"
				 ),
		arg_rollback_update("",
				 "rollback_update",
				 "Rollback of the last installed update "\
				 "(must be started before commit update)"
				 ),
		arg_commit_update("",
				  "commit_update",
				  "Confirm success of installation, rollback, switch or fail. "\
				  "Run after boot and waits for application response"
				  ),
		arg_urs("",
			"update_reboot_state",
			"Get state of update"
			),
		arg_debug("",
			  "debug",
			  "Enable debug output"
			  ),
		get_fw_version("",
			       "firmware_version",
			       "Show current firmware version"
			       ),
		get_app_version("",
				"application_version",
				"Show current application version"
				),
		get_version("",
			    "version",
			    "Print cli version"
			    ),
		arg_help("h",
			 "help",
			 "Display usage information and exit"
			 ),
		notice_update_available("",
					"is_update_available",
					"Check update available on the server"
					),
		apply_update("",
			     "apply_update",
			     "Apply update installation, rollback or switch to other slot. "\
				 "Reboot to the updated slot."
			     ),
		download_progress("",
				  "download_progress",
				  "Show the progress of the current update"
				  ),
		download_update("",
					    "download_update",
					    "Download the available update"
					    ),
		install_update("",
					   "install_update",
					   "Install an update: with a path, install that local bundle "
					   "via the service (blocking); without, advance an ADU-staged "
					   "download"
					   ),
		install_path("install_path",
					 "Optional local update bundle path for --install_update",
					 false,
					 "",
					 "absolute filesystem path",
					 this->cmd
					 ),
		arg_detach("",
				   "detach",
				   "With --install_update <path>: start the install and return "
				   "immediately with the session id instead of blocking"
				   ),
		arg_serial("",
				   "serial",
				   "Send log output to the serial console (modifier, like --debug)"
				   ),
		install_progress("",
						 "install_progress",
						 "Show the progress of the current install"
						 ),
		cancel_install_arg("",
						   "cancel_install",
						   "Best-effort cancel of the install for the given session_id",
						   false,
						   0,
						   "session_id (uint32)"
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
				"Check application state for bad",
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
				"Check firmware state for bad",
				false,
			    'a',
				"accepted states: A or B"
			    )
{
	this->cmd.setOutput(&this->output);

    this->cmd.add(arg_rollback_update);
    this->cmd.add(arg_switch_fw_slot);
    this->cmd.add(arg_switch_app_slot);
    this->cmd.add(arg_commit_update);
    this->cmd.add(arg_urs);
    this->cmd.add(arg_debug);
    this->cmd.add(get_fw_version);
    this->cmd.add(get_app_version);
    this->cmd.add(get_version);
    this->cmd.add(arg_help);
    this->cmd.add(apply_update);
    this->cmd.add(install_update);
    this->cmd.add(install_path);
    this->cmd.add(arg_detach);
    this->cmd.add(arg_serial);
    this->cmd.add(install_progress);
    this->cmd.add(cancel_install_arg);
    this->cmd.add(download_progress);
    this->cmd.add(download_update);
    this->cmd.add(notice_update_available);
    this->cmd.add(set_app_state_bad);
    this->cmd.add(is_app_state_bad);
    this->cmd.add(set_fw_state_bad);
    this->cmd.add(is_fw_state_bad);
}

std::string cli::CliArgs::usage_text()
{
	this->output.usage_text.str("");
	this->output.usage(this->cmd);
	return this->output.usage_text.str();
}

/* Banner only. The stock output puts this on stderr and the usage block on
 * stdout; keeping them apart here is what lets the caller reproduce that. */
std::string cli::CliArgs::error_banner() const
{
	return this->output.banner.str();
}

cli::Command cli::CliArgs::matched_action(std::size_t &action_count) const
{
	struct ActionEntry
	{
		const TCLAP::Arg *arg;
		Command command;
	};

	const std::array<ActionEntry, 19> actions = {{
		{&this->arg_commit_update,       Command::commit_update},
		{&this->arg_urs,                 Command::update_reboot_state},
		{&this->get_app_version,         Command::application_version},
		{&this->get_fw_version,          Command::firmware_version},
		{&this->get_version,             Command::print_version},
		{&this->notice_update_available, Command::is_update_available},
		{&this->download_update,         Command::download_update},
		{&this->download_progress,       Command::download_progress},
		{&this->install_update,          Command::install_update},
		{&this->install_progress,        Command::install_progress},
		{&this->cancel_install_arg,      Command::cancel_install},
		{&this->apply_update,            Command::apply_update},
		{&this->arg_rollback_update,     Command::rollback_update},
		{&this->arg_switch_fw_slot,      Command::switch_fw_slot},
		{&this->arg_switch_app_slot,     Command::switch_app_slot},
		{&this->set_app_state_bad,       Command::set_app_state_bad},
		{&this->is_app_state_bad,        Command::is_app_state_bad},
		{&this->set_fw_state_bad,        Command::set_fw_state_bad},
		{&this->is_fw_state_bad,         Command::is_fw_state_bad},
	}};

	Command matched = Command::none;
	action_count = 0;

	for (const auto &entry : actions)
	{
		if (entry.arg->isSet())
		{
			matched = entry.command;
			++action_count;
		}
	}

	return matched;
}

cli::ParseResult cli::CliArgs::parse(int argc, const char **argv)
{
	ParseResult result;
	this->output.reset();

	/* Arguments latch once set, so a second parse on the same instance would
	 * see the first one's flags. Production parses once; tests do not. */
	this->cmd.reset();

	this->cmd.parse(argc, argv);

	/* The parser reports a rejection through the output rather than by
	 * terminating, so it lands here as a value. */
	if (this->output.failed)
	{
		result.kind = ParseResult::Kind::parse_error;
		result.rc   = 1;
		return result;
	}

	if (this->arg_help.isSet())
	{
		result.kind = ParseResult::Kind::help;
		return result;
	}

	RawFlags flags;
	flags.install_update_set = this->install_update.isSet();
	flags.install_path_set   = this->install_path.isSet();
	flags.install_path       = this->install_path.isSet()
								   ? this->install_path.getValue()
								   : std::string{};
	flags.detach = this->arg_detach.isSet();

	const ParseOutcome outcome = cli::classify(flags);
	result.install_outcome     = outcome;

	if (outcome.kind != ParseOutcome::Kind::ok)
	{
		result.kind = ParseResult::Kind::guard_error;
		/* An unusable path is a file-not-found, not an argument-combo error —
		 * script callers key on it for a bad bundle path. */
		result.rc = (outcome.kind == ParseOutcome::Kind::bad_path)
						? static_cast<int>(UPDATER_CLI_VALIDATION::UPDATE_FILE_NOT_FOUND)
						: static_cast<int>(UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO);
		return result;
	}

	result.command = this->matched_action(result.action_count);

	switch (cli::dispatch_verdict(result.action_count))
	{
	case DispatchVerdict::version_only:
		result.kind = ParseResult::Kind::version_only;
		break;
	case DispatchVerdict::run:
		result.kind = ParseResult::Kind::ok;
		break;
	case DispatchVerdict::combo_error:
		result.kind = ParseResult::Kind::combo_error;
		/* Nothing is dispatchable, so report no command rather than whichever
		 * of the competing flags the scan happened to see last. */
		result.command = Command::none;
		result.rc      = static_cast<int>(UPDATER_CLI_VALIDATION::INCOMPATIBLE_ARG_COMBO);
		break;
	}

	return result;
}

bool cli::CliArgs::debug() const  { return this->arg_debug.isSet(); }
bool cli::CliArgs::serial() const { return this->arg_serial.isSet(); }

std::uint32_t cli::CliArgs::cancel_session_id() const { return this->cancel_install_arg.getValue(); }
char cli::CliArgs::app_state_to_set() const   { return this->set_app_state_bad.getValue(); }
char cli::CliArgs::app_state_to_query() const { return this->is_app_state_bad.getValue(); }
char cli::CliArgs::fw_state_to_set() const    { return this->set_fw_state_bad.getValue(); }
char cli::CliArgs::fw_state_to_query() const  { return this->is_fw_state_bad.getValue(); }
