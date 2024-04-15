#ifndef CAMINFO_HPP_DDC8321B_6771_4D29_9CAD_D97FD52277B3
#define CAMINFO_HPP_DDC8321B_6771_4D29_9CAD_D97FD52277B3

#include <string_view>
#include <string>
#include <optional>
#include "varmap.hpp"

namespace caminfo {

enum class CameraVendor {
    GoPro,
    Insta360,
    Unknown
};

std::string_view get_vendor_name(CameraVendor type) noexcept;


struct CameraInfo {
    CameraVendor vendor = CameraVendor::Unknown;
    GroupedVarMap extras;

    std::string describe() const noexcept;
};

std::optional<CameraInfo> detect(const char* video_file, bool metadata_only = false) noexcept;

}


#endif /* CAMINFO_HPP_DDC8321B_6771_4D29_9CAD_D97FD52277B3 */
