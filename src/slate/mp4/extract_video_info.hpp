#ifndef EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194
#define EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194

#include "mp4.hpp"
#include "slate/video_info.hpp"
#include <slate/value_types.hpp>

namespace slate::mp4 {

bool extract_video_info(Mp4Stream& file, VideoInfo& out, types::VecI32* video_track_ids = nullptr) noexcept;

} /* namespace slate::mp4 */

#endif /* EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194 */
