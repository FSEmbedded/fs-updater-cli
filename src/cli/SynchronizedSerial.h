#pragma once

#include <string>
#include <mutex>

#include <fs_update_framework/uboot_interface/UBoot.h>
#include <fs_update_framework/handle_update/utils.h>

class SynchronizedSerial
{
    private:
        int serial_fd{-1};
        std::mutex lock_query;

    public:
        /**
         * Read from UBoot the standard serial output interface and redirect all output to that.
         */
        SynchronizedSerial();
        ~SynchronizedSerial();

        SynchronizedSerial(const SynchronizedSerial&) = delete;
        SynchronizedSerial& operator=(const SynchronizedSerial&) = delete;

        /**
         * Write string to serial output.
         * Lock the resource serial for each write command.
         * @param in String for serial output.
         */
        void write(const std::string &in);
};
