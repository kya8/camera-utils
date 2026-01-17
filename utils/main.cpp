#include "camera_utils.hpp"
#include <string_view>
#include <filesystem>
#include <print>
#include <ranges>

// Multi-call binary

namespace {

struct MainFn {
    using Fn = int(int, char**);
    std::string_view name;
    Fn& func;
};

constexpr MainFn main_fns[] {
    {"camerainfo", main_camerainfo},
    {"calibrator", main_calibrator},
    {"insta360_normalize", main_insta360_normalize},
    {"mp4_join", main_mp4_join}
};

} // namespace

int main(int argc, char** argv)
{
    const auto name = std::filesystem::path{argv[0]}.stem().string();
    for (const auto& fn : main_fns) { // runtime loop
        if (name == fn.name) {
            return fn.func(argc, argv);
        }
    }

    std::print(stderr, "Unknown program name: {}. Valid names are: {:n:s}\n",
               name,
               main_fns | std::views::transform([](const auto& fn) { return fn.name; })
               );

    return 2;
}
