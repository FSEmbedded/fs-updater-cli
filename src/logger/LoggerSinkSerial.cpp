#include "LoggerSinkSerial.h"
#include <fs_update_framework/logger/LoggerEntry.h>
#include <chrono>
#include <ctime>
#include <string>
#include <array>

namespace logger
{
    std::mutex LoggerSinkSerial::serial_mutex;

    LoggerSinkSerial::LoggerSinkSerial(logger::logLevel level,
                                       std::shared_ptr<SynchronizedSerial> port)
        : log_level(level), serial_port(std::move(port))
    {
    }

    void LoggerSinkSerial::setLogEntry(const std::shared_ptr<logger::LogEntry>& entry) noexcept
    {
        if (!entry)
            return;

        const auto entry_level = entry->getLogLevel();
        bool should_output = false;
        const char* level_prefix = nullptr;

        switch (entry_level)
        {
            case logLevel::DEBUG:
                if (log_level == logLevel::DEBUG)
                {
                    level_prefix = "DEBUG";
                    should_output = true;
                }
                break;
            case logLevel::WARNING:
                if (log_level == logLevel::DEBUG || log_level == logLevel::WARNING)
                {
                    level_prefix = "WARNING";
                    should_output = true;
                }
                break;
            case logLevel::ERROR:
                level_prefix = "ERROR";
                should_output = true;
                break;
            default:
                return;
        }

        if (!should_output)
            return;

        const auto time_t_val = std::chrono::system_clock::to_time_t(entry->getTimepoint());
        std::tm time_buf{};
        localtime_r(&time_t_val, &time_buf);

        std::array<char, 20> time_str{};
        std::strftime(time_str.data(), time_str.size(), "%Y-%m-%d %H:%M:%S", &time_buf);

        std::string out;
        out.reserve(128);
        out.append(level_prefix);
        out.append(": [");
        out.append(time_str.data());
        out.append("] - ");
        out.append(entry->getLogDomain());
        out.append(": ");
        out.append(entry->getLogMessage());

        std::lock_guard<std::mutex> lock(serial_mutex);
        serial_port->write(out);
    }
}
