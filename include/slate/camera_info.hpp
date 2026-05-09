#ifndef CAMERA_INFO_HPP_C29E77D3_4AC8_45FB_BF9C_74643D794575
#define CAMERA_INFO_HPP_C29E77D3_4AC8_45FB_BF9C_74643D794575

#include <string_view>
#include <string>
#include "varmap.hpp"
#include "slate/export.h"

namespace slate {

enum class CameraVendor {
    GoPro,
    Insta360,
    Unknown
};

SLATE_EXPORT [[nodiscard]] std::string_view to_string(CameraVendor type) noexcept;

struct CameraInfo {
    CameraVendor vendor = CameraVendor::Unknown;
    GroupedVarMap extras;

    SLATE_EXPORT [[nodiscard]] std::string describe() const noexcept;
};

} // namespace slate

#endif /* CAMERA_INFO_HPP_C29E77D3_4AC8_45FB_BF9C_74643D794575 */
