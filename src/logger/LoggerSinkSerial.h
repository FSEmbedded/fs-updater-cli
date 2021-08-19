
#pragma once
#include <fs_update_framework/logger/LoggerSinkBase.h>

#include "../cli/SynchronizedSerial.h"

#include <sstream>
#include <iostream>
#include <time.h>
#include <iomanip>
#include <fstream>

namespace logger
{
    class LoggerSinkSerial : public LoggerSinkBase
    {
        private:
            logger::logLevel log_level;
            std::shared_ptr<SynchronizedSerial> sink;

        public:
            LoggerSinkSerial(logger::logLevel level, std::shared_ptr<SynchronizedSerial> &);
            ~LoggerSinkSerial() = default;
            virtual void setLogEntry(const std::shared_ptr<logger::LogEntry> &) override;
    };
}