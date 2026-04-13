#include "slate_utils.hpp"
#include <opencv2/core.hpp>
#include <print>
#include <filesystem>
#include <vector>
#include <span>
#include <thread> // hardware_concurrency
#include <algorithm>
#include <charconv>
#include <cassert>
#include <string_view>
#include <fstream>
#include <sstream>
#include <ctime>
#include <utility>
#include <optional>
#include <atomic>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include <ThreadPool.hpp>
#include <version.hpp>
#include <ranges>
#include <algorithm>

#include "fs.hpp"
#include "string_utils.hpp"
#include "time_utils.hpp"


// TODO:
// Fisheye(OpenCV) calibration
// AprilTags

namespace fs = std::filesystem;

namespace {

auto& get_pool() noexcept
{
    static ThreadPool pool(
        []{
            const auto n = std::thread::hardware_concurrency();
            if (n > 0) return std::min(n, 16u);
            return 8u;
        }()
    );
    return pool;
}

std::optional<cv::Size> detect_checkerboard_size(const cv::Mat& img, int size_max) noexcept
{
    static constexpr int size_min = 3;
    if (size_max < size_min) {
        return std::nullopt;
    }
    //static constexpr int size_max = 20;
    std::atomic<bool> found{false};
    std::atomic<bool> multiple_found{false};
    cv::Size result{};
    auto& tp = get_pool();
    for (auto rows = size_max; rows >= size_min; --rows) {
        for (auto cols = rows; cols >= size_min; --cols) { // Skip duplicate sizes (e.g., 7x5 and 5x7)
            tp.enqueue(
                [&, rows, cols] {
                    if (found.load(std::memory_order_acquire)) {
                        return;
                    }
                    std::vector<cv::Point2f> out;
                    if (cv::findChessboardCorners(img, {cols, rows}, out)) {
                        // Multiple threads could reach here and each find corners sucessfully.
                        bool expected = false;
                        if (!found.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                            // Already found for another size. This is considered a failure.
                            multiple_found.store(true, std::memory_order_relaxed);
                            return;
                        }
                        result = {cols, rows};
                    }
                }
            );
        }
    }

    tp.wait_all();
    if (found.load(std::memory_order_relaxed) && !multiple_found.load(std::memory_order_relaxed)) {
        return result;
    }
    return std::nullopt;
}

std::vector<std::vector<cv::Point2f>>
detect_corners(std::span<const fs::path> imgs_path, cv::Size pattern_size, bool invert, bool sb, bool draw) noexcept // detection of corners is allowed to fail
{
    std::vector<std::vector<cv::Point2f>> result(imgs_path.size());

    auto& tp = get_pool();

    for (const auto& [img_path, out] : std::views::zip(imgs_path, result)) {
        tp.enqueue(
            [&] {
                auto img = cv::imread(img_path.string());
                if (img.empty())
                    return;
                cv::Mat img_mono;
                cv::cvtColor(img, img_mono, cv::COLOR_BGR2GRAY);
                if (invert) {
                    cv::bitwise_not(img_mono, img_mono);
                }

                //if (!cv::findChessboardCorners(img_mono, {cols, rows}, result[i])) {
                if (!(sb ?
                      cv::findChessboardCornersSB(img_mono, pattern_size, out, cv::CALIB_CB_EXHAUSTIVE + cv::CALIB_CB_NORMALIZE_IMAGE, cv::noArray())
                      : cv::findChessboardCorners(img_mono, pattern_size, out)))
                {
                    out.clear();
                    return;
                }
                // refine corner coordinates (not needed for findChessboardCornersSB)
                if (!sb)
                    cv::cornerSubPix(img_mono, out, {11, 11}, {-1, -1}, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.0001));
                if (draw) {
                    cv::drawChessboardCorners(img, pattern_size, out, true);
                    auto out_path = img_path;
                    out_path.replace_filename(img_path.stem() += "_detected.jpg");
                    cv::imwrite(out_path.string(), img);
                }
            }
        );
    }
    tp.wait_all();

    return result;
}

template<typename T>
requires requires (T x) { std::from_chars({}, {}, x); }
bool parse(std::string_view sv, T& val) noexcept
{
    const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    return res.ec == std::errc{} && res.ptr == sv.data() + sv.size();
}

const auto& help_text =
R"^^(🏁 calibrator: Utility to calibrate camera using chessboard patterns.

Usage: calibrator <INPUT> [--size ROWSxCOLS] [OPTIONS...]

Positional arguments:
  INPUT             Directory containing chessboard images for calibration.
                    Or, if -D is specified, a single image file to detect checkerboard size.

