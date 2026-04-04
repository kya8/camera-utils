#include "time_utils.hpp"

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
