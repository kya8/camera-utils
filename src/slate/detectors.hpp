#ifndef DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB
#define DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB

#include <slate/camera_info.hpp>
#include "slate/mp4/mp4.hpp"

// All detectors should return true if the specified camera's signature is found. Otherwise, false is returned.

namespace slate {

bool
detect_gopro(mp4::Mp4Stream& file, CameraInfo& info, bool metadata_only = false) noexcept;

bool
detect_insta360(mp4::Mp4Stream& file, CameraInfo& info, bool metadata_only = false) noexcept;

} // namespace slate


#endif /* DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB */
