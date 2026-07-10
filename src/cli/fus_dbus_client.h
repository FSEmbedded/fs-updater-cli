#pragma once

#include <cstdint>
#include <string>

namespace fus_dbus {

/* Why a privileged D-Bus call failed, derived from the sd_bus error name +
 * errno, so the CLI can return a distinct exit code / message instead of
 * collapsing every failure into one sentinel. */
enum class CallError {
    none,        /* the call succeeded */
    busy,        /* -EBUSY / de.fsembedded.fsupdate1.Error.Busy */
    denied,      /* org.freedesktop.DBus.Error.AccessDenied (polkit / bus policy) */
    no_updater,  /* -ENOSYS / updater not available on the service */
    other        /* anything else (bad args, I/O, bus not reachable, …) */
};

struct CheckUpdateResult {
    bool        available = false;
    std::string type;
    std::string version;
    uint64_t    size = 0;
};

CheckUpdateResult check_update_available();
uint32_t          start_download(const std::string& type, const std::string& version, uint64_t size);
int               get_download_progress();   /* 0-100, or -1 on error */
std::string       get_download_state();      /* "idle"|"in_progress"|"finished"|"failed"|"" */
bool              start_install(uint32_t session_id, const std::string& type);
/* 0 on error; when err_out is non-null it receives why the call failed
 * (busy / denied / no_updater / other) so the CLI can map a distinct rc. */
uint32_t          install_local(const std::string& path, CallError* err_out = nullptr);
bool              cancel_install(uint32_t session_id);
int               get_install_progress();    /* 0-100, or -1 on error */
std::string       get_install_state();       /* "idle"|"in_progress"|"finished"|"failed"|"" */
std::string       get_update_type();         /* "fw"|"app"|"fw+app"|"" */
uint32_t          get_session_id();          /* 0 on error */
bool              apply(bool& reboot_needed);

/* Outcome of a blocking install wait. */
struct InstallResult {
    enum class Status {
        completed,     /* a terminal verdict was observed (success/type valid) */
        timed_out,     /* watched, but no terminal within the no-progress window */
        observe_error  /* could not observe the install (bus / match failure) */
    };
    Status      status  = Status::observe_error;
    bool        success = false; /* valid iff status == completed */
    std::string type;            /* "fw"|"app"|"fw+app", valid iff completed */
};

/* Block until the InstallCompleted signal (or a terminal InstallState) for
 * session_id is observed, rendering live progress via on_progress (called only
 * when the percentage changes). Progress/state are read on the already-open
 * connection — no per-poll reconnect. The watchdog resets on forward progress;
 * if no progress and no terminal is seen for idle_timeout_ms it returns
 * timed_out (the install may still be running — the caller should not treat
 * this as a terminal failure). A bus/match failure returns observe_error. */
[[nodiscard]] InstallResult wait_for_install(uint32_t session_id, int idle_timeout_ms,
                                             void (*on_progress)(int) = nullptr);

} // namespace fus_dbus
