#include "fus_dbus_client.h"

#include <systemd/sd-bus.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

static constexpr const char* BUS_NAME  = "de.fsembedded.fsupdate1";
static constexpr const char* OBJ_PATH  = "/de/fsembedded/fsupdate1";
static constexpr const char* INTERFACE = "de.fsembedded.fsupdate1";

namespace fus_dbus {

namespace {

struct BusGuard {
    sd_bus* bus = nullptr;
    ~BusGuard() { if (bus) sd_bus_unref(bus); }
};

using MsgPtr = std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)>;

MsgPtr wrap_msg(sd_bus_message* m) {
    return MsgPtr(m, sd_bus_message_unref);
}

bool open_bus(BusGuard& g) {
    return sd_bus_open_system(&g.bus) >= 0;
}

std::string read_string_property(const char* prop)
{
    BusGuard g;
    if (!open_bus(g)) return {};

    sd_bus_error err   = SD_BUS_ERROR_NULL;
    char*        raw   = nullptr;
    std::string  result;

    if (sd_bus_get_property_string(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                                   prop, &err, &raw) >= 0 && raw) {
        /* sd-bus returns a heap string the caller must release; own it
         * with a guard so it frees on every exit path. */
        const std::unique_ptr<char, decltype(&std::free)> owned(raw, std::free);
        result = raw;
    }

    sd_bus_error_free(&err);
    return result;
}

} // namespace

CheckUpdateResult check_update_available()
{
    BusGuard g;
    if (!open_bus(g)) return {};

    sd_bus_error err     = SD_BUS_ERROR_NULL;
    sd_bus_message* raw  = nullptr;
    CheckUpdateResult result;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "CheckUpdateAvailable", &err, &raw, nullptr);
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        int         available = 0;
        const char* type      = nullptr;
        const char* version   = nullptr;
        uint64_t    size      = 0;
        if (sd_bus_message_read(reply.get(), "bsst",
                                &available, &type, &version, &size) >= 0) {
            result.available = available != 0;
            if (type)    result.type    = type;
            if (version) result.version = version;
            result.size = size;
        }
    }

    sd_bus_error_free(&err);
    return result;
}

uint32_t start_download(const std::string& type, const std::string& version, uint64_t size)
{
    BusGuard g;
    if (!open_bus(g)) return 0;

    sd_bus_error    err  = SD_BUS_ERROR_NULL;
    sd_bus_message* raw  = nullptr;
    uint32_t        sid  = 0;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "StartDownload", &err, &raw,
                               "sst", type.c_str(), version.c_str(), size);
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        sd_bus_message_read(reply.get(), "u", &sid);
    }

    sd_bus_error_free(&err);
    return sid;
}

int get_download_progress()
{
    BusGuard g;
    if (!open_bus(g)) return -1;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;
    int32_t         pct = -1;

    int r = sd_bus_get_property(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                                "DownloadProgress", &err, &raw, "i");
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        sd_bus_message_read(reply.get(), "i", &pct);
    }

    sd_bus_error_free(&err);
    return static_cast<int>(pct);
}

std::string get_download_state()
{
    return read_string_property("DownloadState");
}

bool start_install(uint32_t session_id, const std::string& type)
{
    BusGuard g;
    if (!open_bus(g)) return false;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "StartInstall", &err, &raw,
                               "us", session_id, type.c_str());
    if (r >= 0) sd_bus_message_unref(raw);

    sd_bus_error_free(&err);
    return r >= 0;
}

uint32_t install_local(const std::string& path)
{
    BusGuard g;
    if (!open_bus(g)) return 0;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;
    uint32_t        sid = 0;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "InstallLocal", &err, &raw,
                               "s", path.c_str());
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        sd_bus_message_read(reply.get(), "u", &sid);
    }

    sd_bus_error_free(&err);
    return sid;
}

bool cancel_install(uint32_t session_id)
{
    BusGuard g;
    if (!open_bus(g)) return false;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "CancelInstall", &err, &raw,
                               "u", session_id);
    if (r >= 0) sd_bus_message_unref(raw);

    sd_bus_error_free(&err);
    return r >= 0;
}

