#include "LoggerSinkSerial.h"

logger::LoggerSinkSerial::LoggerSinkSerial(logger::logLevel level, std::shared_ptr<SynchronizedSerial> &ptr):
    sink(ptr)
{
    this->log_level = level;
}

void logger::LoggerSinkSerial::setLogEntry(const std::shared_ptr<logger::LogEntry> &ptr)
{
    std::stringstream out;
    if((ptr->getLogLevel() == logger::logLevel::DEBUG) && (this->log_level == logger::logLevel::DEBUG))
    {
        out << "DEBUG: ";
    }
    else if( (ptr->getLogLevel() == logger::logLevel::WARNING) &&
        (
            (this->log_level == logger::logLevel::WARNING) ||
            (this->log_level == logger::logLevel::ERROR) ||
            (this->log_level == logger::logLevel::DEBUG)
        )
    )
    {
        out << "WARNING: ";
    }
    else if(
        (ptr->getLogLevel() == logger::logLevel::ERROR) &&
        (
            (this->log_level == logger::logLevel::ERROR) ||
            (this->log_level == logger::logLevel::DEBUG)
        )
    )
    {
        out << "ERROR: ";
    }
    else
    {
        return;
    }

    std::time_t temp = std::chrono::system_clock::to_time_t(ptr->getTimepoint());
    struct tm buf;

    out << "[" <<  std::put_time(localtime_r(&temp,&buf), "%Y-%m-%d %X") << "]" << " - " << ptr->getLogDomain();
    out << ": " << ptr->getLogMessage() << std::endl;

    sink->write(out.str());
}