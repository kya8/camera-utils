#include "camera_utils.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread> // hardware_concurrency
#include <algorithm>
#include <charconv>
#include <cassert>
#include <string_view>
#include <fstream>
#include <ctime>
#include <utility>
#include <optional>
#include <atomic>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include <ThreadPool.hpp>
#include <version.hpp>
#include <range.hpp>

#include "fs.hpp"
#include <time_utils.hpp>


// TODO:
// Fisheye(OpenCV) calibration
// AprilTags

namespace fs = std::filesystem;

using range::make_index;

namespace {

auto& get_pool() noexcept
{
    static ThreadPool<void> pool(
        []{
            const auto n = std::thread::hardware_concurrency();
            if (n > 0) return std::min(n, 16u);
            return 8u;
        }()
    );
    return pool;
}

std::optional<cv::Size> detect_checkerboard_size(const cv::Mat& img) noexcept
{
    static constexpr int size_min = 4;
    static constexpr int size_max = 20;
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
                        found.store(true, std::memory_order_release);
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
detect_corners(const std::vector<fs::path>& imgs_path, cv::Size pattern_size, bool invert, bool sb, bool draw) noexcept // detection of corners is allowed to fail
{
    std::vector<std::vector<cv::Point2f>> result(imgs_path.size());

    auto& tp = get_pool();

    for (const auto i : make_index(imgs_path.size())) {
        tp.enqueue(
            [&, i] {
                auto img = cv::imread(imgs_path[i].string());
                if (img.empty())
                    return;
                cv::Mat img_mono;
                cv::cvtColor(img, img_mono, cv::COLOR_BGR2GRAY);
                if (invert) {
                    cv::bitwise_not(img_mono, img_mono);
                }

                //if (!cv::findChessboardCorners(img_mono, {cols, rows}, result[i])) {
                if (!(sb ?
                      cv::findChessboardCornersSB(img_mono, pattern_size, result[i], cv::CALIB_CB_EXHAUSTIVE, cv::noArray())
                      : cv::findChessboardCorners(img_mono, pattern_size, result[i]))) {
                    result[i].clear();
                    return;
                }
                // refine corner coordinates (not needed for findChessboardCornersSB)
                if (!sb)
                    cv::cornerSubPix(img_mono, result[i], {11, 11}, {-1, -1}, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.0001));
                if (draw) {
                    cv::drawChessboardCorners(img, pattern_size, result[i], true);
                    auto out = imgs_path[i];
                    out.replace_filename(imgs_path[i].stem() += "_detected.jpg");
                    cv::imwrite(out.string(), img);
                }
            }
        );
    }
    tp.wait_all();

    return result;
}

template<typename T>
bool parse(std::string_view sv, T& val) noexcept
{
    const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    return res.ec == std::errc{} && res.ptr == sv.data() + sv.size();
}

const auto& help_text =
R"^^(calibrator: Utility to calibrate camera using chessboard patterns.

Usage: calibrator <INPUT> [--size ROWSxCOLS] [OPTIONS...]

Positional arguments:
  INPUT             Directory containing chessboard images for calibration.
                    Or, if -D is specified, a single image file to detect checkerboard size.

Options:
  --size ROWSxCOLS  Specify the number of inner corners per a chessboard row and column.
                    If not specified, the program will attempt to determine the grid size
                    from the first image, using a grid search.
  -D                Detect and print the checkerboard size from the given INPUT image and exit.
  -e EXT            Specify the image file extension to look for (e.g., jpg, png).
                    If not specified, the program will try to guess one.
  -i                Invert the images before processing (useful if chessboard has black borders).
  -k3               Use k3 distortion coeff. Otherwise k3 is fixed at 0.
  -s                Use the more robust findChessboardCornersSB() for corner detection.
  -d                Draw and save detected corners on images.
  -r FILE           Output a calibration report to FILE.
  -V                Display version information.
  -h                Display this help message.
)^^";

} // namespace


