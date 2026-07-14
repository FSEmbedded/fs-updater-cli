/* The parser's behavioural pin.
 *
 * These read tests/golden/*, rather than restating them, so the files stay the
 * single statement of the contract. That is the point: the parser is due to be
 * replaced, and the replacement is correct exactly when it still makes these
 * files pass. A table copied into this source could drift from the file the
 * reviewers of that cut will actually read.
 *
 * Everything here runs without a board: the argument surface does not depend
 * on the class that owns the handlers. */
#include "cli/cli_args.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	std::string read_file(const char *path)
	{
		std::ifstream in(path, std::ios::binary);
		EXPECT_TRUE(in.good()) << "cannot read " << path;
		std::ostringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	std::string trim(const std::string &s)
	{
		const auto b = s.find_first_not_of(" \t");
		if (b == std::string::npos) return "";
		const auto e = s.find_last_not_of(" \t");
		return s.substr(b, e - b + 1);
	}

	std::vector<std::string> split(const std::string &s, char sep)
	{
		std::vector<std::string> out;
		std::istringstream ss(s);
		std::string part;
		while (std::getline(ss, part, sep)) out.push_back(trim(part));
		return out;
	}

	/* Shared deliberately: the argument library refuses a second instance in
	 * the same process (see CliArgs). parse() resets between calls. */
	cli::CliArgs &parser()
	{
		static cli::CliArgs instance;
		return instance;
	}

	/* argv[0] matters: the usage block embeds basename(argv[0]), so the golden
	 * only matches under the name the binary actually ships as. */
	std::vector<std::string> argv_of(const std::string &args)
	{
		std::vector<std::string> v{"fs-updater"};
		std::istringstream ss(args);
		std::string tok;
		while (ss >> tok)
		{
			/* the corpus writes an empty argument as "" */
			if (tok == "\"\"") tok = "";
			v.push_back(tok);
		}
		return v;
	}

	cli::ParseResult::Kind kind_of(const std::string &s)
	{
		if (s == "ok")           return cli::ParseResult::Kind::ok;
		if (s == "help")         return cli::ParseResult::Kind::help;
		if (s == "version_only") return cli::ParseResult::Kind::version_only;
		if (s == "parse_error")  return cli::ParseResult::Kind::parse_error;
		if (s == "guard_error")  return cli::ParseResult::Kind::guard_error;
		if (s == "combo_error")  return cli::ParseResult::Kind::combo_error;
		ADD_FAILURE() << "unknown kind in corpus: " << s;
		return cli::ParseResult::Kind::parse_error;
	}

	std::string name_of(cli::Command c)
	{
		switch (c)
		{
		case cli::Command::none:                return "-";
		case cli::Command::commit_update:       return "commit_update";
		case cli::Command::update_reboot_state: return "update_reboot_state";
		case cli::Command::application_version: return "application_version";
		case cli::Command::firmware_version:    return "firmware_version";
		case cli::Command::print_version:       return "print_version";
		case cli::Command::is_update_available: return "is_update_available";
		case cli::Command::download_update:     return "download_update";
		case cli::Command::download_progress:   return "download_progress";
		case cli::Command::install_update:      return "install_update";
		case cli::Command::install_progress:    return "install_progress";
		case cli::Command::cancel_install:      return "cancel_install";
		case cli::Command::apply_update:        return "apply_update";
		case cli::Command::rollback_update:     return "rollback_update";
		case cli::Command::switch_fw_slot:      return "switch_fw_slot";
		case cli::Command::switch_app_slot:     return "switch_app_slot";
		case cli::Command::set_app_state_bad:   return "set_app_state_bad";
		case cli::Command::is_app_state_bad:    return "is_app_state_bad";
		case cli::Command::set_fw_state_bad:    return "set_fw_state_bad";
		case cli::Command::is_fw_state_bad:     return "is_fw_state_bad";
		}
		return "?";
	}
}

/* Every corpus row, checked. A row the parser disagrees with is either a
 * regression or a decision someone has to make on purpose. */
TEST(ArgvCorpus, EveryRowHoldsAsWritten)
{
	const std::string text = read_file(FSUP_CORPUS_FILE);
	std::istringstream lines(text);
	std::string line;
	int checked = 0;

	while (std::getline(lines, line))
	{
		const std::string row = trim(line);
		if (row.empty() || row[0] == '#') continue;

		const std::vector<std::string> col = split(row, '|');
		ASSERT_EQ(col.size(), 6u) << "malformed corpus row: " << row;

		const std::vector<std::string> args = argv_of(col[0]);
		std::vector<const char *> argv;
		argv.reserve(args.size());
		for (const auto &a : args) argv.push_back(a.c_str());

		const cli::ParseResult got =
			parser().parse(static_cast<int>(argv.size()), argv.data());

		EXPECT_EQ(got.kind, kind_of(col[1])) << "kind for: " << col[0];
		EXPECT_EQ(name_of(got.command), col[2]) << "command for: " << col[0];
		EXPECT_EQ(std::to_string(got.action_count), col[3]) << "actions for: " << col[0];
		if (col[4] != "--")
		{
			EXPECT_EQ(std::to_string(got.rc), col[4]) << "rc for: " << col[0];
		}
		++checked;
	}

	/* Guards against a corpus that silently stopped being read. */
	EXPECT_GT(checked, 30) << "corpus looks truncated";
}

/* The usage text is produced by the argument library, not by us. The golden was
 * taken from a target binary, so matching it here also shows the native build
 * and the shipped one render it identically. */
TEST(UsageText, MatchesTheGoldenByteForByte)
{
	const char *argv[] = {"fs-updater", "--help"};

	const cli::ParseResult got = parser().parse(2, argv);
	ASSERT_EQ(got.kind, cli::ParseResult::Kind::help);

	EXPECT_EQ(parser().usage_text(), read_file(FSUP_GOLDEN_HELP));
}
