#include "cli_args.h"

#include "arg_scan.h"
#include "fs_updater_error.h"

#include <getopt.h>

#include <array>
#include <cstring>
#include <vector>

/* One table declares every option and everything about it — how it parses,
 * whether it counts as an action, where its value lands, how it reads in the
 * usage text. Nothing about an option is stated anywhere else: the checks
 * below are exhaustive over the table's own enums, so an option that is
 * declared but not routed fails the build rather than parsing into nothing. */
namespace cli
{
namespace
{
	enum class ArgKind
	{
		action_switch, /* counts as an action, takes no value */
		action_value,  /* counts as an action, takes a value */
		modifier,      /* --debug/--serial/--detach: never an action */
		help,          /* -h/--help */
	};

	enum class ValueKind
	{
		none,
		u32,
		state_char,
	};

	struct OptionSpec
	{
		const char *name;
		ArgKind kind;
		Command command;       /* meaningful for action_switch/action_value only */
		ValueKind value_kind;   /* meaningful for action_value only */
		const char *value_name; /* usage text, e.g. "session_id (uint32)" */
		const char *help;
	};

	/* getopt_long option codes start past every ASCII byte, so the short
	 * option 'h' stays free for -h/--help. Table order also drives
	 * usage_text() — kept in the previous parser's add() order to minimize
	 * the visible diff in the golden usage block. */
	constexpr int kFirstCode = 256;

	constexpr std::array<OptionSpec, 23> kOptions = {{
		{"rollback_update", ArgKind::action_switch, Command::rollback_update, ValueKind::none, nullptr,
			"Rollback of the last installed update (must be started before commit update)"},
		{"switch_fw_slot", ArgKind::action_switch, Command::switch_fw_slot, ValueKind::none, nullptr,
			"Switch from active firmware slot to the inactive (apply update required)"},
		{"switch_app_slot", ArgKind::action_switch, Command::switch_app_slot, ValueKind::none, nullptr,
			"Switch from active to the inactive application slot. (apply update required)"},
		{"commit_update", ArgKind::action_switch, Command::commit_update, ValueKind::none, nullptr,
			"Confirm success of installation, rollback, switch or fail. Run after boot and waits for application response"},
		{"update_reboot_state", ArgKind::action_switch, Command::update_reboot_state, ValueKind::none, nullptr,
			"Get state of update"},
		{"debug", ArgKind::modifier, Command::none, ValueKind::none, nullptr,
			"Enable debug output"},
		{"firmware_version", ArgKind::action_switch, Command::firmware_version, ValueKind::none, nullptr,
			"Show current firmware version"},
		{"application_version", ArgKind::action_switch, Command::application_version, ValueKind::none, nullptr,
			"Show current application version"},
		{"version", ArgKind::action_switch, Command::print_version, ValueKind::none, nullptr,
			"Print cli version"},
		{"help", ArgKind::help, Command::none, ValueKind::none, nullptr,
			"Display usage information and exit"},
		{"apply_update", ArgKind::action_switch, Command::apply_update, ValueKind::none, nullptr,
			"Apply update installation, rollback or switch to other slot. Reboot to the updated slot."},
		{"install_update", ArgKind::action_switch, Command::install_update, ValueKind::none, nullptr,
			"Install an update: with a path, install that local bundle via the service (blocking); without, advance an ADU-staged download"},
		{"detach", ArgKind::modifier, Command::none, ValueKind::none, nullptr,
			"With --install_update <path>: start the install and return immediately with the session id instead of blocking"},
		{"serial", ArgKind::modifier, Command::none, ValueKind::none, nullptr,
			"Send log output to the serial console (modifier, like --debug)"},
		{"install_progress", ArgKind::action_switch, Command::install_progress, ValueKind::none, nullptr,
			"Show the progress of the current install"},
		{"cancel_install", ArgKind::action_value, Command::cancel_install, ValueKind::u32,
			"session_id (uint32)", "Best-effort cancel of the install for the given session_id"},
		{"download_progress", ArgKind::action_switch, Command::download_progress, ValueKind::none, nullptr,
			"Show the progress of the current update"},
		{"download_update", ArgKind::action_switch, Command::download_update, ValueKind::none, nullptr,
			"Download the available update"},
		{"is_update_available", ArgKind::action_switch, Command::is_update_available, ValueKind::none, nullptr,
			"Check update available on the server"},
		{"set_app_state_bad", ArgKind::action_value, Command::set_app_state_bad, ValueKind::state_char,
			"accepted states: A or B", "Mark application A or B bad"},
		{"is_app_state_bad", ArgKind::action_value, Command::is_app_state_bad, ValueKind::state_char,
			"accepted states: A or B", "Check application state for bad"},
		{"set_fw_state_bad", ArgKind::action_value, Command::set_fw_state_bad, ValueKind::state_char,
			"accepted states: A or B", "Mark firmware A or B bad"},
		{"is_fw_state_bad", ArgKind::action_value, Command::is_fw_state_bad, ValueKind::state_char,
			"accepted states: A or B", "Check firmware state for bad"},
	}};

