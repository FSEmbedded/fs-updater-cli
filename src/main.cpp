#include "cli/cli.h"
#include <iostream>

int main(int argc, const char ** argv)
{
    try
    {
        cli::fs_update_cli cli_handler;
        cli_handler.parse_input(argc, argv);
        return cli_handler.getReturnCode();
    }
    catch(const std::exception &e)
    {
        std::cerr << "Unhandled error: " << e.what() << std::endl;
        return 254;
    }
}