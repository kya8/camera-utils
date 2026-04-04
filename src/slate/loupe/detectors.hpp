#ifndef DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB
#define DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB

#include <slate/loupe/loupe.hpp>
#include "slate/mp4/mp4.hpp"

// All detectors should return true if the specified camera's signature is found. Otherwise, false is returned.

namespace slate::loupe {

bool
detect_gopro(mp4::Mp4Stream& file, CameraInfo& info, bool metadata_only = false) noexcept;

bool
detect_insta360(mp4::Mp4Stream& file, CameraInfo& info, bool metadata_only = false) noexcept;


using DetectFuncType = decltype(&detect_gopro);
constexpr DetectFuncType detect_functions[] {&detect_gopro, &detect_insta360};  // compile-time loop over enum specializations?
constexpr auto nb_detect_functions = sizeof(detect_functions) / sizeof(*detect_functions);

} // namespace slate::loupe


#endif /* DETECTORS_HPP_A7EBA89B_4DF5_4E6E_AF1B_50B9D248BEAB */