	constexpr const char *kInstallPathName     = "install_path";
	constexpr const char *kInstallPathTypeDesc = "absolute filesystem path";
	constexpr const char *kInstallPathHelp     = "Optional local update bundle path for --install_update";

	int code_of(std::size_t index)
	{
		return (kOptions[index].kind == ArgKind::help) ? 'h' : (kFirstCode + static_cast<int>(index));
	}

	std::vector<option> build_long_options()
	{
		std::vector<option> opts;
		opts.reserve(kOptions.size() + 1);
		for (std::size_t i = 0; i < kOptions.size(); ++i)
		{
			const auto &spec = kOptions[i];
			const int has_arg = (spec.kind == ArgKind::action_value) ? required_argument : no_argument;
			opts.push_back(option{spec.name, has_arg, nullptr, code_of(i)});
		}
		opts.push_back(option{nullptr, 0, nullptr, 0});
		return opts;
	}

	/* -1 when code matches none of ours — cannot happen for a code getopt_long
	 * itself handed back, since every code it knows about came from our own
	 * table, but the check keeps the lookup total. */
	long find_option_index(int code)
	{
		for (std::size_t i = 0; i < kOptions.size(); ++i)
		{
			if (code_of(i) == code) return static_cast<long>(i);
		}
		return -1;
	}

	std::string banner_for(const std::string &token, const std::string &reason)
	{
		/* The previous parser's shape; the continuation indent aligns the reason
		 * under the token. ErrorBanner in the tests pins it. */
		return "PARSE ERROR: " + token + "\n             " + reason + "\n\n";
	}

	/* Why the table says a long token is unusable. getopt_long reports both an
	 * unknown option and a well-formed flag misused ("--debug=1", an ambiguous
	 * prefix) as a bare '?', which is too coarse to diagnose. */
	enum class TokenVerdict
	{
		unknown,        /* matches no table name, not even as a prefix */
		abbreviation,   /* a prefix of at least one name, but not a name */
		takes_no_value, /* a name, given "=value" it does not accept */
		exact,          /* a name, used well-formed */
	};

