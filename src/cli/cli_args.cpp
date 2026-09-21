#include "cli_args.h"

#include "arg_scan.h"
#include "fs_updater_error.h"

#include <getopt.h>

#include <array>
#include <cstring>
#include <iterator>
#include <string>
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
	enum class Role
	{
		action,   /* competes for the one dispatchable command */
		modifier, /* --debug/--serial/--detach: never an action */
		help,     /* -h/--help */
	};

	enum class ValueKind
	{
		none, /* a switch */
		u32,
		state_char,
	};

	/* Where a flag's effect lands. Naming it in the table is what keeps the
	 * declaration and the assignment from drifting: the switches that consume
	 * a Slot are exhaustive and the static_assert below rejects a row whose
	 * slot does not fit its role, so neither half can be forgotten. */
	enum class Slot
	{
		none, /* the flag's whole effect is its Command */
		debug,
		serial,
		detach,
		cancel_session_id,
		app_state_set,
		app_state_query,
		fw_state_set,
		fw_state_query,
	};

	constexpr bool is_modifier_slot(Slot s)
	{
		return s == Slot::debug || s == Slot::serial || s == Slot::detach;
	}

	constexpr bool is_u32_slot(Slot s) { return s == Slot::cancel_session_id; }

	constexpr bool is_state_slot(Slot s)
	{
		return s == Slot::app_state_set || s == Slot::app_state_query
			|| s == Slot::fw_state_set || s == Slot::fw_state_query;
	}

	struct OptionSpec
	{
		const char *name;
		Role role;
		Command command;        /* actions only */
		ValueKind value_kind;
		Slot slot;
		const char *value_name; /* usage text, e.g. "session_id (uint32)" */
		const char *help;
	};

	/* getopt_long option codes start past every ASCII byte, so the short
	 * option 'h' stays free for -h/--help. Table order also drives
	 * usage_text(). */
	constexpr int kFirstCode = 256;

	constexpr OptionSpec kOptions[] = {
		{"rollback_update", Role::action, Command::rollback_update, ValueKind::none, Slot::none, nullptr,
			"Rollback of the last installed update (must be started before commit update)"},
		{"switch_fw_slot", Role::action, Command::switch_fw_slot, ValueKind::none, Slot::none, nullptr,
			"Switch from active firmware slot to the inactive (apply update required)"},
		{"switch_app_slot", Role::action, Command::switch_app_slot, ValueKind::none, Slot::none, nullptr,
			"Switch from active to the inactive application slot. (apply update required)"},
		{"commit_update", Role::action, Command::commit_update, ValueKind::none, Slot::none, nullptr,
			"Confirm success of installation, rollback, switch or fail. Run after boot and waits for application response"},
		{"update_reboot_state", Role::action, Command::update_reboot_state, ValueKind::none, Slot::none, nullptr,
			"Get state of update"},
		{"debug", Role::modifier, Command::none, ValueKind::none, Slot::debug, nullptr,
			"Enable debug output"},
		{"firmware_version", Role::action, Command::firmware_version, ValueKind::none, Slot::none, nullptr,
			"Show current firmware version"},
		{"application_version", Role::action, Command::application_version, ValueKind::none, Slot::none, nullptr,
			"Show current application version"},
		{"version", Role::action, Command::print_version, ValueKind::none, Slot::none, nullptr,
			"Print cli version"},
		{"help", Role::help, Command::none, ValueKind::none, Slot::none, nullptr,
			"Display usage information and exit"},
		{"apply_update", Role::action, Command::apply_update, ValueKind::none, Slot::none, nullptr,
			"Apply update installation, rollback or switch to other slot. Reboot to the updated slot."},
		{"install_update", Role::action, Command::install_update, ValueKind::none, Slot::none, nullptr,
			"Install an update: with a path, install that local bundle via the service (blocking); without, advance an ADU-staged download"},
		{"detach", Role::modifier, Command::none, ValueKind::none, Slot::detach, nullptr,
			"With --install_update <path>: start the install and return immediately with the session id instead of blocking"},
		{"serial", Role::modifier, Command::none, ValueKind::none, Slot::serial, nullptr,
			"Send log output to the serial console (modifier, like --debug)"},
		{"install_progress", Role::action, Command::install_progress, ValueKind::none, Slot::none, nullptr,
			"Show the progress of the current install"},
		{"cancel_install", Role::action, Command::cancel_install, ValueKind::u32, Slot::cancel_session_id,
			"session_id (uint32)", "Best-effort cancel of the install for the given session_id"},
		{"download_progress", Role::action, Command::download_progress, ValueKind::none, Slot::none, nullptr,
			"Show the progress of the current update"},
		{"download_update", Role::action, Command::download_update, ValueKind::none, Slot::none, nullptr,
			"Report whether a download is running (the update source starts them)"},
		{"is_update_available", Role::action, Command::is_update_available, ValueKind::none, Slot::none, nullptr,
			"Check update available on the server"},
		{"set_app_state_bad", Role::action, Command::set_app_state_bad, ValueKind::state_char, Slot::app_state_set,
			"accepted states: A or B", "Mark application A or B bad"},
		{"is_app_state_bad", Role::action, Command::is_app_state_bad, ValueKind::state_char, Slot::app_state_query,
			"accepted states: A or B", "Check application state for bad"},
		{"set_fw_state_bad", Role::action, Command::set_fw_state_bad, ValueKind::state_char, Slot::fw_state_set,
			"accepted states: A or B", "Mark firmware A or B bad"},
		{"is_fw_state_bad", Role::action, Command::is_fw_state_bad, ValueKind::state_char, Slot::fw_state_query,
			"accepted states: A or B", "Check firmware state for bad"},
	};

	constexpr std::size_t kOptionCount = std::size(kOptions);

	/* Each row's slot must fit its role and value kind, so a flag cannot be
	 * declared with nowhere to put what it parses. Together with the
	 * exhaustive switches in parse(), this is what makes the table the only
	 * place an option is described. */
	constexpr bool table_is_routed()
	{
		for (const auto &spec : kOptions)
		{
			if ((spec.role == Role::modifier) != is_modifier_slot(spec.slot)) return false;

			switch (spec.value_kind)
			{
			case ValueKind::none:
				if (is_u32_slot(spec.slot) || is_state_slot(spec.slot)) return false;
				break;
			case ValueKind::u32:
				if (!is_u32_slot(spec.slot)) return false;
				break;
			case ValueKind::state_char:
				if (!is_state_slot(spec.slot)) return false;
				break;
			}
		}
		return true;
	}
	static_assert(table_is_routed(), "an option's slot must match its role and value kind");

	constexpr std::size_t find_help_index()
	{
		for (std::size_t i = 0; i < kOptionCount; ++i)
		{
			if (kOptions[i].role == Role::help) return i;
		}
		return kOptionCount;
	}
	constexpr std::size_t kHelpIndex = find_help_index();
	static_assert(kHelpIndex < kOptionCount, "the table must declare a help option");

	constexpr const char *kInstallPathName     = "install_path";
	constexpr const char *kInstallPathTypeDesc = "absolute filesystem path";

	constexpr int code_of(std::size_t index)
	{
		return (kOptions[index].role == Role::help) ? 'h' : (kFirstCode + static_cast<int>(index));
	}

	/* Built once at compile time from the table; the codes are the table index
	 * offset by kFirstCode, which is what lets parse() find a row by
	 * arithmetic instead of searching. */
	constexpr std::array<option, kOptionCount + 1> make_long_options()
	{
		std::array<option, kOptionCount + 1> opts{}; /* last stays the zero terminator */
		for (std::size_t i = 0; i < kOptionCount; ++i)
		{
			const int has_arg = (kOptions[i].value_kind != ValueKind::none) ? required_argument : no_argument;
			opts[i] = option{kOptions[i].name, has_arg, nullptr, code_of(i)};
		}
		return opts;
	}
	constexpr std::array<option, kOptionCount + 1> kLongOptions = make_long_options();

	std::string banner_for(const std::string &token, const std::string &reason)
	{
		/* The continuation indent aligns the reason under the token. ErrorBanner in the tests pins it. */
		return "PARSE ERROR: " + token + "\n             " + reason + "\n\n";
	}

	/* Every rejection is the same verdict, so it is stated once: name the
	 * token, say why, return code 1. */
	ParseResult fail(std::string &banner, const char *token, const char *reason)
	{
		ParseResult result;
		banner = banner_for(token != nullptr ? token : "?", reason);
		result.kind = ParseResult::Kind::parse_error;
		result.rc = 1;
		return result;
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
			return (has_value && spec.value_kind == ValueKind::none) ? TokenVerdict::takes_no_value
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

	std::array<bool, kOptionCount> seen{};
	std::size_t distinct_actions = 0;
	Command matched_command = Command::none;
	bool help_requested = false;
	bool detach = false;
	bool install_update_set = false;
	bool has_operand = false;
	std::string install_path;

	/* The install path may be given once, whichever of the two ways it
	 * arrives: in argv order from getopt, or from the tail after a literal
	 * "--". False means one was already taken. */
	const auto take_operand = [&](const std::string &token) {
		if (has_operand) return false;
		has_operand = true;
		install_path = token;
		return true;
	};

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

		const int c = ::getopt_long(argc, argv_copy.data(), "-:h", kLongOptions.data(), nullptr);
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
			if (token == "-") return fail(m_banner, token.c_str(), "unrecognized option");

			if (!take_operand(token))
				return fail(m_banner, token.c_str(), "an install path may be given at most once");
			continue;
		}

		if (c == ':' || c == '?')
		{
			/* An abbreviation is rejected on its own merits either way: getopt
			 * resolves a unique prefix and only then misses the value (':'),
			 * while an ambiguous one never resolves at all ('?') — both must
			 * name the abbreviation, not the symptom. */
			switch (classify_token(raw_token))
			{
			case TokenVerdict::abbreviation:
				return fail(m_banner, raw_token, "unknown option (abbreviations are not accepted)");
			case TokenVerdict::takes_no_value:
				return fail(m_banner, raw_token, "option takes no value");
			case TokenVerdict::unknown:
			case TokenVerdict::exact:
				break;
			}
			return fail(m_banner, raw_token,
				(c == ':') ? "missing a required value" : "unrecognized option");
		}

		/* Codes are the table index offset by kFirstCode, and help is the one
		 * short option — so the row is arithmetic, not a search. The bound
		 * check keeps that total for a code we never handed out. */
		const std::size_t index = (c == 'h') ? kHelpIndex : static_cast<std::size_t>(c - kFirstCode);
		if (index >= kOptionCount) return fail(m_banner, raw_token, "internal parser error");

		const OptionSpec &spec = kOptions[index];

		/* No abbreviation, for any long option including --help: a
		 * prefix match breaks silently once a later flag makes it ambiguous.
		 * Short options (raw_token[1] != '-') are never abbreviations. */
		if (raw_token != nullptr && raw_token[0] == '-' && raw_token[1] == '-'
			&& !exact_long_match(raw_token, spec.name))
		{
			return fail(m_banner, raw_token, "unknown option (abbreviations are not accepted)");
		}

		/* Repeating a flag is rejected; modifiers are the only exception. */
		if (spec.role != Role::modifier)
		{
			if (seen[index])
			{
				return fail(m_banner, raw_token != nullptr ? raw_token : spec.name,
					"already given, must not repeat");
			}
			seen[index] = true;
		}

		if (spec.role == Role::help)
		{
			/* Do not return yet: an error anywhere else in argv (unknown
			 * option, missing value, a bundled "-hx") still wins over --help.
			 * Checked once after the loop, ahead of classify(). */
			help_requested = true;
			continue;
		}

		if (spec.role == Role::modifier)
		{
			/* Exhaustive on purpose, no default: a new modifier slot has to be
			 * routed here or the build fails. */
			switch (spec.slot)
			{
			case Slot::debug:  m_debug = true; break;
			case Slot::serial: m_serial = true; break;
			case Slot::detach: detach = true; break;
			case Slot::none:
			case Slot::cancel_session_id:
			case Slot::app_state_set:
			case Slot::app_state_query:
			case Slot::fw_state_set:
			case Slot::fw_state_query:
				break; /* not modifier targets — the static_assert rules them out */
			}
			continue;
		}

		if (spec.value_kind != ValueKind::none)
		{
			/* An empty value ("--flag ''" or "--flag=") is rejected. */
			if (optarg == nullptr || optarg[0] == '\0')
			{
				return fail(m_banner, raw_token != nullptr ? raw_token : spec.name,
					"value must not be empty");
			}

			if (spec.value_kind == ValueKind::u32)
			{
				const auto value = parse_u32(optarg);
				if (!value)
				{
					return fail(m_banner, raw_token != nullptr ? raw_token : spec.name,
						"value must be a decimal number, no sign, no overflow");
				}
				switch (spec.slot)
				{
				case Slot::cancel_session_id: m_cancel_session_id = *value; break;
				case Slot::none:
				case Slot::debug:
				case Slot::serial:
				case Slot::detach:
				case Slot::app_state_set:
				case Slot::app_state_query:
				case Slot::fw_state_set:
				case Slot::fw_state_query:
					break; /* not uint32 targets — the static_assert rules them out */
				}
			}
			else /* ValueKind::state_char */
			{
				const auto value = parse_state_char(optarg);
				if (!value)
				{
					return fail(m_banner, raw_token != nullptr ? raw_token : spec.name,
						"value must be exactly one character");
				}
				switch (spec.slot)
				{
				case Slot::app_state_set:   m_app_state_to_set = *value; break;
				case Slot::app_state_query: m_app_state_to_query = *value; break;
				case Slot::fw_state_set:    m_fw_state_to_set = *value; break;
				case Slot::fw_state_query:  m_fw_state_to_query = *value; break;
				case Slot::none:
				case Slot::debug:
				case Slot::serial:
				case Slot::detach:
				case Slot::cancel_session_id:
					break; /* not state targets — the static_assert rules them out */
				}
			}
		}

		if (spec.command == Command::install_update) install_update_set = true;

		++distinct_actions;
		matched_command = spec.command;
	}

	/* A literal "--" makes getopt_long return -1 immediately, regardless of
	 * RETURN_IN_ORDER: it does not keep returning code 1 for what follows.
	 * optind is left pointing at the first token after it, so the tail still
	 * has to go through the same single-operand rule. */
	for (int i = optind; i < argc; ++i)
	{
		const char *token_ptr = argv_copy[static_cast<std::size_t>(i)];
		const std::string token = (token_ptr != nullptr) ? token_ptr : "";
		if (!take_operand(token))
			return fail(m_banner, token.c_str(), "an install path may be given at most once");
	}

	/* Full argv is now known error-free — honour a --help seen anywhere,
	 * ahead of classify(). */
	if (help_requested)
	{
		result.kind = ParseResult::Kind::help;
		return result;
	}

	RawFlags flags;
	flags.install_update_set = install_update_set;
	flags.install_path_set   = has_operand;
	flags.install_path       = install_path;
	flags.detach             = detach;

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
	/* The one rendering of an option's name and value, so the synopsis and the
	 * description list below cannot spell the same flag differently. */
	std::string option_label(const OptionSpec &spec)
	{
		std::string label = "--";
		label += spec.name;
		if (spec.value_name != nullptr)
		{
			label += " <";
			label += spec.value_name;
			label += ">";
		}
		return label;
	}

	std::string synopsis_token(const OptionSpec &spec)
	{
		if (spec.role == Role::help) return "[-h]";
		return "[" + option_label(spec) + "]";
	}

	std::vector<std::string> split_words(const std::string &text)
	{
		std::vector<std::string> words;
		std::size_t pos = 0;
		while (pos < text.size())
		{
			std::size_t next = text.find(' ', pos);
			if (next == std::string::npos) next = text.size();
			words.push_back(text.substr(pos, next - pos));
			pos = next + 1;
		}
		return words;
	}

	/* Plain whitespace word-wrap without <sstream>/<iostream>, which this code
	 * does not use. The first line follows first_prefix, the rest cont_prefix;
	 * no trailing newline, the caller owns the spacing. */
	std::string wrap_tokens(const std::vector<std::string> &tokens, const std::string &first_prefix,
		const std::string &cont_prefix, std::size_t width)
	{
		std::string out;
		std::string line = first_prefix;
		bool at_line_start = true;

		for (const auto &tok : tokens)
		{
			const std::size_t needed = (at_line_start ? 0 : 1) + tok.size();
			if (!at_line_start && line.size() + needed > width)
			{
				out += line;
				out += "\n";
				line = cont_prefix;
				at_line_start = true;
			}
			if (!at_line_start) line += " ";
			line += tok;
			at_line_start = false;
		}
		out += line;
		return out;
	}

	std::string wrap_paragraph(const std::string &text, const std::string &indent, std::size_t width)
	{
		return wrap_tokens(split_words(text), indent, indent, width) + "\n";
	}
}