Options:
  -s, --size <ROWSxCOLS>  Specify the number of inner corners per a chessboard row and column.
                          If not specified, the program will attempt to determine the grid size
                          from the first image, using a grid search.
  -D, --detect            Detect and print the checkerboard size from the given INPUT image and exit.
  -e, --extension <EXT>   Specify the image file extension to look for (e.g., jpg, png).
                          If not specified, the program will try to guess one.
  -i, --invert            Invert the images before processing (useful if chessboard has black borders).
  -k, --k3                Use k3 distortion coeff. Otherwise k3 is fixed at 0.
  -S                      Use the more robust findChessboardCornersSB() for corner detection.
  -d, --draw-corners      Draw and save detected corners on images.
  -r, --report <FILE>     Output a calibration report to FILE.
  -m, --max-size <NUM>    Max size for grid search when auto detecting the pattern size. Default: 20.
                          Only sizes less than or equal to NUMxNUM will be searched.
  -V, --version           Display version information.
  -h, --help              Display this help message.
)^^";

template<typename T>
requires requires(T mat) {
    cv::format(mat, cv::Formatter::FMT_DEFAULT);
}
auto format_mat(const T& mat, cv::Formatter::FormatType fmt = cv::Formatter::FMT_DEFAULT)
{
    std::ostringstream ss;
    ss << cv::format(mat, fmt);
    return std::move(ss).str();
}

} // namespace


