#include "LoggerSinkSerial.h"
#include <fs_update_framework/logger/LoggerEntry.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace logger
{
    // Definition of the static mutex for serial output
    std::mutex LoggerSinkSerial::serial_mutex;

    LoggerSinkSerial::LoggerSinkSerial(logger::logLevel level,
                                       std::shared_ptr<SynchronizedSerial> port)
        : log_level(level), serial_port(std::move(port))
    {
        // Constructor body left empty: initialization done in initializer list
    }

    void LoggerSinkSerial::setLogEntry(const std::shared_ptr<logger::LogEntry>& entry) noexcept
    {
        if (!entry)
            return;  // Nothing to log

        // Filter based on log level
        const auto entry_level = entry->getLogLevel();
        bool should_output = false;
        std::string level_prefix;

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
                // Always log ERROR if sink accepts ERROR or higher
                level_prefix = "ERROR";
                should_output = true;
                break;
            default:
                // Unhandled levels are ignored
                return;
        }

        if (!should_output)
            return;

        // Format timestamp from the log entry's timepoint
        const auto time_t_val = std::chrono::system_clock::to_time_t(entry->getTimepoint());
        std::tm time_buf;
        localtime_r(&time_t_val, &time_buf);

        std::ostringstream out;
        out << level_prefix
            << ": [" << std::put_time(&time_buf, "%Y-%m-%d %H:%M:%S") << "]"
            << " - " << entry->getLogDomain()
            << ": " << entry->getLogMessage();

                // Thread-safe write to serial port
        std::lock_guard<std::mutex> lock(serial_mutex);
        // Write the formatted string directly to the serial interface
        serial_port->write(out.str());
    }
}