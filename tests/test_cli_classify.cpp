/* Native unit tests for the HW-free install-surface classifier.
 *
 * classify() owns exactly the install-surface decisions that must hold without
 * touching the parser or the FSUpdate/HW path: the bare-path rule (a bare positional path is
 * only legal together with --install_update), the --detach guard (detach needs
 * a local path), and the local-vs-cloud selection of --install_update [<path>].
 * Mutual exclusion across all actions is the dispatch loop's job, not here. */
#include "cli/cli_classify.h"

#include <gtest/gtest.h>

using cli::InstallMode;
using cli::ParseOutcome;
using cli::RawFlags;

/* --- bare-path rule: a positional path without --install_update is rejected,
 * so `fs-updater --commit_update /tmp/x` and bare `fs-updater /tmp/x` keep
 * failing cleanly instead of being silently swallowed by the global positional. */
TEST(Classify, PositionalWithoutInstallUpdateIsParseError)
{
    RawFlags f{};
    f.install_path_set   = true;        // a positional token was given
    f.install_path       = "/tmp/x";
    f.install_update_set = false;       // ...but --install_update was not

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

/* --- Unified --install_update [<path>] local/cloud selection --- */
TEST(Classify, InstallUpdateWithPathSelectsLocal)
{
    RawFlags f{};
    f.install_update_set = true;
    f.install_path_set   = true;
    f.install_path       = "/tmp/bundle.fs";

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::ok);
    EXPECT_EQ(out.mode, InstallMode::local);
    EXPECT_EQ(out.install_path, "/tmp/bundle.fs");
}

TEST(Classify, InstallUpdateWithoutPathSelectsCloud)
{
    RawFlags f{};
    f.install_update_set = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::ok);
    EXPECT_EQ(out.mode, InstallMode::cloud);
    EXPECT_TRUE(out.install_path.empty());
}

/* --- --detach is carried for a local install --- */
TEST(Classify, DetachWithLocalPathIsCarried)
{
    RawFlags f{};
    f.install_update_set = true;
    f.install_path_set   = true;
    f.install_path       = "/tmp/bundle.fs";
    f.detach             = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::ok);
    EXPECT_EQ(out.mode, InstallMode::local);
    EXPECT_TRUE(out.detach);
}

/* --- --detach without a path is rejected: the cloud-advance form is
 * already non-blocking, so `--install_update --detach` (no path) is meaningless
 * and must not be silently ignored. */
TEST(Classify, DetachWithoutPathIsParseError)
{
    RawFlags f{};
    f.install_update_set = true;        // cloud mode (no path)
    f.detach             = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

TEST(Classify, DetachAloneIsParseError)
{
    RawFlags f{};
    f.detach = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

TEST(Classify, NoFlagsIsOkWithNoMode)
{
    const RawFlags f{};

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::ok);
    EXPECT_EQ(out.mode, InstallMode::none);
}

/* --- Empty-string path: `fs-updater --install_update ""` must be rejected at
 * the parse seam, not trickle into realpath("") and a misleading I/O error. */
TEST(Classify, EmptyPathWithInstallUpdateIsParseError)
{
    RawFlags f{};
    f.install_update_set = true;
    f.install_path_set   = true;
    f.install_path       = "";

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

/* An empty positional without --install_update stays a bare-path rejection. */
TEST(Classify, EmptyPathWithoutInstallUpdateIsParseError)
{
    RawFlags f{};
    f.install_path_set = true;
    f.install_path     = "";

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

/* --- dispatch_verdict: pins the action-count contract of the dispatch loop.
 * Modifiers (--debug/--serial/--detach) never enter the action table, so an
 * invocation like `fs-updater --serial` is the 0-action case: print the
 * version, run nothing, return code stays 0 — not a combo error. */
TEST(DispatchVerdict, NoActionPrintsVersionOnly)
{
    EXPECT_EQ(cli::dispatch_verdict(0), cli::DispatchVerdict::version_only);
}

TEST(DispatchVerdict, ExactlyOneActionRuns)
{
    EXPECT_EQ(cli::dispatch_verdict(1), cli::DispatchVerdict::run);
}

TEST(DispatchVerdict, MultipleActionsAreAComboError)
{
    EXPECT_EQ(cli::dispatch_verdict(2), cli::DispatchVerdict::combo_error);
    EXPECT_EQ(cli::dispatch_verdict(19), cli::DispatchVerdict::combo_error);
}

/* --- install_terminal_code: terminal verdict → return code by type --- */
TEST(InstallTerminalCode, KnownTypesMapToTheirFamily)
{
    EXPECT_EQ(cli::install_terminal_code(true,  "fw"),     0);
    EXPECT_EQ(cli::install_terminal_code(false, "fw"),     3);
    EXPECT_EQ(cli::install_terminal_code(true,  "app"),    4);
    EXPECT_EQ(cli::install_terminal_code(false, "app"),    7);
    EXPECT_EQ(cli::install_terminal_code(true,  "fw+app"), 8);
    EXPECT_EQ(cli::install_terminal_code(false, "fw+app"), 11);
}

/* An empty/unknown type must NOT be reported as the firmware family (which
 * would map an app success to 0 instead of 4). It yields the type-agnostic
 * install-family terminal code instead. */
TEST(InstallTerminalCode, EmptyOrUnknownTypeIsTypeAgnosticNotFirmware)
{
    EXPECT_EQ(cli::install_terminal_code(true,  ""),       48); /* not 0 */
    EXPECT_EQ(cli::install_terminal_code(false, ""),       49); /* not 3 */
    EXPECT_EQ(cli::install_terminal_code(true,  "bogus"),  48);
    EXPECT_EQ(cli::install_terminal_code(false, "bogus"),  49);
}
