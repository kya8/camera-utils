#ifndef INSTA360_TF_HPP_BDD826EF_DD54_498E_879E_3CA75D80320D
#define INSTA360_TF_HPP_BDD826EF_DD54_498E_879E_3CA75D80320D

#include <Eigen/Core>
#include <caminfo_fwd.hpp>
#include <optional>

namespace caminfo::insta360 {

const Eigen::Matrix3d* get_gyro_to_body_transform(const caminfo::CameraInfo& info) noexcept;

// Coord transform from body frame to lens0 and lens1 (rotational part)
std::optional<Eigen::Matrix3d> get_R0(const double* offset_v3, int len) noexcept;
std::optional<Eigen::Matrix3d> get_R1(const double* offset_v3, int len) noexcept;
// Coord transform from lens0 to lens1 (translational part)
std::optional<Eigen::Vector3d> get_T01(const double* offset_v3, int len) noexcept;

} // namespace caminfo::insta360


#endif /* INSTA360_TF_HPP_BDD826EF_DD54_498E_879E_3CA75D80320D */
