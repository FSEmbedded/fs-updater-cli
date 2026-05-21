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

} // namespace fus_dbus
