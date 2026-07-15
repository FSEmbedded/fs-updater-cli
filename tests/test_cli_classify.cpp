/* Native unit tests for the HW-free install-surface classifier.
 *
 * classify() owns exactly the install-surface decisions that must hold without
 * touching the parser or the FSUpdate/HW path: the bare-path rule (a bare positional path
 * is only legal together with --install_update), the --detach guard (detach
 * needs a local path), and the local-vs-cloud selection of
 * --install_update [<path>]. Mutual exclusion across all actions is the
 * dispatch loop's job, not here. */
#include "cli/cli_classify.h"

#include <gtest/gtest.h>

#include <cerrno>

using cli::CallError;
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
 * the parse seam, not trickle into realpath("") and a misleading I/O error. It
 * is a bad_path (→ file-not-found 61), distinct from an argument-combo error
 * (65): a script keying on 61 for a bad bundle path must still get 61. */
TEST(Classify, EmptyPathWithInstallUpdateIsBadPath)
{
    RawFlags f{};
    f.install_update_set = true;
    f.install_path_set   = true;
    f.install_path       = "";

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::bad_path);
    EXPECT_FALSE(out.error.empty());
}

/* An empty positional without --install_update stays a bare-path rejection: the
 * order matters — the bare-path rule fires before the empty-path check, so the
 * outcome is parse_error with the bare-path message, not bad_path. */
TEST(Classify, EmptyPathWithoutInstallUpdateIsGuardB)
{
    RawFlags f{};
    f.install_path_set = true;
    f.install_path     = "";

    const ParseOutcome out = cli::classify(f);

    EXPECT_EQ(out.kind, ParseOutcome::Kind::parse_error);
    EXPECT_EQ(out.error, "a path argument is only valid with --install_update");
}

/* --- dispatch_verdict: pins only the count→verdict arithmetic (0→version,
 * 1→run, N→combo). WHICH flags count as actions is decided by the caller
 * (parse_input builds the action table); this function never sees the flags,
 * so these cases pin the mapping, not table membership. */
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

/* --- classify_call_error: the busy-name string is the only reliable busy
 * detector over the bus and, before this seam existed, had no executable pin
 * on the client side. The exact literals must match service.cpp. --- */

TEST(ClassifyCallError, SuccessIsNone)
{
    EXPECT_EQ(cli::classify_call_error(0,  nullptr), CallError::none);
    EXPECT_EQ(cli::classify_call_error(1,  nullptr), CallError::none);
}

TEST(ClassifyCallError, NamesAreAuthoritative)
{
    EXPECT_EQ(cli::classify_call_error(-EIO, "de.fsembedded.fsupdate1.Error.Busy"),
              CallError::busy);
    EXPECT_EQ(cli::classify_call_error(-EIO, "org.freedesktop.DBus.Error.AccessDenied"),
              CallError::denied);
    EXPECT_EQ(cli::classify_call_error(-EIO, "de.fsembedded.fsupdate1.Error.NoUpdater"),
              CallError::no_updater);
}

/* EACCES-collision pin: a generic Failed error maps to -EACCES on the wire, so
 * classifying by errno would call it "denied". The name must win → other, and
 * -EACCES must NEVER be treated as denied. */
TEST(ClassifyCallError, FailedNameIsOtherNotDenied)
{
    EXPECT_EQ(cli::classify_call_error(-EACCES, "org.freedesktop.DBus.Error.Failed"),
              CallError::other);
    EXPECT_EQ(cli::classify_call_error(-EACCES, nullptr), CallError::other);
}

/* errno fallback: a same-host direct peer with no known name still yields the
 * raw -EBUSY / -ENOSYS (an unregistered name maps to -EIO, hence other). */
TEST(ClassifyCallError, ErrnoFallbackWhenNameAbsent)
{
    EXPECT_EQ(cli::classify_call_error(-EBUSY,  nullptr), CallError::busy);
    EXPECT_EQ(cli::classify_call_error(-ENOSYS, nullptr), CallError::no_updater);
    EXPECT_EQ(cli::classify_call_error(-EIO,    nullptr), CallError::other);
}

/* --- install_start_error_code: pins the exit codes for each CallError
 * (66/67 for busy/denied, 47/49 for the accepted/failed defaults). --- */
TEST(InstallStartErrorCode, MapsEachCallError)
{
    EXPECT_EQ(cli::install_start_error_code(CallError::busy),       66); /* INSTALL_BUSY */
    EXPECT_EQ(cli::install_start_error_code(CallError::denied),     67); /* PERMISSION_DENIED */
    EXPECT_EQ(cli::install_start_error_code(CallError::none),       47); /* accepted; poll */
    EXPECT_EQ(cli::install_start_error_code(CallError::no_updater), 49); /* install failed */
    EXPECT_EQ(cli::install_start_error_code(CallError::other),      49);
}

/* --- is_option_like: guarded the previous parser's global install
 * positional against absorbing an unknown flag as the path — that parser's
 * unlabelled positional argument type accepted any non-blank token, so
 * without this predicate `fs-updater --bogus` parsed "successfully" with
 * install_path="--bogus" and was reported as a bare-path violation ("a path
 * argument is only valid with --install_update", rc 65) instead of an unknown
 * option. The getopt_long parser makes this distinction
 * itself, so is_option_like() is no longer called from the parsing path; kept
 * here for its own coverage (cli_classify.* stays byte-identical across the
 * cut). */
TEST(OptionLike, LeadingDashTokensAreOptionLike)
{
    EXPECT_TRUE(cli::is_option_like("--bogus"));
    EXPECT_TRUE(cli::is_option_like("--definitely-not-a-real-flag"));
    EXPECT_TRUE(cli::is_option_like("--install_up"));
    EXPECT_TRUE(cli::is_option_like("--cancel_install=5"));
    EXPECT_TRUE(cli::is_option_like("-h"));
    EXPECT_TRUE(cli::is_option_like("-"));
}

TEST(OptionLike, PathsAndValuesAreNotOptionLike)
{
    EXPECT_FALSE(cli::is_option_like("/tmp/update.raucb"));
    EXPECT_FALSE(cli::is_option_like("update.raucb"));
    EXPECT_FALSE(cli::is_option_like("./-weird-name"));  /* the documented escape for a dashed path */
    EXPECT_FALSE(cli::is_option_like(""));               /* empty stays classify()'s bad_path case */
}
