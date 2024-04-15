#ifndef FS_HPP_F48C75B0_9514_46CA_AA60_26A71726682A
#define FS_HPP_F48C75B0_9514_46CA_AA60_26A71726682A

#include <filesystem>
#include <vector>

namespace util {

std::vector<std::filesystem::path> get_files(const std::filesystem::path& dir, const std::filesystem::path& ext);

std::filesystem::path find_most_occuring_extension(const std::filesystem::path& dir);

std::vector<std::filesystem::path> get_most_occuring_extension_files(const std::filesystem::path& dir);

} //namespace util

#endif /* FS_HPP_F48C75B0_9514_46CA_AA60_26A71726682A */
