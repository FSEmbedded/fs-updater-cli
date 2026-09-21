/* Pin for the getopt_long parser's low-level scanners: strict
 * decimal parsing, single-character state syntax, and long-option exact
 * matching (no abbreviation, "--flag=value" form allowed). */
#include "cli/arg_scan.h"

#include <gtest/gtest.h>

TEST(ParseU32, RejectsSignHexTrailingJunkAndOverflow)
{
	EXPECT_FALSE(cli::parse_u32(""));
	EXPECT_FALSE(cli::parse_u32("-1"));
	EXPECT_FALSE(cli::parse_u32("+5"));
	EXPECT_FALSE(cli::parse_u32("0x10"));
	EXPECT_FALSE(cli::parse_u32("5x"));
	EXPECT_FALSE(cli::parse_u32("abc"));
	EXPECT_FALSE(cli::parse_u32(" 5"));
	EXPECT_FALSE(cli::parse_u32("5 "));
	EXPECT_FALSE(cli::parse_u32("99999999999")); /* > UINT32_MAX, fits unsigned long */
	EXPECT_FALSE(cli::parse_u32("4294967296"));  /* UINT32_MAX + 1 */
}

TEST(ParseU32, AcceptsZeroAndUint32Max)
{
	EXPECT_EQ(cli::parse_u32("0"), 0u);
	EXPECT_EQ(cli::parse_u32("42"), 42u);
	EXPECT_EQ(cli::parse_u32("4294967295"), 4294967295u);
}

TEST(ParseStateChar, RejectsEmptyAndMultiChar)
{
	EXPECT_FALSE(cli::parse_state_char(""));
	EXPECT_FALSE(cli::parse_state_char("AB"));
	EXPECT_FALSE(cli::parse_state_char("toolong"));
}

TEST(ParseStateChar, AcceptsSingleChar)
{
	/* No A/B constraint at the parser layer — any single character is
	 * syntactically valid here; the handler owns the semantic check (rc 53). */
	EXPECT_EQ(cli::parse_state_char("A"), 'A');
	EXPECT_EQ(cli::parse_state_char("Z"), 'Z');
}

TEST(ExactMatch, RejectsAbbreviationAcceptsEqualsForm)
{
	EXPECT_TRUE(cli::exact_long_match("--debug", "debug"));
	EXPECT_TRUE(cli::exact_long_match("--debug=x", "debug"));
	EXPECT_FALSE(cli::exact_long_match("--deb", "debug"));      /* abbreviation */
	/* Rejected by the terminator check, not by the prefix compare: "debugger"
	 * does start with "debug". Drop that check and this case starts matching. */
	EXPECT_FALSE(cli::exact_long_match("--debugger", "debug"));
}
