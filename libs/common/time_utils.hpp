#ifndef TIME_UTILS_HPP_CFD3FFD1_A6DE_42CB_A446_1A12FA90EA24
#define TIME_UTILS_HPP_CFD3FFD1_A6DE_42CB_A446_1A12FA90EA24

#include <ctime>

namespace util {

/**
 * @brief Safe localtime function.
 * @return 0 on success, non-zero on failure.
 */
int localtime(const std::time_t& t, std::tm& tm) noexcept;

int gmtime(const std::time_t& t, std::tm& tm) noexcept;

} // namespace util

#endif /* TIME_UTILS_HPP_CFD3FFD1_A6DE_42CB_A446_1A12FA90EA24 */
