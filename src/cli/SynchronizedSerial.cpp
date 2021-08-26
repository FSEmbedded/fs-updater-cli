#include "SynchronizedSerial.h"

SynchronizedSerial::SynchronizedSerial()
{
    UBoot::UBoot uboot_handler("/etc/fw_env.config");
    const std::string console = util::split(util::split(uboot_handler.getVariable("console"),'=').back(), ',').at(0);
    this->serial_object = std::ofstream(std::string("/dev/") + console, std::ofstream::out);
}

SynchronizedSerial::~SynchronizedSerial()
{

}

void SynchronizedSerial::write(const std::string &in)
{
    std::lock_guard<std::mutex> lock(this->lock_query);
    this->serial_object << in;
    this->serial_object.flush();
}

