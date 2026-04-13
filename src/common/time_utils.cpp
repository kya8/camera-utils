#include "time_utils.hpp"
#include <memory>
#include <stdexcept>

int
util::localtime(const std::time_t& t, std::tm& tm) noexcept
{
#if defined(_WIN32)
    return ::localtime_s(&tm, &t);
#else // POSIX
    return ::localtime_r(&t, &tm) ? 0 : -1;
#endif
}

int
util::gmtime(const std::time_t& t, std::tm& tm) noexcept
{
#if defined(_WIN32)
    return ::gmtime_s(&tm, &t);
#else
    return ::gmtime_r(&t, &tm) ? 0 : -1;
#endif
}

std::string
util::format_time(const std::tm &tm, const char* fmt)
{
    for (int len = 64; len < 1024; len *= 2) {
        const auto buf = std::make_unique_for_overwrite<char[]>(len);
        const auto size = std::strftime(buf.get(), 64, fmt, &tm);
        if (size > 0) {
            return {buf.get(), size};
        }
    }
    throw std::length_error("String is too long");
}