	TokenVerdict classify_token(const char *token)
	{
		if (token == nullptr || token[0] != '-' || token[1] != '-') return TokenVerdict::unknown;

		const char *name = token + 2;
		std::size_t n = 0;
		while (name[n] != '\0' && name[n] != '=') ++n;
		if (n == 0) return TokenVerdict::unknown;
		const bool has_value = (name[n] == '=');

		for (const auto &spec : kOptions)
		{
			if (std::strlen(spec.name) != n || std::strncmp(name, spec.name, n) != 0) continue;
			return (has_value && spec.kind != ArgKind::action_value) ? TokenVerdict::takes_no_value
			                                                         : TokenVerdict::exact;
		}
		for (const auto &spec : kOptions)
		{
			if (std::strncmp(name, spec.name, n) == 0) return TokenVerdict::abbreviation;
		}
		return TokenVerdict::unknown;
	}
}

CliArgs::CliArgs() = default;

ParseResult CliArgs::parse(int argc, const char **argv)
{
	ParseResult result;

	m_banner.clear();
	m_debug = false;
	m_serial = false;
	m_detach = false;
	m_install_path_set = false;
	m_install_update_set = false;
	m_install_path.clear();
	m_cancel_session_id = 0;
	m_app_state_to_set = 'c';
	m_app_state_to_query = 'a';
	m_fw_state_to_set = 'c';
	m_fw_state_to_query = 'a';

	/* getopt_long wants a mutable argv; these c-strings are never rewritten,
	 * only the array of pointers is a local copy. */
	std::vector<char *> argv_copy;
	argv_copy.reserve(static_cast<std::size_t>(argc) + 1);
	for (int i = 0; i < argc; ++i) argv_copy.push_back(const_cast<char *>(argv[i]));
	argv_copy.push_back(nullptr);

	const std::vector<option> long_options = build_long_options();

	/* optind = 0 forces glibc's full internal re-initialization, not just an
	 * index reset — required to parse more than once per process (tests; a
	 * production run only ever parses once). opterr = 0 suppresses getopt's
	 * own stderr message, since every rejection is reported through our own
	 * banner instead. optstring "-:h": leading '-' returns each operand in
	 * argv order (RETURN_IN_ORDER) rather than permuting or deferring to
	 * POSIXLY_CORRECT; ':' distinguishes a missing value from an unknown
	 * option; 'h' is the short form of --help. */
	::optind = 0;
	::opterr = 0;

	std::array<bool, kOptions.size()> seen{};
	std::size_t distinct_actions = 0;
	Command matched_command = Command::none;
	bool has_operand = false;
	bool help_requested = false;

	for (;;)
	{
		/* optind reads 0 only before the very first call (our reset sentinel,
		 * not yet corrected by glibc's internal re-init) — the real first
		 * argument is always at index 1. Every later iteration already has a
		 * valid optind from the previous call. */
		const int captured_index = (optind == 0) ? 1 : optind;
		const char *raw_token = (captured_index >= 0 && captured_index < argc)
			? argv_copy[static_cast<std::size_t>(captured_index)]
			: nullptr;

		int longindex = -1;
		const int c = ::getopt_long(argc, argv_copy.data(), "-:h", long_options.data(), &longindex);
		if (c == -1) break;

		if (c == 1)
		{
			/* Operand (RETURN_IN_ORDER). A token that looks like an option is
			 * never reported here: getopt_long always tries option matching
			 * first for anything starting with '-', producing '?' instead.
			 * Tokens after a literal "--" never reach this branch
			 * either: getopt_long returns -1 at the "--" instead of yielding
			 * what follows, so the post-loop scan collects those. */
			const std::string token = (raw_token != nullptr) ? raw_token : "";

			/* A lone "-" is rejected as a path; only this branch rejects it,
			 * the post-loop scan accepts dash-prefixed operands. */
			if (token == "-")
			{
				m_banner = banner_for(token, "unrecognized option");
				result.kind = ParseResult::Kind::parse_error;
				result.rc = 1;
				return result;
			}

			if (has_operand)
			{
				m_banner = banner_for(token, "an install path may be given at most once");
				result.kind = ParseResult::Kind::parse_error;
				result.rc = 1;
				return result;
			}
			has_operand = true;
			m_install_path_set = true;
			m_install_path = token;
			continue;
		}

		if (c == ':' || c == '?')
		{
			/* An abbreviation is rejected on its own merits either way: getopt
			 * resolves a unique prefix and only then misses the value (':'),
			 * while an ambiguous one never resolves at all ('?') — both must
			 * name the abbreviation, not the symptom. */
			const char *token = (raw_token != nullptr) ? raw_token : "?";
			const TokenVerdict verdict = classify_token(raw_token);

			if (verdict == TokenVerdict::abbreviation)
				m_banner = banner_for(token, "unknown option (abbreviations are not accepted)");
			else if (verdict == TokenVerdict::takes_no_value)
				m_banner = banner_for(token, "option takes no value");
			else if (c == ':')
				m_banner = banner_for(token, "missing a required value");
			else
				m_banner = banner_for(token, "unrecognized option");

			result.kind = ParseResult::Kind::parse_error;
			result.rc = 1;
			return result;
		}

		const long index = find_option_index(c);
		if (index < 0)
		{
			/* Unreachable: every code getopt_long can return here is one this
			 * table generated. Kept as a hard stop rather than silently
			 * ignoring an option we cannot classify. */
			m_banner = banner_for(raw_token != nullptr ? raw_token : "?", "internal parser error");
			result.kind = ParseResult::Kind::parse_error;
			result.rc = 1;
			return result;
		}

		const OptionSpec &spec = kOptions[static_cast<std::size_t>(index)];

		/* No abbreviation, for any long option including --help: a
		 * prefix match breaks silently once a later flag makes it ambiguous.
		 * Short options (raw_token[1] != '-') are never abbreviations. */
		if (raw_token != nullptr && raw_token[0] == '-' && raw_token[1] == '-'
			&& !exact_long_match(raw_token, spec.name))
		{
			m_banner = banner_for(raw_token, "unknown option (abbreviations are not accepted)");
			result.kind = ParseResult::Kind::parse_error;
			result.rc = 1;
			return result;
		}

		/* Repeating a flag is rejected outright, as the previous parser's
		 * per-Arg "already set" tracking did. Modifiers are the one lenient
		 * exception; help is not (found during review) — so the check
		 * sits ahead of both branches below and covers actions and help alike. */
		if (spec.kind != ArgKind::modifier)
		{
			if (seen[static_cast<std::size_t>(index)])
			{
				m_banner = banner_for(raw_token != nullptr ? raw_token : spec.name,
					"already given, must not repeat");
				result.kind = ParseResult::Kind::parse_error;
				result.rc = 1;
				return result;
			}
			seen[static_cast<std::size_t>(index)] = true;
		}

		if (spec.kind == ArgKind::help)
		{
			/* Do not return yet: the previous parser scanned the whole line
			 * before honouring --help, so an error anywhere else in argv
			 * (unknown option, missing value, a bundled "-hx") still wins.
			 * Checked once after the loop, ahead of classify(). */
			help_requested = true;
			continue;
		}

		if (spec.kind == ArgKind::modifier)
		{
			if (std::strcmp(spec.name, "debug") == 0) m_debug = true;
			else if (std::strcmp(spec.name, "serial") == 0) m_serial = true;
			else if (std::strcmp(spec.name, "detach") == 0) m_detach = true;
			continue;
		}

		if (spec.kind == ArgKind::action_value)
		{
			/* An empty value ("--flag ''" or "--flag=") is rejected. */
			if (optarg == nullptr || optarg[0] == '\0')
			{
				m_banner = banner_for(raw_token != nullptr ? raw_token : spec.name, "value must not be empty");
				result.kind = ParseResult::Kind::parse_error;
				result.rc = 1;
				return result;
			}

			if (spec.value_kind == ValueKind::u32)
			{
				const auto value = parse_u32(optarg);
				if (!value)
				{
					m_banner = banner_for(raw_token != nullptr ? raw_token : spec.name,
						"value must be a decimal number, no sign, no overflow");
					result.kind = ParseResult::Kind::parse_error;
					result.rc = 1;
					return result;
				}
				m_cancel_session_id = *value;
			}
			else /* ValueKind::state_char */
			{
				const auto value = parse_state_char(optarg);
				if (!value)
				{
					m_banner = banner_for(raw_token != nullptr ? raw_token : spec.name,
						"value must be exactly one character");
					result.kind = ParseResult::Kind::parse_error;
					result.rc = 1;
					return result;
				}
				switch (spec.command)
				{
				case Command::set_app_state_bad: m_app_state_to_set = *value; break;
				case Command::is_app_state_bad:  m_app_state_to_query = *value; break;
				case Command::set_fw_state_bad:  m_fw_state_to_set = *value; break;
				case Command::is_fw_state_bad:   m_fw_state_to_query = *value; break;
				default: break;
				}
			}
		}

		if (spec.command == Command::install_update) m_install_update_set = true;

		++distinct_actions;
		matched_command = spec.command;
	}

	/* A literal "--" makes getopt_long return -1 immediately, regardless of
	 * RETURN_IN_ORDER: it does not keep returning code 1 for what follows.
	 * optind is left pointing at the first token after it, so any remaining
	 * operands still need the same single-operand handling as the ones
	 * getopt_long did return via code 1 above. */
	for (int i = optind; i < argc; ++i)
	{
		const char *token_ptr = argv_copy[static_cast<std::size_t>(i)];
		const std::string token = (token_ptr != nullptr) ? token_ptr : "";
		if (has_operand)
		{
			m_banner = banner_for(token, "an install path may be given at most once");
			result.kind = ParseResult::Kind::parse_error;
			result.rc = 1;
			return result;
		}
		has_operand = true;
		m_install_path_set = true;
		m_install_path = token;
	}

	/* Full argv is now known error-free — honour a --help seen anywhere,
	 * ahead of classify(), matching the previous parser's ordering. */
	if (help_requested)
	{
		result.kind = ParseResult::Kind::help;
		return result;
	}

	RawFlags flags;
	flags.install_update_set = m_install_update_set;
	flags.install_path_set   = m_install_path_set;
	flags.install_path       = m_install_path;
	flags.detach             = m_detach;

	const ParseOutcome outcome = classify(flags);
	result.install_outcome    = outcome;

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

	result.command      = matched_command;
	result.action_count = distinct_actions;

	switch (dispatch_verdict(result.action_count))
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

namespace
{
	/* One bracketed synopsis token per option, in table order — the same
	 * source usage_text()'s "Where:" block and help.txt both read from, so
	 * the three cannot drift apart. */
	std::string synopsis_token(const OptionSpec &spec)
	{
		if (spec.kind == ArgKind::help) return "[-h]";

		std::string tok = "[--";
		tok += spec.name;
		if (spec.value_name != nullptr)
		{
			tok += " <";
			tok += spec.value_name;
			tok += ">";
		}
		tok += "]";
		return tok;
	}

	/* Plain whitespace word-wrap — no <sstream>/<iostream>, which this code
	 * does not use, just find()/substr() over the description
	 * strings the option table already holds. */
	std::string wrap_paragraph(const std::string &text, const std::string &indent, std::size_t width)
	{
		std::string out;
		std::string line = indent;
		bool at_line_start = true;
		std::size_t pos = 0;

		while (pos < text.size())
		{
			std::size_t next = text.find(' ', pos);
			if (next == std::string::npos) next = text.size();
			const std::string word = text.substr(pos, next - pos);
			pos = next + 1;

			const std::size_t needed = (at_line_start ? 0 : 1) + word.size();
			if (!at_line_start && line.size() + needed > width)
			{
				out += line;
				out += "\n";
				line = indent;
				at_line_start = true;
			}
			if (!at_line_start) line += " ";
			line += word;
			at_line_start = false;
		}
		out += line;
		out += "\n";
		return out;
	}
}

std::string CliArgs::usage_text()
{
	std::string text;
	text.reserve(2048);

	text += "\nUSAGE:\n\n";

	std::vector<std::string> tokens;
	tokens.reserve(kOptions.size() + 2);
	for (const auto &spec : kOptions) tokens.push_back(synopsis_token(spec));
	tokens.emplace_back("[--]");
	tokens.emplace_back("[install_path]");

	const std::string first_prefix = "   fs-updater ";
	const std::string cont_prefix(first_prefix.size(), ' ');
	constexpr std::size_t kWidth = 79;

	std::string line = first_prefix;
	bool at_line_start = true;
	for (const auto &tok : tokens)
	{
		const std::size_t needed = (at_line_start ? 0 : 1) + tok.size();
		if (!at_line_start && line.size() + needed > kWidth)
		{
			text += line;
			text += "\n";
			line = cont_prefix;
			at_line_start = true;
		}
		if (!at_line_start) line += " ";
		line += tok;
		at_line_start = false;
	}
	text += line;
	text += "\n\n";

	text += "Where:\n\n";

	constexpr std::size_t kDescWidth = 79;
	const std::string kDescIndent = "     ";

	for (const auto &spec : kOptions)
	{
		if (spec.kind == ArgKind::help)
		{
			text += "   -h, --help\n";
			text += wrap_paragraph(spec.help, kDescIndent, kDescWidth);
			text += "\n";
			continue;
		}

		text += "   --";
		text += spec.name;
		if (spec.value_name != nullptr)
		{
			text += " <";
			text += spec.value_name;
			text += ">";
		}
		text += "\n";
		text += wrap_paragraph(spec.help, kDescIndent, kDescWidth);
		text += "\n";
	}

	text += "   --\n";
	text += wrap_paragraph(
		"Ends option parsing: every token after this one is the install path, "
		"even one that starts with '-'.",
		kDescIndent, kDescWidth);
	text += "\n";

	text += "   ";
	text += kInstallPathName;
	text += " <";
	text += kInstallPathTypeDesc;
	text += ">\n";
	text += wrap_paragraph(
		std::string(kInstallPathHelp) +
			". The state-taking flags above (--set_app_state_bad, "
			"--is_app_state_bad, --set_fw_state_bad, --is_fw_state_bad) accept "
			"any single character at this layer; the accepted values (A or B) "
			"are checked by the command, not the parser.",
		kDescIndent, kDescWidth);

	text += "\n   F&S Update Framework CLI\n\n";

	return text;
}

std::string CliArgs::error_banner() const { return m_banner; }

bool CliArgs::debug() const  { return m_debug; }
bool CliArgs::serial() const { return m_serial; }

std::uint32_t CliArgs::cancel_session_id() const { return m_cancel_session_id; }
char CliArgs::app_state_to_set() const   { return m_app_state_to_set; }
char CliArgs::app_state_to_query() const { return m_app_state_to_query; }
char CliArgs::fw_state_to_set() const    { return m_fw_state_to_set; }
char CliArgs::fw_state_to_query() const  { return m_fw_state_to_query; }
}
