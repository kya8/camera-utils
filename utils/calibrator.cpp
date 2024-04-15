#include "camera_utils.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread> // hardware_concurrency
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include <string_view>
#include <fstream>
#include <ctime>

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

std::vector<std::vector<cv::Point2f>>
detect_corners(const std::vector<fs::path>& imgs_path, int rows, int cols, bool invert, bool sb, bool draw) noexcept // detection of corners is allowed to fail
{
    std::vector<std::vector<cv::Point2f>> result(imgs_path.size());

    ThreadPool<void> tp(
        [] {
            const auto n = std::thread::hardware_concurrency();
            if (n > 0) return std::min(n, 16u);
            return 8u;
        }()
    );

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
                      cv::findChessboardCornersSB(img_mono, {cols, rows}, result[i], cv::CALIB_CB_EXHAUSTIVE, cv::noArray())
                      : cv::findChessboardCorners(img_mono, {cols, rows}, result[i]))) {
                    result[i].clear();
                    return;
                }
                // refine corner coordinates (not needed for findChessboardCornersSB)
                if (!sb)
                    cv::cornerSubPix(img_mono, result[i], {11, 11}, {-1, -1}, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.0001));
                if (draw) {
                    cv::drawChessboardCorners(img, {rows, cols}, result[i], true);
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

} // namespace


int main_calibrator(int argc, char** argv) noexcept
try
{
    using std::cout, std::cerr;

    fs::path dir, img_ext, report, out_brief;
    int rows, cols;
    bool invert = false; // findChessboardCorners() requires white borders
    bool use_k3 = false;
    bool corner_sb = false;
    bool draw_corners = false;
    {
        const bool arg_status = [&] {
            if (argc < 4) return false;

            dir = argv[1];

            char* e1, *e2;
            errno = 0;
            rows = std::strtol(argv[2], &e1, 10);
            cols = std::strtol(argv[3], &e2, 10);
            if (e1 == argv[2] || e2 == argv[3] || errno || rows <= 0 || cols <= 0) return false;

            for (int i = 4; i < argc; ++i) {
                const auto opt = std::string_view{argv[i]};
                if (opt == "-i") {
                    invert = true;
                }
                else if (opt == "-k3") {
                    use_k3 = true;
                }
                else if (opt == "-s") {
                    corner_sb = true;
                }
                else if (opt == "-d") {
                    draw_corners = true;
                }
                else if (opt == "-e") {
                    if (i + 1 == argc) return false;
                    img_ext = argv[++i];
                }
                else if (opt == "-r") {
                    if (i + 1 == argc) return false;
                    report = argv[++i];
                }
                else if (opt == "-o") {
                    if (i + 1 == argc) return false;
                    out_brief = argv[++i];
                }
                else {
                    return false;
                }
            }
            return true;
        }();

        if (!arg_status) {
            using namespace camera_utils::version;
            cerr << "Usage: calibrator <dir> <rows> <cols> [-e EXT] [-i] [-k3] [-s] [-d] [-r FILE] [-o FILE]\n"
                 << "Version: " << COMMIT_HASH << ", " << COMMIT_DATE << '\n';
            return 2;
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

        if (!out_brief.empty()) out_brief = dir / out_brief;
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

    auto corners = detect_corners(images, rows, cols, invert, corner_sb, draw_corners);
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
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                pts.emplace_back(j * square_size, i * square_size, 0.0f);
            }
        }
        return std::vector(corners.size(), pts);
    }();

    const cv::Size image_size = [&] { // (W, H)
        const auto img = cv::imread(images.front().string());
        return cv::Size{img.cols, img.rows};
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
            << "# Rows / Cols: " << rows << " / " << cols << '\n'
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

    if (!out_brief.empty()) {
        std::ofstream ofs(out_brief);
        if (!ofs) {
            cerr << "Failed to open output file: " << out_brief << '\n';
            return 1;
        }
        ofs << cv::format(camera_matrix, cv::Formatter::FMT_CSV)
            << cv::format(distortion_coef, cv::Formatter::FMT_CSV) << '\n'
            << image_size.width << ", " << image_size.height << '\n';
    }

    return 0;
}
catch (const fs::filesystem_error& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
}
