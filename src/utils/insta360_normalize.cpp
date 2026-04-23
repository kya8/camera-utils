#include "slate_utils.hpp"
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <slate/detect.hpp>
#include <slate/extra/insta360_params.hpp>
#include <slate/extra/insta360_tf.hpp>
#include <print>
#include <cstdio>
#include <fstream>
#include <tuple>
#include <cmath>        // std::tan
#include <Eigen/Geometry>
#include <thread>       // hardware_concurrency()
#include <memory>
#include <numbers>
#include <algorithm>
#include <cassert>
#include "ThreadPool.hpp"
#include "string_utils.hpp"
#include "version.hpp"
#include "sys_utils.hpp"
#include "fs.hpp"

using std::numbers::pi, std::numbers::inv_pi;

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

struct CvMaps {
    cv::Mat_<float> mapx, mapy;
};

CvMaps get_perspective_map(cv::Size size, double f, double cx, double cy, auto&& proj) noexcept
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

CvMaps get_equirectangular_map(int height, auto&& proj) noexcept
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

// Project from normalized image coordinate to EAC uv coordinate [0, 1]
// EAC projection: https://blog.google/products-and-platforms/products/google-ar-vr/bringing-pixels-front-and-center-vr-video/
[[maybe_unused]] double project_eac(double x)
{
    return 2 * inv_pi * std::atan(x) + 0.5;
}

// Map from EAC uv coordinate [0, 1] to normalized image coordinate.
double inverse_eac(double x)
{
    return std::tan((x - 0.5) * pi * 0.5);
};

/**************
 | F | L | U |
 | B | R | D |
***************/
static constexpr std::string_view cube_order_default = "flubrd";

// cube_order is assumed to be valid
CvMaps get_cubemap(int size, auto&& proj, bool equi_angular, std::string_view cube_order) noexcept
{
    assert(std::ranges::is_permutation(cube_order, cube_order_default) && "invalid cube_order");

    static const struct {
        char id;
        Eigen::AngleAxisd rot;
    } lut_faces[] {
        // front
        {'f', Eigen::AngleAxisd::Identity()},
        // back
        {'b', Eigen::AngleAxisd{pi, Eigen::Vector3d::UnitY()}},
        // left
        {'l', Eigen::AngleAxisd{-pi / 2, Eigen::Vector3d::UnitY()}},
        // right
        {'r', Eigen::AngleAxisd{pi / 2, Eigen::Vector3d::UnitY()}},
        // up
        {'u', Eigen::AngleAxisd{pi / 2, Eigen::Vector3d::UnitX()}},
        // down
        {'d', Eigen::AngleAxisd{-pi / 2, Eigen::Vector3d::UnitX()}}
    };

    const auto c = (size - 1) / 2.0; // The optical center is at the exact center of pixels.
    const auto f = c;

    cv::Mat_<float> mapx(2 * size, 3 * size);
    cv::Mat_<float> mapy(2 * size, 3 * size);

    for (auto i = 0uz; i < cube_order.size(); ++i) {
        // For each face in the order string, find its destination row/col in the grid,
        // and its associated direction(rotation).
        const auto R = i / 3;
        const auto C = i % 3;
        const auto& rot = std::ranges::find(lut_faces, cube_order[i], [](const auto& x){ return x.id; })->rot;

        const Eigen::Quaterniond quat{rot};
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const auto xn = equi_angular? inverse_eac(double(col) / size) : (col - c) / f;
                const auto yn = equi_angular? inverse_eac(double(row) / size) : (row - c) / f;
                const Eigen::Vector3d pt = quat * Eigen::Vector3d{xn, yn, 1.0};
                const auto uv  = proj(pt.x(), pt.y(), pt.z());
                mapx(R*size + row, C*size + col) = static_cast<float>(uv[0]);
                mapy(R*size + row, C*size + col) = static_cast<float>(uv[1]);
            }
        }
    }

    return {mapx, mapy};
}

enum Mode {
    Split = 0,
    Equirectangular,
    Cubemap
};

const auto& help_message =
R"^^(🌐 insta360_normalize: Undistort images from insta360 cameras.

Usage: insta360_normalize <-v FILE> [-0 DIRECTORY] [-1 DIRECTORY] [OPTIONS...]