int slate::main_calibrator(int argc, char** argv) noexcept
try
{
    fs::path dir, img_ext, report;
    std::optional<cv::Size> pattern_size;
    bool invert = false; // findChessboardCorners() requires white borders
    bool use_k3 = false;
    bool corner_sb = false;
    bool draw_corners = false;
    bool detect_size = false;
    int  max_detect_size = 20;
    bool print_version = false;
    bool print_help = false;
    {
        const bool arg_status = [&] {
            if (argc < 2)
                return false;

            for (int i = 1; i < argc; ++i) {
                const auto opt = std::string_view{argv[i]};
                if (match(opt, "-i", "--invert")) {
                    invert = true;
                } else if (match(opt, "-k", "--k3", "-k3")) {
                    use_k3 = true;
                } else if (match(opt, "-S")) {
                    corner_sb = true;
                } else if (match(opt, "-d", "--draw-corners")) {
                    draw_corners = true;
                } else if (match(opt, "-D", "--detect")) {
                    detect_size = true;
                } else if (match(opt, "-e", "--extension")) {
                    if (i + 1 == argc) return false;
                    img_ext = argv[++i];
                } else if (match(opt, "-r", "--report")) {
                    if (i + 1 == argc) return false;
                    report = argv[++i];
                } else if (match(opt, "-s", "--size")) {
                    if (i + 1 == argc) return false;
                    const auto s = std::string_view{argv[++i]};
                    const auto x_pos = s.find('x');
                    if (x_pos == std::string_view::npos) return false;
                    int rows, cols;
                    if (!parse(s.substr(0, x_pos), rows) || !parse(s.substr(x_pos + 1), cols)) {
                        return false;
                    }
                    pattern_size.emplace(cols, rows);
                } else if (match(opt, "-m", "--max-size")) {
                    if (i + 1 == argc)
                        return false;
                    if (!parse(argv[++i], max_detect_size)) {
                        return false;
                    }
                } else if (match(opt, "-V", "--version")) {
                    print_version = true;
                    break;
                } else if (match(opt, "-h", "--help")) {
                    print_help = true;
                    break;
                } else { // positional arg
                    if (!dir.empty()) {
                        return false;
                    }
                    dir = opt;
                }
            }
            return !dir.empty() || print_help || print_version;
        }();

        if (!arg_status) {
            std::print(stderr, "Invalid parameters.\nPass -h for help.\n");
            return 2;
        }

        if (print_help) {
            std::print("{}", help_text);
            return 0;
        }

        if (print_version) {
            using namespace slate::version;
            print_info();
            return 0;
        }

        if (detect_size) {
            const auto input_file = dir.string();
            const auto input_image = cv::imread(input_file);
            if (!input_image.empty() && (pattern_size = detect_checkerboard_size(input_image, max_detect_size))) {
                std::print("Auto-detected checkerboard size: {}x{}\n", pattern_size->height, pattern_size->width);
                return 0;
            } else {
                std::print(stderr, "Could not autodetect checkerboard size from image: {}\n", input_file);
                return 1;
            }
        }

        // if (invert) {
        //     cout << "Images will be inverted!\n";
        // }
        // if (use_k3) {
        //     cout << "Using k3!\n";
        // }
        // if (corner_sb) {
        //     cout << "Using sb detection!\n";
        // }

        if (!report.empty()) {
            report = dir / report;
        }
    }

    if (img_ext.empty()) {
        img_ext = util::find_most_occuring_extension(dir);
    } else {
        img_ext = fs::path(".") += img_ext;
    }
    auto images = util::get_files(dir, img_ext);
    if (images.empty()) {
        std::print(stderr, "No images were found with extension: {}\n", img_ext.string());
        return 1;
    }

    cv::Size image_size;
    {
        const auto sample_img = cv::imread(images.front().string());
        if (sample_img.empty()) {
            std::println(stderr, "Could not read sample image");
            return 1;
        }
        image_size = {sample_img.cols, sample_img.rows};

        if (!pattern_size) {
            pattern_size = detect_checkerboard_size(sample_img, max_detect_size);
            if (!pattern_size) {
                std::println(stderr, "Could not auto-detect checkerboard size from sample image");
                return 1;
            }
            std::println("Auto-detected checkerboard size: {}x{}", pattern_size->height, pattern_size->width);
        }
    }

    auto corners = detect_corners(images, *pattern_size, invert, corner_sb, draw_corners);
    assert(images.size() == corners.size());
    const auto nb_input_images = (int)images.size();

    std::vector<fs::path> failed_images;

    for (int i = nb_input_images - 1; i >= 0; --i) { // Reverse order, so erase() won't invalidate later indices.
        if (corners[i].empty()) {
            std::println(stderr, "Could not detect patterns in {}", images[i].string());
            failed_images.push_back(images[i]);
            images.erase(images.begin() + i);
            corners.erase(corners.begin() + i);
        }
    }

    std::println("Found patterns in {} of {} input images", images.size(), nb_input_images);

    if (corners.empty()) {
        std::println(stderr, "No images for calibration");
        return 1;
    }

    const float square_size = 0.05f; // may be configurable
    const auto object_points = [&] {
        std::vector<cv::Point3f> pts;
        for (int i = 0; i < pattern_size->height; ++i) {
            for (int j = 0; j < pattern_size->width; ++j) {
                pts.emplace_back(j * square_size, i * square_size, 0.0f);
            }
        }
        return std::vector(corners.size(), pts);
    }();

    cv::Mat camera_matrix;
    std::vector<double> distortion_coef(5, 0.0); // k1, k2, p1, p2, k3
    //std::vector<cv::Mat> rvecs, tvecs;
    std::vector<double> per_view_error;
    const auto rms_error = cv::calibrateCamera(object_points, corners, image_size, camera_matrix, distortion_coef, cv::noArray(), cv::noArray(), cv::noArray(), cv::noArray(), per_view_error,
    use_k3 ? 0 : cv::CALIB_FIX_K3 // k3 stays zero
    );

    std::print("Camera Matrix:\n{}\nDistortion (k1, k2, p1, p2, k3): {:n}", format_mat(camera_matrix), distortion_coef);
    std::print("\nOverall RMS reprojection error: {:.5f}\n", rms_error);

    assert(images.size() == per_view_error.size());
    // zip sort by error
    std::ranges::sort(std::views::zip(images, per_view_error), [](const auto& t1, const auto& t2){ return std::get<1>(t1) > std::get<1>(t2); });
    // print reproj error for each image:
    std::print("\nPer-image reprojection error:\n");
    for (const auto& [image, val] : std::views::zip(images, per_view_error)) {
        std::println("{}: {:.5f}", image.string(), val);
    }

    if (!report.empty()) {
        std::ofstream ofs(report);
        if (!ofs) {
            std::println(stderr, "Failed to open report file: {}", report.string());
            return 1;
        }
        const auto local_time = []() {
            const auto t = std::time(nullptr);
            std::tm tm{};
            util::localtime(t, tm);
            return util::format_time(tm, "%Y-%m-%d %H:%M:%S");
        }();
        std::print(ofs, "# Calibrator report generated at: {}\n"
                        "# Image directory: {}\n"
                        "# Rows x Cols: {} x {}\n"
                        "# Invert: {}\n"
                        "# k3: {}\n"
                        "# Detection method: {}\n"
                        "# Detected images: {} / {}\n\n",
                    local_time, dir.string(), pattern_size->height, pattern_size->width, invert, use_k3, (corner_sb? 1 : 0),
                    images.size(), nb_input_images);

        if (!failed_images.empty()) {
            ofs << "Failed images:\n";
            for (const auto& img : failed_images) {
                ofs << img.string() << '\n';
            }
            ofs << '\n';
        }

        std::print(ofs,
                   "Image size:\n{}, {}\n\n"
                   "Camera matrix:\n{}\n"
                   "Distortion (k1, k2, p1, p2, k3):\n{:n}\n\n"
                   "RMS reprojection error:\n{:.5f}\n\n"
                   "Per-image reprojection error:\n",
                   image_size.width, image_size.height,
                   format_mat(camera_matrix, cv::Formatter::FMT_CSV),
                   distortion_coef,
                   rms_error);
        for (const auto& [img, val] : std::views::zip(images, per_view_error)) {
            std::println(ofs, "{}: {:.5f}", img.string(), val);
        }
    }

    return 0;
}
catch (const fs::filesystem_error& ex) {
    std::println(stderr, "{}", ex.what());
    return 1;
}