int main_calibrator(int argc, char** argv) noexcept
try
{
    using std::cout, std::cerr;

    fs::path dir, img_ext, report;
    std::optional<cv::Size> pattern_size;
    bool invert = false; // findChessboardCorners() requires white borders
    bool use_k3 = false;
    bool corner_sb = false;
    bool draw_corners = false;
    bool detect_size = false;
    bool print_version = false;
    bool print_help = false;
    {
        const bool arg_status = [&] {
            if (argc < 2)
                return false;

            for (int i = 1; i < argc; ++i) {
                const auto opt = std::string_view{argv[i]};
                if (opt == "-i") {
                    invert = true;
                }
                else if (opt == "-k3") {
                    use_k3 = true;
                } else if (opt == "-s") {
                    corner_sb = true;
                } else if (opt == "-d") {
                    draw_corners = true;
                } else if (opt == "-D") {
                    detect_size = true;
                } else if (opt == "-e") {
                    if (i + 1 == argc) return false;
                    img_ext = argv[++i];
                } else if (opt == "-r") {
                    if (i + 1 == argc) return false;
                    report = argv[++i];
                } else if (opt == "--size") {
                    if (i + 1 == argc) return false;
                    const auto s = std::string_view{argv[++i]};
                    const auto x_pos = s.find('x');
                    if (x_pos == std::string_view::npos) return false;
                    int rows, cols;
                    if (!parse(s.substr(0, x_pos), rows) || !parse(s.substr(x_pos + 1), cols)) {
                        return false;
                    }
                    pattern_size.emplace(cols, rows);
                } else if (opt == "-V") {
                    print_version = true;
                    break;
                } else if (opt == "-h") {
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
            cerr << "Invalid parameters.\nPass -h for help.\n";
            return 2;
        }
        
        if (print_help) {
            cout << help_text;
            return 0;
        }

        if (print_version) {
            using namespace camera_utils::version;
            print_info();
            return 0;
        }

        if (detect_size) {
            const auto input_file = dir.string();
            const auto input_image = cv::imread(input_file);
            if (!input_image.empty() && (pattern_size = detect_checkerboard_size(input_image))) {
                cout << "Autodetected checkerboard size: " << pattern_size->height << "x" << pattern_size->width << '\n';
                return 0;
            } else {
                cerr << "Could not autodetect checkerboard size from image: " << input_file << '\n';
                return 1;
            }
        }

        if (invert) {
            cout << "Images will be inverted!\n";
        }
        if (use_k3) {
            cout << "Using k3!\n";
        }
        if (corner_sb) {
            cout << "Using sb detection!\n";
        }

        if (!report.empty()) report = dir / report;
    }

    if (img_ext.empty()) {
        img_ext = util::find_most_occuring_extension(dir);
    } else {
        img_ext = fs::path(".") += img_ext;
    }
    auto images = util::get_files(dir, img_ext);
    if (images.empty()) {
        cerr << "No images were found with extension: " << img_ext << '\n';
        return 1;
    }

    cv::Size image_size;
    {
        const auto sample_img = cv::imread(images.front().string());
        if (sample_img.empty()) {
            cerr << "Could not read image\n";
            return 1;
        }
        image_size = {sample_img.cols, sample_img.rows};

        if (!pattern_size) {
            pattern_size = detect_checkerboard_size(sample_img);
            if (!pattern_size) {
                cerr << "Could not autodetect checkerboard size\n";
                return 1;
            }
            cout << "Autodetected checkerboard size: " << pattern_size->height << "x" << pattern_size->width << '\n';
        }
    }

    auto corners = detect_corners(images, *pattern_size, invert, corner_sb, draw_corners);
    assert(images.size() == corners.size());
    const auto nb_input_images = (int)images.size();

    std::vector<fs::path> failed_images;

    for (int i = nb_input_images - 1; i >= 0; --i) { // Reverse order, so erase() won't invalidate later indices.
        if (corners[i].empty()) {
            cout << "Could not detect patterns in " << images[i].string() << '\n';
            failed_images.push_back(images[i]);
            images.erase(images.begin() + i);
            corners.erase(corners.begin() + i);
        }
    }

    cout << "Found patterns in " << images.size() << " of " << nb_input_images << " input images\n";

    if (corners.empty()) {
        cerr << "No images for calibration\n";
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

    cout << "Camera Matrix:\n" << camera_matrix << '\n'
         << "Distortion (k1, k2, p1, p2, k3):";
    for (const auto& x : distortion_coef) {
        cout << ' ' << x;
    }
    cout << "\nOverall RMS reprojection error: " << rms_error << '\n';

    assert(images.size() == per_view_error.size());
    // print reproj error for each image:
    cout << "\nPer-image reprojection error:\n";
    for (const auto i : make_index(images.size())) {
        cout << images[i].string() << ": " << per_view_error[i] << '\n';
    }

    if (!report.empty()) {
        std::ofstream ofs(report);
        if (!ofs) {
            cerr << "Failed to open report file: " << report << '\n';
            return 1;
        }
        const auto datetime = []() -> std::string {
            char buf[64]{0};
            const auto t = std::time(nullptr);
            std::tm tm;
            if (util::localtime(t, tm) != 0 || std::strftime(buf, 64, "%Y-%m-%d %H:%M:%S", &tm) == 0) {
                return "";
            }
            return buf;
        }();
        ofs << std::boolalpha;
        ofs << "# Calibrator report generated at: " << datetime << '\n'
            << "# Image directory: " << dir << '\n'
            << "# Rows x Cols: " << pattern_size->height << " x " << pattern_size->width << '\n'
            << "# Invert: " << invert << '\n'
            << "# k3: " << use_k3 << '\n'
            << "# Detection method: " << (corner_sb ? 1 : 0) << "\n"
            << "# Detected images: " << images.size() << " / " << nb_input_images << '\n'
            << '\n';

        if (!failed_images.empty()) {
            ofs << "Failed images:\n";
            for (const auto& img : failed_images) {
                ofs << img.string() << '\n';
            }
            ofs << '\n';
        }

        ofs << "Image size:\n"
            << image_size.width << ", " << image_size.height << "\n\n"
            << "Camera matrix:\n" << cv::format(camera_matrix, cv::Formatter::FMT_CSV) << '\n'
            << "Distortion (k1, k2, p1, p2, k3):\n" << cv::format(distortion_coef, cv::Formatter::FMT_CSV) << "\n\n"
            << "RMS reprojection error:\n" << rms_error << "\n\n"
            << "Per-image reprojection error:\n";
        for (const auto i : make_index(images.size())) {
            ofs << images[i].string() << ": " << per_view_error[i] << '\n';
        }
    }

    return 0;
}
catch (const fs::filesystem_error& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
}
