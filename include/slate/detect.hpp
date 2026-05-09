#ifndef DETECT_HPP_BCD5DE1F_5C62_460F_95D1_D0A5D9B18F61
#define DETECT_HPP_BCD5DE1F_5C62_460F_95D1_D0A5D9B18F61

#include "camera_info.hpp"
#include <optional>
#include "slate/export.h"

namespace slate {

SLATE_EXPORT [[nodiscard]] std::optional<CameraInfo> detect(const char* video_file, bool metadata_only = false) noexcept;

} // namespace slate


#endif /* DETECT_HPP_BCD5DE1F_5C62_460F_95D1_D0A5D9B18F61 */
