#include "camera_utils.hpp"
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <caminfo.hpp>
#include <extras/insta360_params.hpp>
#include <extras/insta360_tf.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cmath>        // std::tan
#include <Eigen/Geometry>
#include "ThreadPool.hpp"
#include <thread>       // hardware_concurrency()
#include <memory>
#include "version.hpp"
#include "sys_utils.hpp"
#include "fs.hpp"
#include <termcolor.hpp>
#include <numbers>

using namespace std::numbers;

namespace {

std::vector<double>
parse_csv(const char* str, char delim = ',') noexcept
{
    std::vector<double> ret;
    char* end;
    while (*str != 0) {
        const auto d = std::strtod(str, &end);
        if (end == str) return ret;
        ret.push_back(d);
        if (*end == 0 || *end != delim) return ret;
        str = end + 1;
    }
    return ret;
}

namespace fs = std::filesystem;

using CvMaps = std::pair<cv::Mat_<float>, cv::Mat_<float>>;

template<typename F>
CvMaps get_perspective_map(cv::Size size, double f, double cx, double cy, const F& proj) noexcept
{
    cv::Mat_<float> mapx(size.height, size.width);
    cv::Mat_<float> mapy(size.height, size.width);

    for (int row = 0; row < size.height; ++row) {
        for (int col = 0; col < size.width; ++col) {
            const auto xn  = (col - cx) / f;
            const auto yn  = (row - cy) / f;
            const auto uv  = proj(xn, yn, 1.0);
            mapx(row, col) = static_cast<float>(uv[0]);
            mapy(row, col) = static_cast<float>(uv[1]);
        }
    }

    return {mapx, mapy};
}

template<typename F>
CvMaps get_equirectangular_map(int height, const F& proj) noexcept
{
    const auto width = height * 2;

    cv::Mat_<float> mapx(height, width);
    cv::Mat_<float> mapy(height, width);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const double lat = pi / 2 - pi * row / height;
            const double lon = pi * 2 * col / width;
            const auto uv = proj(std::cos(lat) * std::sin(lon), -std::sin(lat), std::cos(lat) * std::cos(lon));
            mapx(row, col) = static_cast<float>(uv[0]);
            mapy(row, col) = static_cast<float>(uv[1]);
        }
    }

    return {mapx, mapy};
}

template<typename F>
CvMaps get_cubemap(int size, const F& proj) noexcept // 2*3 stacked
{
    cv::Mat_<float> mapx(2 * size, 3 * size);
    cv::Mat_<float> mapy(2 * size, 3 * size);

    static const struct {
        int row, col;
        Eigen::AngleAxisd rot;
    } LUT_cubes[] {
        /**************
         | F | L | U |
         | B | R | D |
        ***************/

        // front
        {0, 0, Eigen::AngleAxisd::Identity()},
        // back
        {1, 0, Eigen::AngleAxisd{pi, Eigen::Vector3d::UnitY()}},
        // left
        {0, 1, Eigen::AngleAxisd{-pi / 2, Eigen::Vector3d::UnitY()}},
        // right
        {1, 1, Eigen::AngleAxisd{pi / 2, Eigen::Vector3d::UnitY()}},
        // up
        {0, 2, Eigen::AngleAxisd{pi / 2, Eigen::Vector3d::UnitX()}},
        // down
        {1, 2, Eigen::AngleAxisd{-pi / 2, Eigen::Vector3d::UnitX()}}
    };

    const auto c = (size - 1) / 2.0;
    const auto f = c;

    for (const auto& [R, C, rot] : LUT_cubes) {
        const Eigen::Quaterniond quat{rot};
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const Eigen::Vector3d pt = quat * Eigen::Vector3d{(col - c) / f, (row - c) / f, 1.0};
                const auto uv  = proj(pt.x(), pt.y(), pt.z());
                mapx(R*size + row, C*size + col) = static_cast<float>(uv[0]);
                mapy(R*size + row, C*size + col) = static_cast<float>(uv[1]);
            }
        }
    }

    return {mapx, mapy};
}

using std::cout, std::cerr, std::endl;

enum Mode {
    Split = 0,
    Equirectangular,
    Cubemap
};

const auto& help_message =
R"^^(insta360_normalize: Undistort images from insta360 cameras.

Usage: insta360_normalize <-v FILE> [-d0 DIRECTORY] [-d1 DIRECTORY] [-f FOV]

