#ifndef LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD
#define LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD

#include <string_view>
#include <string>
#include <optional>
#include "varmap.hpp"

namespace slate::loupe {

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

} // namespace slate::loupe


#endif /* LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD */
