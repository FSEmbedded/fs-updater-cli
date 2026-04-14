#pragma once

#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace posix_helpers {

[[nodiscard]] inline bool path_exists(const char* path) noexcept
{
    struct stat st;
    return (::stat(path, &st) == 0);
}

[[nodiscard]] inline ssize_t file_size(const char* path) noexcept
{
    struct stat st;
    if (::stat(path, &st) != 0) { return -1; }
    return static_cast<ssize_t>(st.st_size);
}

[[nodiscard]] inline bool read_file(const char* path, std::string& out) noexcept
{
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { return false; }

    struct stat st;
    if (::fstat(fd, &st) != 0) { ::close(fd); return false; }

    out.resize(static_cast<std::string::size_type>(st.st_size));
    const ssize_t n = ::read(fd, out.data(), out.size());
    ::close(fd);

    if (n < 0) { out.clear(); return false; }
    out.resize(static_cast<std::string::size_type>(n));

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    {
        out.pop_back();
    }
    return true;
}

[[nodiscard]] inline bool create_marker_file(const char* path) noexcept
{
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) { return false; }
    ::close(fd);
    return true;
}

[[nodiscard]] inline bool remove_file(const char* path) noexcept
{
    if (::unlink(path) != 0 && errno != ENOENT) { return false; }
    return true;
}

[[nodiscard]] inline std::string path_join(std::string_view dir, std::string_view file) noexcept
{
    std::string result;
    result.reserve(dir.size() + 1 + file.size());
    result.append(dir);
    if (!dir.empty() && dir.back() != '/') { result += '/'; }
    result.append(file);
    return result;
}

} // namespace posix_helpers