Options:
 -v  FILE   Primary .insv video file
 -d0 DIR    Image directory for lens 0 or joint video
 -d1 DIR    Secondary image directory for lens 1, in case the videos are split
 -f  FOV    Horizontal FOV angle (degrees, full view) of undistorted image. Default: 90.
 -w  WIDTH  Output image width. Default: original video width.
 -nc        Disable cropping. (For testing only)
 -T  NUM    Number of threads to use. Default is 0 (auto).
 --eqr      Equirectangular projection.
 --cube     Cubemap projection.
 --tf       Specify transform from lens0 to lens1. x, y, z, qx, qy, qz, qw.
            If not specified, built-in transform is used.
 -F FORMAT  Output image format, e.g. jpg/png. Default: jpg.
 -V         Display version information.
 -h         Display this help message.
)^^";

struct Cfg {
    std::string video_file;
    fs::path dir[2];
    //double f_scale = 3.0;
    double fov_x   = 90;    // horizontal fov angle (degrees)
    int output_width = -1;
    bool with_crop = true;
    unsigned nb_threads = 0;
    bool print_version = false;
    bool print_help = false;
    Mode mode = Split;
    bool has_tf = false;
    Eigen::Isometry3d tf;
    std::string output_image_format = "jpg";

    bool load_args(int argc, char** argv) noexcept
    {
        bool err_flag = 0;
        int i = 1;
        const auto check_arg = [&] {
            if (i+1 >= argc /* || argv[i+1][0] == '-' */) {
                err_flag = 1;
                return false;
            }
            return true;
        };

        while (i < argc && !err_flag) {
            if (!std::strcmp(argv[i], "-v")) {
                if (check_arg()) {
                    video_file = argv[++i];
                }
            }
            else if (!std::strcmp(argv[i], "-d0")) {
                if (check_arg()) {
                    dir[0] = argv[++i];
                }
            }
            else if (!std::strcmp(argv[i], "-d1")) {
                if (check_arg()) {
                    dir[1] = argv[++i];
                }
            }
            else if (!std::strcmp(argv[i], "-f")) {
                if (check_arg()) {
                    char* str_end;
                    fov_x = std::strtod(argv[++i], &str_end);
                    if (str_end == argv[i]) err_flag = 1;
                }
            }
            else if (!std::strcmp(argv[i], "-w")) {
                if (check_arg()) {
                    char* str_end;
                    output_width = std::strtol(argv[++i], &str_end, 0);
                    if (str_end == argv[i]) err_flag = 1;
                }
            }
            else if (!std::strcmp(argv[i], "-nc")) {
                with_crop = false;
            }
            else if (!std::strcmp(argv[i], "-T")) {
                if (check_arg()) {
                    char* str_end;
                    nb_threads = std::strtoul(argv[++i], &str_end, 0);
                    if (str_end == argv[i]) err_flag = 1;
                }
            }
            else if (!std::strcmp(argv[i], "--eqr")) {
                mode = Equirectangular;
            }
            else if (!std::strcmp(argv[i], "--cube")) {
                mode = Cubemap;
            }
            else if (!std::strcmp(argv[i], "--tf")) {
                if (check_arg()) {
                    const auto v = parse_csv(argv[++i]);
                    if (v.size() != 7) {
                        err_flag = 1;
                    } else {
                        has_tf = true;
                        tf = Eigen::Translation3d(v[0], v[1], v[2]) * Eigen::Quaterniond(v.data() + 3);
                    }
                }
            }
            else if (!std::strcmp(argv[i], "-V")) {
                print_version = true;
                break;
            }
            else if (!std::strcmp(argv[i], "-h")) {
                print_help = true;
                break;
            }
            else if (!std::strcmp(argv[i], "-F")) {
                if (check_arg()) {
                    output_image_format = argv[++i];
                }
            }
            ++i;
        }

        return !err_flag;
    }

    bool process_dir(int lens_id, const caminfo::insta360::Params& params, ThreadPool& pool) noexcept try
    {
        if (mode) lens_id = 0;
        const auto& lens = params.lens[lens_id];

        const auto output_width  = this->output_width > 0? this->output_width : params.width;
        const auto output_height = output_width * params.height / params.width;
        const auto cx = (output_width - 1) / 2.0;
        const auto cy = (output_height - 1) / 2.0;
        const auto f = cx / std::tan(fov_x / 2 * 0.01745329251994329577);

        const auto& [map1, map2] = [&] {
            if (mode) {
                const auto proj = [&](double x, double y, double z) {
                    if (z >= 0) {
                        return params.lens[0].project_point(x, y, z);
                    }
                    const Eigen::Vector3d pt = tf.rotation() * Eigen::Vector3d{x, y, z};
                    auto uv = params.lens[1].project_point(pt.x(), pt.y(), pt.z());
                    uv[0] += params.width;
                    return uv;
                };
                if (mode == Equirectangular)
                    return get_equirectangular_map(output_height, proj);
                else
                    return get_cubemap(params.height / 2, proj);
            }
            return get_perspective_map({output_width, output_height}, f, cx, cy,
            [&](double x, double y, double z) {
                auto uv = lens.project_point(x, y, z);
                if (lens_id == 1 && params.joined)
                    uv[0] += params.width;
                return uv;
            });
        }();

        // look for images
        const auto& work_dir = (lens_id == 0) || (params.joined)? dir[0] : dir[1];

        if (!fs::is_directory(work_dir) || (mode && !params.joined && !fs::is_directory(dir[1]))) {
            cerr << "Invalid image directory.\n";
            return false;
        }
        const auto output_dir = work_dir / (lens_id == 0? "normalized_0" : "normalized_1");
        const auto image_files = util::get_most_occuring_extension_files(work_dir);
        if (image_files.empty()) {
            cerr << "No images were found!\n";
            return false;
        }
        {
            std::error_code ec;
            fs::create_directories(output_dir, ec);
            if (ec) {
                cerr << "Cannot create output directory.\n";
                return EXIT_FAILURE;
            }
        }

        // Process images
        const auto total_images = image_files.size();
        cout << total_images << " images to process...\n";
        std::size_t cnt_done = 0, prev_done = 0, cnt_bad = 0;
        std::condition_variable cond;
        std::mutex mtx;

        for (auto& src : image_files) {
            pool.enqueue([&, src = std::move(src)] {
                const auto status = [&] {
                    cv::Mat rhs_image;
                    if (mode && !params.joined) {
                        const fs::path rhs_path = dir[1] / src.filename();
                        rhs_image = cv::imread(rhs_path.string());
                        if (rhs_image.empty()) return 1;
                    }
                    auto in = cv::imread(src.string());
                    if (mode && !params.joined) {
                        cv::Mat concat;
                        cv::hconcat(in, rhs_image, concat);
                        in = std::move(concat);
                    }
                    if (in.empty()) {
                        cerr << "Failure reading image " << src << '\n';
                        return 1;
                    }

                    // check image dimension...

                    cv::Mat out;
                    cv::remap(in, out, map1, map2, cv::InterpolationFlags::INTER_LINEAR);
                    const auto dst = output_dir / src.filename().replace_extension(output_image_format);
                    if (!imwrite(dst.string(), out, {cv::IMWRITE_JPEG_QUALITY, 90})) {
                        cerr << "Failure writing output file " << dst << '\n';
                        return 1;
                    }
                    return 0;
                }();
                {
                    std::lock_guard lk(mtx);
                    cnt_done += 1;
                    if (status != 0) cnt_bad += 1;
                }
                cond.notify_all();
                //return status;
            });
        }
        for (;;) {
            {
                std::unique_lock lk(mtx);
                cond.wait(lk, [&] { return cnt_done > prev_done; });
                prev_done = cnt_done;
            }
            std::string out {"\r"};
            out.append(std::to_string(prev_done)).append(" / ").append(std::to_string(total_images));
            cout << out << std::flush;
            if (prev_done == total_images) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        pool.wait_all();
        if (mode) cout << (mode == Equirectangular? "\nEquirectangular" : "\nCubemap");
        else cout << "\nLens " << lens_id;
        cout << " finished: " << total_images << " total, " << cnt_bad << " failed.\n";

        if (!mode) {
            std::ofstream ofs(output_dir / "normalized_camera_params.txt");
            if (!ofs) {
                cerr << "Error creating params file.\n";
            }
            else {
                ofs << "Output image size: " << output_width << "*" << output_height << '\n';
                ofs << "Lens " << lens_id << ": Undistorted fx, fy, cx, cy: " << f << ", " << f << ", " << cx << ", " << cy << '\n';
            }
        }

        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }

    int run(int argc, char** argv) noexcept
    {
        const bool args_ok = load_args(argc, argv);

        if (print_version && args_ok) {
            using namespace camera_utils::version;
            print_info();
            return 0;
        }

        if (print_help) {
            cout << help_message;
            return 0;
        }

        if (!args_ok || video_file.empty() || (dir[0].empty() && dir[1].empty())) {
            cerr << tc::wrap<tc::RedFg, tc::Bold>("Invalid argument!\n", []{return sys::is_colorterm(stderr);});
            cout << "Pass '-h' for help.\n";
            return 2;
        }

        if (mode && dir[0].empty()) {
            cerr << "Equirectangular/Cubemap projection requires full imagery.\n";
            return 2;
        }

        if (fov_x >= 180 || fov_x <= 0) {
            cerr << "Invalid FOV angle!\n";
            return 2;
        }

        if (!cv::haveImageWriter(std::string{"."} += output_image_format)) {
            cerr << "Unsupported output image format: " << output_image_format << ".\n";
            return 1;
        }

        using namespace caminfo;

        insta360::Params params;
        const auto cam_info = detect(video_file.c_str(), true);
        if (!cam_info || !insta360::get_params(*cam_info, params, with_crop)) {
            cerr << "Could not read camera info from video file.\n";
            return 1;
        }

        if (mode && !has_tf) {
            // Get Insta360 builtin tf
            has_tf = [&] {
                const auto offset_v3 = cam_info->extras.get<types::VecD>(GroupId::Metadata, "offset_v3");
                if (!offset_v3)
                    return false;
                const auto r_b0 = insta360::get_R0(offset_v3->data(), static_cast<int>(offset_v3->size()));
                const auto r_b1 = insta360::get_R1(offset_v3->data(), static_cast<int>(offset_v3->size()));
                const auto t_01 = insta360::get_T01(offset_v3->data(), static_cast<int>(offset_v3->size()));
                if (!(r_b0 && r_b1 && t_01))
                    return false;
                const Eigen::Quaterniond r_01 = Eigen::Quaterniond(*r_b1) * Eigen::Quaterniond(*r_b0).inverse();
                tf = Eigen::Translation3d{*t_01} * r_01;
                cout << "Using built-in lenses transform: [" << t_01->x() << ", " << t_01->y() << ", " << t_01->z() << ", "
                                                             << r_01.x() << ", " << r_01.y() << ", " << r_01.z() << ", " << r_01.w() << "]\n";
                return true;
            }();
            if (!has_tf) {
                cerr << "Could not extract built-in lenses transform, which is required.\n";
                return 1;
            }
        }
        const auto camera_model = cam_info->extras.get_or<types::String>(GroupId::NormalizedMetadata, KeyId::CameraModel, "Unknown");
        const auto camera_SN    = cam_info->extras.get_or<types::String>(GroupId::NormalizedMetadata, KeyId::SerialNumber, "Unknown");
        cout << "Video file: " << video_file << "\nCamera model: " << camera_model << "\nSN: " << camera_SN << '\n'
            << "Resolution: " << params.width << " x " << params.height << "; Number of lenses: " << params.nb_lens <<  "; Video is Joined: " << (params.joined ? "Yes" : "No") << '\n';
        cout << "Using fov_x angle: " << fov_x << " deg\n";

        if (params.selfie) {
            cerr << "Bad lens direction!\n";
            return 1;
        }

        if (params.joined && !dir[1].empty()) {
            cerr << "Secondary image directory provided, but video is joined.\n";
            return 1;
        }
        if (!dir[1].empty() && params.nb_lens < 2) {
            cerr << "Only one lens was detected!\n";
            return 1;
        }
        if (mode && !params.joined && dir[1].empty()) {
            cerr << "Missing secondary image dir\n";
            return 1;
        }

        const auto pool = std::make_unique<ThreadPool>([&]{
            if (nb_threads > 0) return nb_threads;
            const auto n = std::thread::hardware_concurrency();
            return n? n : 8u;
        }());

        if (mode) {
            process_dir(0, params, *pool);
        }
        else {
            if (!dir[0].empty()) process_dir(0, params, *pool);
            if (!dir[1].empty() || params.joined) process_dir(1, params, *pool);
        }

        return 0;
    }
};

} // unnamed namespace

int main_insta360_normalize(int argc, char** argv) noexcept
{
    return Cfg{}.run(argc, argv);
}
