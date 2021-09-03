#include "cli/cli.h"
#include <fs_update_framework/BaseException.h>
#include <iostream>

int main(int argc, const char ** argv)
{
    try
    {
        cli::fs_update_cli cli_handler;
        cli_handler.parse_input(argc, argv);
        return cli_handler.getReturnCode();
    }
    catch(const fs::BaseFSUpdateException &e)
    {
        std::cerr << "Unhandled fs::BaseFSUpdateException: " << e.what() << std::endl;
        return 13;
    }
    catch(const std::exception &e)
    {
        std::cerr << "Unhandled std::exception: " << e.what() << std::endl;
        return 124;
    }
}
