#pragma once

#include <string_view>
#include <unistd.h>

namespace cli_io {

inline void write_stdout(std::string_view msg) noexcept
{
    ssize_t ret = ::write(STDOUT_FILENO, msg.data(), msg.size());
    (void)ret;
}

inline void write_stderr(std::string_view msg) noexcept
{
    ssize_t ret = ::write(STDERR_FILENO, msg.data(), msg.size());
    (void)ret;
}

} // namespace cli_io