int get_install_progress()
{
    BusGuard g;
    if (!open_bus(g)) return -1;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;
    int32_t         pct = -1;

    int r = sd_bus_get_property(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                                "InstallProgress", &err, &raw, "i");
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        sd_bus_message_read(reply.get(), "i", &pct);
    }

    sd_bus_error_free(&err);
    return static_cast<int>(pct);
}

std::string get_install_state()
{
    return read_string_property("InstallState");
}

std::string get_update_type()
{
    return read_string_property("UpdateType");
}

uint32_t get_session_id()
{
    BusGuard g;
    if (!open_bus(g)) return 0;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;
    uint32_t        sid = 0;

    int r = sd_bus_get_property(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                                "SessionId", &err, &raw, "u");
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        sd_bus_message_read(reply.get(), "u", &sid);
    }

    sd_bus_error_free(&err);
    return sid;
}

bool apply(bool& reboot_needed)
{
    BusGuard g;
    if (!open_bus(g)) return false;

    sd_bus_error    err = SD_BUS_ERROR_NULL;
    sd_bus_message* raw = nullptr;

    int r = sd_bus_call_method(g.bus, BUS_NAME, OBJ_PATH, INTERFACE,
                               "Apply", &err, &raw, nullptr);
    if (r >= 0) {
        auto reply = wrap_msg(raw);
        int rb = 0;
        sd_bus_message_read(reply.get(), "b", &rb);
        reboot_needed = rb != 0;
    }

    sd_bus_error_free(&err);
    return r >= 0;
}

namespace {

struct CompletionState {
    uint32_t want_sid = 0;
    bool     fired    = false;
    bool     success  = false;
};

/* sd-bus signal handler for InstallCompleted("ubs" = session_id, success,
 * message). Non-capturing so it converts to a plain function pointer. */
int on_install_completed(sd_bus_message* m, void* userdata, sd_bus_error* /*err*/)
{
    auto* st = static_cast<CompletionState*>(userdata);
    uint32_t    sid = 0;
    int         ok  = 0;
    const char* msg = nullptr;
    if (sd_bus_message_read(m, "ubs", &sid, &ok, &msg) >= 0 && sid == st->want_sid) {
        st->fired   = true;
        st->success = ok != 0;
    }
    return 0;
}

} // namespace

InstallResult wait_for_install(uint32_t session_id, int idle_timeout_ms,
                               void (*on_progress)(int))
{
    InstallResult res;

    BusGuard g;
    if (!open_bus(g)) return res;

    CompletionState st;
    st.want_sid = session_id;

    sd_bus_slot* slot = nullptr;
    if (sd_bus_match_signal(g.bus, &slot, BUS_NAME, OBJ_PATH, INTERFACE,
                            "InstallCompleted", on_install_completed, &st) < 0) {
        return res;
    }
    const std::unique_ptr<sd_bus_slot, decltype(&sd_bus_slot_unref)>
        slot_guard(slot, sd_bus_slot_unref);

    /* Race guard: the worker may have reached a terminal state between
     * install_local() returning and the match being installed. */
    {
        const std::string s0 = get_install_state();
        if (s0 == "finished" || s0 == "failed") {
            res.reached_terminal = true;
            res.success          = (s0 == "finished");
            res.type             = get_update_type();
            return res;
        }
    }

    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::milliseconds(idle_timeout_ms);
    int  last_pct = -1;

    while (!st.fired) {
        const int r = sd_bus_process(g.bus, nullptr);
        if (r < 0) break;
        if (r > 0) continue; /* handled an event — re-check fired / drain queue */

        const int pct = get_install_progress();
        if (pct >= 0 && pct != last_pct) {
            last_pct = pct;
            if (on_progress != nullptr) on_progress(pct);
            deadline = clock::now() + std::chrono::milliseconds(idle_timeout_ms);
        }
        if (clock::now() >= deadline) break; /* idle watchdog — no hang */

        sd_bus_wait(g.bus, 250000 /* us */);
    }

    if (st.fired) {
        res.reached_terminal = true;
        res.success          = st.success;
        res.type             = get_update_type();
    }
    return res;
}

} // namespace fus_dbus
