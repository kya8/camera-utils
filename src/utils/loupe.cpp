#include "slate_utils.hpp"
#include <slate/loupe/loupe.hpp>
#include <print>
#include <ostream>
#include "timer.hpp"
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include "version.hpp"
#include "sys_utils.hpp"
#include <range.hpp>
#include "string_utils.hpp"
#include <iostream>

using namespace slate::loupe;

namespace {

const auto& help_msg =
R"^^(🔍 loupe: Extract and display camera information from video files.

Usage: loupe <FILES...> [OPTIONS...]

Options:
 -m, --metadata-only  Only extract metadata, skip sensor data and other streaming data.
 -j, --raw-output     Output a raw JSON-like format for easier parsing.
 -g, --dump-gyro      Dump gyroscope data (if available) in CSV format for each input.
 -q, --dump-quat      Dump camera quaternion data (if available) in CSV format.
 -l <N>               Limit the number of elements printed for vector types to N. Default is 50. Set to 0 for no limit.
                      Does not affect '-j' output, which always prints the full data.
 --                   Treat all following arguments as input file names, even if they start with '-'.
 -V, --version        Display version information and exit.
 -h, --help           Display this help message and exit.
)^^";

} // namespace

int slate::main_loupe(int argc, char** argv) noexcept
{
    std::vector<const char*> files;
    bool metadata_only = false;
    bool dump_gyro = false;
    bool dump_quat = false;
    std::size_t max_vec_len = 50;
    bool print_version = false;
    bool show_help = false;
    bool raw_output = false;
    {
        bool err_flag = 0;
        bool positional_only = false;
        for (int i = 1; i < argc && !err_flag; ++i) {
            if (positional_only) {
                files.push_back(argv[i]);
            } else if (match(argv[i], "--")) { // After '--', all arguments are treated as input file names.
                positional_only = true;
            } else if (match(argv[i], "-m", "--metadata-only")) {
                metadata_only = true;
            } else if (match(argv[i], "-j", "--raw-output")) {
                raw_output = true;
            } else if (match(argv[i], "-g", "--dump-gyro")) {
                dump_gyro = true;
            } else if (match(argv[i], "-q", "--dump-quat")) {
                dump_quat = true;
            } else if (match(argv[i], "-l")) {
                if (++i < argc) {
                    char* end;
                    max_vec_len = std::strtoul(argv[i], &end, 0);
                    if (end == argv[i])
                        err_flag = 1;
                }
                else {
                    err_flag = 1;
                }
            } else if (match(argv[i], "-h", "--help")) {
                show_help = true;
                break;
            } else if (match(argv[i], "-V", "--version")) {
                print_version = true;
                break;
            } else {
                files.push_back(argv[i]);
            }
        }

        if (err_flag || (files.empty() && !print_version && !show_help)) {
            std::print(stderr, "Invaild argument.\nPass '-h' for help.\n");
            return 2;
        }
    }

    if (show_help) {
        std::print("{}", help_msg);
        return 0;
    }

    if (print_version) {
        using namespace slate::version;
        print_info();
        return 0;
    }

    util::Timer timer;
    const bool stdout_is_colorterm = sys::is_colorterm(stdout);
    for (const auto i : range::make_index(files.size())) {
        const auto& file = files[i];
        timer.tic();
        const auto info = detect(file, metadata_only);
        const auto time_detect = timer.toc();

        if (i > 0) std::putchar('\n');
        if (stdout_is_colorterm)
            std::print("\033[1;32m{}\033[0m", file);
        else
            std::print("{}", file);
        if (info)
            std::print(": Done in {:.3f} ms\n", time_detect * 1e3);
        else
            std::print(": Failed to extract data, or input file is invalid.\n");

        if (info) {
            static constexpr auto width = 30;
            std::print("{:<{}}: {}\n", "Camera Vendor", width, to_string(info->vendor));

            for (const auto& [group, map] : info->extras) {
                if (stdout_is_colorterm)
                    std::print("\033[33m");
                std::print("<Group: {}>\n", to_string(group));
                if (stdout_is_colorterm)
                    std::print("\033[0m" );
                if (raw_output) {
                    std::cout << map << '\n';
                } else {
                    for (const auto& [key, value] : map) {
                        if (stdout_is_colorterm)
                            std::print("  \033[36m{:<{}}\033[0m", to_string(key), width - 2);
                        else
                            std::print("  {:<{}}", to_string(key), width - 2);
                        std::print(": {}\n", to_string(value, max_vec_len));
                    }
                }
            }

            // dump data to file
            if (dump_gyro) {
                const auto gyro = info->extras.get<types::GyroVec>(GroupId::SensorData, KeyId::GyroData);
                if (gyro) {
                    std::ofstream ofs(std::string(file) + "_gyro.txt");
                    // ofs.precision(17);
                    if (ofs) {
                        std::println(ofs, "time, gyro_x, gyro_y, gyro_z");
                        for (const auto& entry : *gyro) {
                            std::println(ofs, "{:n}", entry); // std::format defaults to round-trip format for FP
                        }
                    }
                }
            }
            if (dump_quat) {
                const auto quat = info->extras.get<types::QuaternionVec>(GroupId::SensorData, KeyId::CameraQuaternionData);
                if (quat) {
                    std::ofstream ofs(std::string(file) + "_quat.txt");
                    if (ofs) {
                        std::println(ofs, "qx, qy, qz, qw");
                        for (const auto& entry : *quat) {
                            std::println(ofs, "{:n}", entry);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
