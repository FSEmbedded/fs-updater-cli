#include "SynchronizedSerial.h"

#include <fcntl.h>
#include <unistd.h>

SynchronizedSerial::SynchronizedSerial()
{
    UBoot::UBoot uboot_handler("/etc/fw_env.config");
    const std::string console = util::split(util::split(uboot_handler.getVariable("console"),'=').back(), ',').at(0);
    const std::string dev_path = std::string("/dev/") + console;
    this->serial_fd = ::open(dev_path.c_str(), O_WRONLY | O_NOCTTY | O_CLOEXEC);
}

SynchronizedSerial::~SynchronizedSerial()
{
    if (this->serial_fd >= 0)
    {
        ::close(this->serial_fd);
    }
}

void SynchronizedSerial::write(const std::string &in)
{
    if (this->serial_fd < 0) { return; }

    std::lock_guard<std::mutex> lock(this->lock_query);
    const char* data = in.data();
    std::string::size_type remaining = in.size();
    while (remaining > 0)
    {
        const ssize_t written = ::write(this->serial_fd, data, remaining);
        if (written <= 0) { break; }
        data += written;
        remaining -= static_cast<std::string::size_type>(written);
    }
}
