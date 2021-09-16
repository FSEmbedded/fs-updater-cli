
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
            /**
             * Init logger endpoint for serial interface.
             * @param level Set the expected log level which should be thrown
             * @param ptr Set the dynamic shared object for access the serial output.
             */
            LoggerSinkSerial(logger::logLevel level, std::shared_ptr<SynchronizedSerial> &ptr);

            ~LoggerSinkSerial() = default;

            /**
             * Override virtual function and define the specific setLogEntry function for serial output.
             * @param ptr Shared object of logger::LogEntry which should be log.
             */
            virtual void setLogEntry(const std::shared_ptr<logger::LogEntry> &ptr) override;
    };
}