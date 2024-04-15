#ifndef EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194
#define EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194

#include "mp4.hpp"
#include "video_info.hpp"
#include <vector>

namespace mp4utils {

bool extract_video_info(Mp4Stream& file, VideoInfo& out, std::vector<int>* video_track_ids = nullptr) noexcept;

} /* namespace mp4utils */

#endif /* EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194 */
