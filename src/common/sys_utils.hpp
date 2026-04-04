#ifndef SYS_UTILS_HPP_B7FAA969_37E6_45B4_89B2_1373F9395991
#define SYS_UTILS_HPP_B7FAA969_37E6_45B4_89B2_1373F9395991

#include <string>
#include <optional>
#include <filesystem>
#include <cstddef>
#include <cstdio>

namespace sys {

std::optional<std::string> get_hostname() noexcept;
std::optional<std::filesystem::path> get_proc_path() noexcept;

struct MemoryInfo {
    std::size_t total; // total physical memory in bytes
    std::size_t available; // available physical memory
};

std::optional<MemoryInfo> get_mem_info() noexcept;

bool is_tty(std::FILE* file) noexcept;

bool is_colorterm() noexcept;
bool is_colorterm(std::FILE* file) noexcept;

} // namespace sys

#endif /* SYS_UTILS_HPP_B7FAA969_37E6_45B4_89B2_1373F9395991 */
