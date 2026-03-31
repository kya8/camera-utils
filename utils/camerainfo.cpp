#include "camera_utils.hpp"
#include "caminfo.hpp"
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

using namespace caminfo;

int main_camerainfo(int argc, char** argv) noexcept
{
    std::vector<const char*> files;
    bool metadata_only = false;
    bool dump_gyro = false;
    bool dump_quat = false;
    std::size_t max_vec_len = 50;
    bool print_version = false;
    {
        bool err_flag = 0;
        bool positional_only = false;
        for (int i = 1; i < argc && !err_flag; ++i) {
            if (positional_only) {
                files.push_back(argv[i]);
            } else if (!std::strcmp(argv[i], "--")) { // After '--', all arguments are treated as input file names.
                positional_only = true;
            } else if (!std::strcmp(argv[i], "-m")) {
                metadata_only = true;
            } else if (!std::strcmp(argv[i], "-dg")) {
                dump_gyro = true;
            } else if (!std::strcmp(argv[i], "-dq")) {
                dump_quat = true;
            } else if (!std::strcmp(argv[i], "-l")) {
                if (++i < argc) {
                    char* end;
                    max_vec_len = std::strtoul(argv[i], &end, 0);
                    if (end == argv[i])
                        err_flag = 1;
                }
                else {
                    err_flag = 1;
                }
            } else if (!std::strcmp(argv[i], "-V")) {
                print_version = true;
                break;
            } else {
                files.push_back(argv[i]);
            }
        }

        if (err_flag || (files.empty() && !print_version)) {
            std::println("Usage: camerainfo <video_files> [-l <max_vec_length>] [-m] [-dg] [-dq] [-V]");
            return 2;
        }
    }

    if (print_version) {
        using namespace camera_utils::version;
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
            std::print("{:<{}}: {}\n", "Camera Vendor", width, get_vendor_name(info->vendor));

            for (const auto& [group, map] : info->extras) {
                if (stdout_is_colorterm)
                    std::print("\033[33m");
                std::print("<Group: {}>\n", to_string(group));
                if (stdout_is_colorterm)
                    std::print("\033[0m" );
                for (const auto& [key, value] : map) {
                    if (stdout_is_colorterm)
                        std::print("  \033[36m{:<{}}\033[0m", to_string(key), width - 2);
                    else
                        std::print("  {:<{}}", to_string(key), width - 2);
                    std::print(": {}\n", to_string(value, max_vec_len));
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
