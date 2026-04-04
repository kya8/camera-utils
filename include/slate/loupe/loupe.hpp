#ifndef LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD
#define LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD

#include <string_view>
#include <string>
#include <optional>
#include "varmap.hpp"
#include "slate/export.h"

namespace slate::loupe {

enum class CameraVendor {
    GoPro,
    Insta360,
    Unknown
};

SLATE_EXPORT std::string_view to_string(CameraVendor type) noexcept;

struct CameraInfo {
    CameraVendor vendor = CameraVendor::Unknown;
    GroupedVarMap extras;

    SLATE_EXPORT std::string describe() const noexcept;
};

SLATE_EXPORT std::optional<CameraInfo> detect(const char* video_file, bool metadata_only = false) noexcept;

} // namespace slate::loupe


#endif /* LOUPE_HPP_A388713C_5F71_41F5_B27B_49EB6D325FFD */
