#ifndef EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194
#define EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194

#include "mp4.hpp"
#include "slate/video_info.hpp"
#include <vector>

namespace slate::mp4 {

/**
 * Extract video info from an MP4 stream.
 * @param[in] file  An MP4 stream opened for reading.
 * @param[out] out  Video info to write to.
 * @param[out] video_track_ids  Optional reference to an output vector for the indices of video tracks in the MP4.
 *
 * @return Whether the extraction was successful.
 */
bool extract_video_info(Mp4Stream& file, VideoInfo& out, std::vector<int>* video_track_ids = nullptr) noexcept;

} /* namespace slate::mp4 */

#endif /* EXTRACT_VIDEO_INFO_HPP_A1AC426B_0713_4083_84FA_F81637795194 */
