/* Native unit tests for the HW-free install-surface classifier.
 *
 * classify() owns exactly the install-surface decisions that must hold without
 * touching the parser or the FSUpdate/HW path: the bare-path rule (a bare positional path is
 * only legal together with --install_update), the local-vs-cloud selection of
 * the unified --install_update [<path>] flag, mutual exclusion of actions, and
 * the modifier pass-through (--detach/--serial/--debug). */
#include "cli/cli_classify.h"

#include <gtest/gtest.h>

using cli::ActionId;
using cli::InstallMode;
using cli::ParseOutcome;
using cli::RawFlags;

/* --- bare-path rule: a positional path without --install_update is rejected,
 * so `fs-updater --commit_update /tmp/x` and bare `fs-updater /tmp/x` keep
 * failing cleanly instead of being silently swallowed by the global positional. */
TEST(Classify, PositionalWithoutInstallUpdateIsParseError)
{
    RawFlags f{};
    f.install_path_set = true;          // a positional token was given
    f.install_path     = "/tmp/x";
    f.install_update_set = false;       // ...but --install_update was not

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

TEST(Classify, PositionalWithOtherActionIsParseError)
{
    RawFlags f{};
    f.install_path_set   = true;
    f.install_path       = "/tmp/x";
    f.commit_update_set  = true;        // an unrelated action grabbed the positional

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

    EXPECT_EQ(out.kind, ParseOutcome::Kind::action);
    EXPECT_EQ(out.action, ActionId::install_update);
    EXPECT_EQ(out.mode, InstallMode::local);
    EXPECT_EQ(out.install_path, "/tmp/bundle.fs");
}

TEST(Classify, InstallUpdateWithoutPathSelectsCloud)
{
    RawFlags f{};
    f.install_update_set = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::action);
    EXPECT_EQ(out.action, ActionId::install_update);
    EXPECT_EQ(out.mode, InstallMode::cloud);
    EXPECT_TRUE(out.install_path.empty());
}

/* --- Mutual exclusion of actions (FR-CLI-09) --- */
TEST(Classify, TwoActionsAreIncompatible)
{
    RawFlags f{};
    f.install_update_set = true;
    f.commit_update_set  = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::incompatible_combo);
}

/* --- Modifiers ride along, are not actions --- */
TEST(Classify, DetachAndSerialAndDebugAreModifiersNotActions)
{
    RawFlags f{};
    f.install_update_set = true;
    f.install_path_set   = true;
    f.install_path       = "/tmp/bundle.fs";
    f.detach = true;
    f.serial = true;
    f.debug  = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::action);
    EXPECT_EQ(out.action, ActionId::install_update);
    EXPECT_TRUE(out.detach);
    EXPECT_TRUE(out.serial);
    EXPECT_TRUE(out.debug);
}

TEST(Classify, DetachWithoutInstallUpdateIsParseError)
{
    /* --detach only qualifies an install; alone it is meaningless. */
    RawFlags f{};
    f.detach = true;

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
}

TEST(Classify, NoFlagsIsNoAction)
{
    const RawFlags f{};

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::no_action);
}