std::string CliArgs::usage_text()
{
	constexpr std::size_t kWidth = 79;
	const std::string kIndent = "     ";

	std::string text;
	text.reserve(2048);

	text += "\nUSAGE:\n\n";

	std::vector<std::string> tokens;
	tokens.reserve(kOptionCount + 2);
	for (const auto &spec : kOptions) tokens.push_back(synopsis_token(spec));
	tokens.emplace_back("[--]");
	tokens.emplace_back("[" + std::string(kInstallPathName) + "]");

	const std::string first_prefix = "   fs-updater ";
	text += wrap_tokens(tokens, first_prefix, std::string(first_prefix.size(), ' '), kWidth);
	text += "\n\n";

	text += "Where:\n\n";

	for (const auto &spec : kOptions)
	{
		text += (spec.role == Role::help) ? "   -h, --help" : "   " + option_label(spec);
		text += "\n";
		text += wrap_paragraph(spec.help, kIndent, kWidth);
		text += "\n";
	}

	text += "   --\n";
	text += wrap_paragraph(
		"Ends option parsing: every token after this one is the install path, "
		"even one that starts with '-'.",
		kIndent, kWidth);
	text += "\n";

	text += "   ";
	text += kInstallPathName;
	text += " <";
	text += kInstallPathTypeDesc;
	text += ">\n";
	text += wrap_paragraph(
		"Optional local update bundle path for --install_update"
		". The state-taking flags above (--set_app_state_bad, "
		"--is_app_state_bad, --set_fw_state_bad, --is_fw_state_bad) accept "
		"any single character at this layer; the accepted values (A or B) "
		"are checked by the command, not the parser.",
		kIndent, kWidth);

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
