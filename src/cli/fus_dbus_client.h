#pragma once

#include <cstdint>
#include <string>

namespace fus_dbus {

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
uint32_t          install_local(const std::string& path);  /* 0 on error */
bool              cancel_install(uint32_t session_id);
int               get_install_progress();    /* 0-100, or -1 on error */
std::string       get_install_state();       /* "idle"|"in_progress"|"finished"|"failed"|"" */
std::string       get_update_type();         /* "fw"|"app"|"fw+app"|"" */
uint32_t          get_session_id();          /* 0 on error */
bool              apply(bool& reboot_needed);

/* Terminal verdict of a blocking install wait. */
struct InstallResult {
    bool        reached_terminal = false; /* false = idle-timeout / bus error */
    bool        success          = false; /* InstallCompleted success flag */
    std::string type;                     /* "fw"|"app"|"fw+app" at terminal */
};

/* Block until the InstallCompleted signal for session_id arrives, rendering
 * live progress via on_progress (called only when the percentage changes).
 * Bounded by an idle watchdog: if neither progress nor completion is seen for
 * idle_timeout_ms, give up with reached_terminal == false (so a service that
 * dies mid-install cannot hang the CLI). Each forward step resets the watchdog,
 * so a long but progressing install is not cut off. */
InstallResult     wait_for_install(uint32_t session_id, int idle_timeout_ms,
                                   void (*on_progress)(int) = nullptr);

} // namespace fus_dbus