Options:
 -v, --video <FILE>     Primary .insv video file.
 -0, --dir0 <DIR>       Image directory for lens 0 or joint video.
 -1, --dir1 <DIR>       Secondary image directory for lens 1, in case the videos are split.
 -f, --fov <FOV>        Horizontal FOV angle (degrees, full view) of undistorted image. Default: 90.
                        Only applicable for perspective projection.
 -w, --width <NUM>      Output image width. Default: original video width.
     --no-crop          Disable cropping. (For testing only)
 -T, --threads <NUM>    Number of threads to use. Default is 0 (auto).
 -E, --eqr              Equirectangular projection.
 -C, --cube             Cubemap projection. The output image is a 2x3 grid of square faces.
     --eac              Use Equi-Angular projection for cubemap.
     --cube-order <S>   Specify order of faces in the cubemap. S should be a permutation of "fblrud",
                        meaning front, back, left, right, up, down respectively. S maps to subimages
                        in the cubemap grid in row-first order. Default: flubrd.
     --tf               Specify transform from lens0 to lens1, in the form of x,y,z,qx,qy,qz,qw.
                        If not specified, built-in transform from video metadata is used.
 -F, --format <FORMAT>  Output image format, e.g. jpg/png. Default: jpg.
 -V, --version          Display version information.
 -h, --help             Display this help message.
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
    bool eac = false;
    std::string_view cubemap_order = cube_order_default;
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
            if (match(argv[i], "-v", "--video")) {
                if (check_arg()) {
                    video_file = argv[++i];
                }
            } else if (match(argv[i], "-l", "-0", "--dir0", "-d0")) {
                if (check_arg()) {
                    dir[0] = argv[++i];
                }
            } else if (match(argv[i], "-r", "-1", "--dir1", "-d1")) {
                if (check_arg()) {
                    dir[1] = argv[++i];
                }
            } else if (match(argv[i], "-f", "--fov")) {
                if (check_arg()) {
                    char* str_end;
                    fov_x = std::strtod(argv[++i], &str_end);
                    if (str_end == argv[i]) err_flag = 1;
                }
            } else if (match(argv[i], "-w", "--width")) {
                if (check_arg()) {
                    char* str_end;
                    output_width = std::strtol(argv[++i], &str_end, 0);
                    if (str_end == argv[i]) err_flag = 1;
                }
            } else if (match(argv[i], "--no-crop")) {
                with_crop = false;
            } else if (match(argv[i], "-T", "--threads")) {
                if (check_arg()) {
                    char* str_end;
                    nb_threads = std::strtoul(argv[++i], &str_end, 0);
                    if (str_end == argv[i]) err_flag = 1;
                }
            } else if (match(argv[i], "-E", "--eqr")) {
                mode = Equirectangular;
            } else if (match(argv[i], "-C", "--cube")) {
                mode = Cubemap;
            } else if (match(argv[i], "--cube-order")) {
                if (check_arg()) {
                    cubemap_order = argv[++i];
                }
            } else if (match(argv[i], "--eac")) {
                eac = true;
            } else if (match(argv[i], "--tf")) {
                if (check_arg()) {
                    const auto v = parse_csv(argv[++i]);
                    if (v.size() != 7) {
                        err_flag = 1;
                    } else {
                        has_tf = true;
                        tf = Eigen::Translation3d(v[0], v[1], v[2]) * Eigen::Quaterniond(v.data() + 3);
                    }
                }
            } else if (match(argv[i], "-V", "--version")) {
                print_version = true;
                break;
            } else if (match(argv[i], "-h", "--help")) {
                print_help = true;
                break;
            } else if (match(argv[i], "-F", "--format")) {
                if (check_arg()) {
                    output_image_format = argv[++i];
                }
            }
            ++i;
        }

        return !err_flag;
    }

    bool process_dir(int lens_id, const slate::insta360::Params& params, auto& pool) noexcept try
    {
        if (mode) lens_id = 0;
        const auto& lens = params.lens[lens_id];

        const auto output_width  = this->output_width > 0? this->output_width : params.width;
        const auto output_height = output_width * params.height / params.width;
        const auto cx = (output_width - 1) / 2.0;
        const auto cy = (output_height - 1) / 2.0;
        const auto f = cx / std::tan(fov_x / 2 * 0.01745329251994329577);

        const auto maps = [&] {
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
                    return get_cubemap(params.height / 2, proj, eac, cubemap_order);
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
            std::println(stderr, "Invalid image directory.");
            return false;
        }
        const auto output_dir = work_dir / (lens_id == 0? "normalized_0" : "normalized_1");
        const auto image_files = util::get_most_occuring_extension_files(work_dir);
        if (image_files.empty()) {
            std::println(stderr, "No images were found!");
            return false;
        }
        {
            std::error_code ec;
            fs::create_directories(output_dir, ec);
            if (ec) {
                std::println(stderr, "Cannot create output directory.");
                return 1;
            }
        }

        // Process images
        const auto total_images = image_files.size();
        std::println("{} images to process...", total_images);
        std::size_t cnt_done = 0, prev_done = 0, cnt_bad = 0;
        std::condition_variable cond;
        std::mutex mtx;

        // Push work in a dedicated thread,
        // so the main thread can proceed to terminal output.
        std::thread push_work([&]{
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
                        std::println(stderr, "Failure reading image {}", src.string());
                        return 1;
                    }

                    // check image dimension...

                    cv::Mat out;
                    cv::remap(in, out, maps.mapx, maps.mapy, cv::InterpolationFlags::INTER_LINEAR);
                    const auto dst = (output_dir / src.filename().replace_extension(output_image_format)).string();
                    if (!imwrite(dst, out, {cv::IMWRITE_JPEG_QUALITY, 90})) {
                        std::println(stderr, "Failure writing output file {}", dst);
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
        });

        for (;;) {
            {
                std::unique_lock lk(mtx);
                cond.wait(lk, [&] { return cnt_done > prev_done || cnt_done == 0; });
                prev_done = cnt_done;
            }
            std::string out {"\r"};
            out.append(std::to_string(prev_done)).append(" / ").append(std::to_string(total_images));
            std::fputs(out.c_str(), stdout);
            std::fflush(stdout);
            if (prev_done == total_images) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        pool.wait_all();
        push_work.join();
        if (mode != Mode::Split)
            std::print("{}", (mode == Equirectangular? "\nEquirectangular" : "\nCubemap"));
        else
            std::print("\nLens {}", lens_id);
        std::print(" finished: {} in total, {} failed.\n", total_images, cnt_bad);

        if (mode == Mode::Split) {
            std::ofstream ofs(output_dir / "normalized_camera_params.txt");
            if (!ofs) {
                std::println(stderr, "Error creating params file.");
            }
            else {
                std::println(ofs, "Output image size: {}*{}", output_width, output_height);
                std::println(ofs, "Lens {}: Undistorted fx, fy, cx, cy: {}, {}, {}, {}", lens_id, f, f, cx, cy);
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
            using namespace slate::version;
            print_info();
            return 0;
        }

        if (print_help) {
            std::fputs(help_message, stdout);
            return 0;
        }

        if (!args_ok || video_file.empty() || (dir[0].empty() && dir[1].empty())) {
            std::print(stderr, "Invalid argument.\nPass '-h' for help.\n");
            return 2;
        }

        // Check cubemap order
        if (mode == Mode::Cubemap && !std::ranges::is_permutation(cubemap_order, cube_order_default)) {
            std::println(stderr, "'{}' is not a valid cubemap order.", cubemap_order);
            return 2;
        }

        if (mode && dir[0].empty()) {
            std::println(stderr, "Equirectangular/Cubemap projection requires full imagery.");
            return 2;
        }

        if (fov_x >= 180 || fov_x <= 0) {
            std::println(stderr, "Invalid FOV angle!");
            return 2;
        }

        if (!cv::haveImageWriter(std::string{"."} += output_image_format)) {
            std::println(stderr, "Unsupported output image format: {}", output_image_format);
            return 1;
        }

        using namespace slate;

        insta360::Params params;
        const auto cam_info = detect(video_file.c_str(), true);
        if (!cam_info || !insta360::get_params(*cam_info, params, with_crop)) {
            std::println(stderr, "Could not read camera info from video file {}.", video_file);
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
                std::println("Using built-in lenses transform: {}", std::tie(t_01->x(), t_01->y(), t_01->z(),
                                                                             r_01.x(), r_01.y(), r_01.z(), r_01.w()));
                return true;
            }();
            if (!has_tf) {
                std::println(stderr, "Could not extract built-in lenses transform, which is required.");
                return 1;
            }
        }
        const auto camera_model = cam_info->extras.get_or<types::String>("Unknown", GroupId::NormalizedMetadata, KeyId::CameraModel);
        const auto camera_SN    = cam_info->extras.get_or<types::String>("Unknown", GroupId::NormalizedMetadata, KeyId::SerialNumber);
        std::print("Video file: {}\nCamera model: {}\nSN: {}\nResolution: {} x {}; Number of lenses: {}; Video is joined: {}\n",
                   video_file, camera_model, camera_SN, params.width, params.height, params.nb_lens, params.joined);
        if (mode == Mode::Split) {
            std::println("Using fov_x angle: {} deg", fov_x);
        } else if (mode == Mode::Cubemap) {
            std::println("Cubemap projection with EAC={}, face order '{}'.", eac, cubemap_order);
        } else if (mode == Mode::Equirectangular) {
            std::println("Equirectangular projection.");
        }

        if (params.selfie) {
            std::println(stderr, "Bad lenses direction.");
            return 1;
        }

        if (params.joined && !dir[1].empty()) {
            std::println(stderr, "Secondary image directory provided, but video is joined.");
            return 1;
        }
        if (!dir[1].empty() && params.nb_lens < 2) {
            std::println(stderr, "Only one lens was detected!");
            return 1;
        }
        if (mode && !params.joined && dir[1].empty()) {
            std::println(stderr, "Missing secondary image directory.");
            return 1;
        }

        auto pool = ThreadPool([&]{
            if (nb_threads > 0) return nb_threads;
            const auto n = std::thread::hardware_concurrency();
            return n? n : 8u;
        }(), 32);

        if (mode) {
            process_dir(0, params, pool);
        }
        else {
            if (!dir[0].empty()) process_dir(0, params, pool);
            if (!dir[1].empty() || params.joined) process_dir(1, params, pool);
        }

        return 0;
    }
};

} // unnamed namespace

int slate::main_insta360_normalize(int argc, char** argv) noexcept
{
    return Cfg{}.run(argc, argv);
}
