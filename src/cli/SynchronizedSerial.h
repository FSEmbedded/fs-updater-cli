#pragma once

#include <fstream>
#include <string>
#include <mutex>

#include <fs_update_framework/uboot_interface/UBoot.h>
#include <fs_update_framework/handle_update/utils.h>

class SynchronizedSerial
{
    private:
        std::ofstream serial_object;
        std::mutex lock_query;
    
    public:
        /**
         * Read from UBoot the standard serial output interface and redirect all output to that.
         */
        SynchronizedSerial();
        ~SynchronizedSerial();

        /**
         * Open serial output and write stream to serial.
         * Lock the resource serial for each write command.
         * @param in Stringstream object for serial output.
         */
        void write(const std::string &in);
};