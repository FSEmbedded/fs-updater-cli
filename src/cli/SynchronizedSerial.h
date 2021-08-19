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
        SynchronizedSerial();
        ~SynchronizedSerial();
        void write(const std::string &in);
};