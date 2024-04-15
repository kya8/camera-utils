#include "fs.hpp"
#include <unordered_map>
#include <algorithm>

namespace fs = std::filesystem;

struct PathHash { // LWG 3657
    auto operator()(const fs::path& path) const noexcept
    {
        return fs::hash_value(path);
    }
};

namespace util {

std::vector<fs::path> get_files(const fs::path& dir, const fs::path& ext)
{
    std::vector<fs::path> ret;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            const auto& path = entry.path();
            if (path.extension() == ext)
                ret.push_back(path);
        }
    }
    return ret;
}

fs::path find_most_occuring_extension(const fs::path& dir)
{
    std::unordered_map<fs::path, int, PathHash> ext_counter;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            ++ext_counter[entry.path().extension()];
        }
    }
    if (ext_counter.empty())
        return {};
    return std::max_element(ext_counter.cbegin(), ext_counter.cend(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
}

std::vector<std::filesystem::path> get_most_occuring_extension_files(const std::filesystem::path& dir)
{
    std::unordered_map<fs::path, std::vector<fs::path>, PathHash> ext_files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            ext_files[entry.path().extension()].push_back(entry.path());
        }
    }
    if (ext_files.empty()) {
        return {};
    }
    return std::max_element(
        ext_files.cbegin(),
        ext_files.cend(),
        [](const auto& a, const auto& b) { return a.second.size() < b.second.size(); }
    )->second;
}

} // namespace util
