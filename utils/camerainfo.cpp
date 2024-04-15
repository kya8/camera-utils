#include "camera_utils.hpp"
#include "caminfo.hpp"
#include <iostream>
#include <iomanip>
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
    using std::cout, std::endl;

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
            cout << "Usage: camerainfo <video_files> [-l <max_vec_length>] [-m] [-dg] [-dq] [-V]" << endl;
            return 2;
        }
    }

    if (print_version) {
        using namespace camera_utils::version;
        cout << "Version " << COMMIT_HASH << ", " << COMMIT_DATE << endl;
        return 0;
    }

    util::Timer timer;
    const bool stdout_is_colorterm = sys::is_colorterm(stdout);
    cout << std::left;
    for (const auto i : range::make_index(files.size())) {
        const auto& file = files[i];
        timer.tic();
        const auto info = detect(file, metadata_only);
        const auto time_detect = timer.toc();

        if (i > 0) cout << '\n';
        if (stdout_is_colorterm)
            cout << "\033[1;32m" << file << "\033[0m";
        else
            cout << file;
        if (info)
            cout << ": Done in " << time_detect * 1e3 << " ms\n";
        else
            cout << ": Failed to extract data, or input file is invalid.\n";

        if (info) {
            static constexpr auto width = 30;
            cout << std::setw(width) << "Camera Vendor" << ": " << get_vendor_name(info->vendor) << '\n';
            // cout << std::setw(width) << "Model"  << ": " << info->model                 << '\n';
            // cout << std::setw(width) << "SN"     << ": " << info->SN                    << '\n';
            for (const auto& [group, map] : info->extras) {
                if (stdout_is_colorterm) cout << "\033[33m";
                cout << "<Group: " << get_group_id_string(group) << ">\n";
                if (stdout_is_colorterm) cout << "\033[0m";
                for (const auto& [key, value] : map) {
                    if (stdout_is_colorterm)
                        cout << "  \033[36m" << std::setw(width - 2) << key_to_string(key) << "\033[0m";
                    else
                        cout << "  " << std::setw(width - 2) << key_to_string(key);
                    cout << ": " << var_to_string(value, max_vec_len) << '\n';
                }
            }

            // dump data to file
            if (dump_gyro) {
                const auto gyro = info->extras.get<types::GyroVec>(GroupId::SensorData, KeyId::GyroData);
                if (gyro) {
                    std::ofstream ofs(std::string(file) + "_gyro.txt");
                    ofs.precision(17);
                    if (ofs) {
                        ofs << "time, gyro_x, gyro_y, gyro_z\n";
                        for (const auto& [t, x, y, z] : *gyro) {
                            ofs << t << ", " << x << ", " << y << ", " << z << '\n';
                        }
                    }
                }
            }
            if (dump_quat) {
                const auto quat = info->extras.get<types::QuaternionVec>(GroupId::SensorData, KeyId::CameraQuaternionData);
                if (quat) {
                    std::ofstream ofs(std::string(file) + "_quat.txt");
                    ofs.precision(17);
                    if (ofs) {
                        ofs << "qx, qy, qz, qw\n";
                        for (const auto& [x, y, z, w] : *quat) {
                            ofs << x << ", " << y << ", " << z << ", " << w << '\n';
                        }
                    }
                }
            }
        }
    }

    return 0;
}
