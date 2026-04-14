#include "cli/cli.h"
#include "cli/cli_io.h"
#include "cli/fs_updater_error.h"
#include <fs_update_framework/BaseException.h>

int main(int argc, const char ** argv)
{
    try
    {
        cli::fs_update_cli cli_handler(argc, argv);
        return cli_handler.getReturnCode();
    }
    catch(const std::exception &e)
    {
        std::string msg = "Unhandled std::exception: ";
        msg.append(e.what());
        msg.append("\n");
        cli_io::write_stderr(msg);
        return static_cast<int>(UPDATER_FATAL::UNHANDLED_EXCEPTION);
    }
}
