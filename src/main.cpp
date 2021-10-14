#include "cli/cli.h"
#include <fs_update_framework/BaseException.h>
#include <iostream>

int main(int argc, const char ** argv)
{
    try
    {
        cli::fs_update_cli cli_handler(argc, argv);
        return cli_handler.getReturnCode();
    }
    catch(const std::exception &e)
    {
        std::cerr << "Unhandled std::exception: " << e.what() << std::endl;
        return 124;
    }
}
