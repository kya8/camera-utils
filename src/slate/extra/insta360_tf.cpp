#include <slate/extra/insta360_tf.hpp>
#include <slate/detect.hpp>
#include <Eigen/Geometry>
#include <numbers>

using namespace std::numbers;

static constexpr double d2r = pi / 180.0;

namespace slate {

const Eigen::Matrix3d*
insta360::get_gyro_to_body_transform(const CameraInfo& info) noexcept
{
    const auto& map = info.extras;
    const auto& model_name = map.get_or<std::string>("", GroupId::NormalizedMetadata, KeyId::CameraModel);
    const auto& sub_model = map.get_or<std::string>("", GroupId::NormalizedMetadata, KeyId::SubModel);

    if (model_name == "Insta360 OneRS" && sub_model == "1-Inch 360 Edition") {
        static const Eigen::Matrix3d mat {
            { 0, -1,  0},
            {-1,  0,  0},
            { 0,  0, -1}
        };
        return &mat;
    }
    else if (model_name == "Insta360 X3") {
        static const Eigen::Matrix3d mat {
            { 0, -1,  0},
            { 0,  0,  1},
            {-1,  0,  0}
        };
        return &mat;
    }
    else if (model_name == "Insta360 X4") {
        static const Eigen::Matrix3d mat {
            { 0,  1,  0},
            { 0,  0, -1},
            {-1,  0,  0}
        };
        return &mat;
    }
    else {
        return nullptr;
    }
}

std::optional<Eigen::Matrix3d>
insta360::get_R0(const double* offset_v3, int len) noexcept
{
    if (len < 20) return {};
    const auto [yaw, pitch, roll] = std::tie(offset_v3[6], offset_v3[7], offset_v3[8]);
    if (len <= 21) {
        return Eigen::Matrix3d {
            Eigen::AngleAxisd(pi/2 + pitch*d2r, Eigen::Vector3d::UnitY()) *
            Eigen::AngleAxisd(yaw*d2r,                Eigen::Vector3d::UnitZ()) *
            Eigen::AngleAxisd(roll*d2r,               Eigen::Vector3d::UnitX())
        };
    }
    // only for dual-lenses camera
    return Eigen::Matrix3d {
        Eigen::AngleAxisd(pi/2 + pitch*d2r, Eigen::Vector3d::UnitY()) *
        Eigen::AngleAxisd(yaw*d2r,          Eigen::Vector3d::UnitZ()) *
        Eigen::AngleAxisd(roll*d2r,         Eigen::Vector3d::UnitX()) *
        Eigen::AngleAxisd(pi,               Eigen::Vector3d::UnitZ())
    };
}

std::optional<Eigen::Matrix3d>
insta360::get_R1(const double* offset_v3, int len) noexcept
{
    if (len < 40) return {};
    const auto [yaw, pitch, roll] = std::tie(offset_v3[25], offset_v3[26], offset_v3[27]);
    return Eigen::Matrix3d {
        Eigen::AngleAxisd(pi/2 + pitch*d2r, Eigen::Vector3d::UnitY()) *
        Eigen::AngleAxisd(yaw*d2r,          Eigen::Vector3d::UnitZ()) *
        Eigen::AngleAxisd(roll*d2r,         Eigen::Vector3d::UnitX())
    };
}

std::optional<Eigen::Vector3d>
insta360::get_T01(const double* offset_v3, int len) noexcept
{
    if (len < 40) return {};
    return Eigen::Vector3d{offset_v3[28], offset_v3[29], offset_v3[30]};
}

} // namespace slate
