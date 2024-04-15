#include "caminfo.hpp"
#include "detectors.hpp"
#include <sstream>
#include "extract_video_info.hpp"

using mp4utils::Mp4Stream;

namespace caminfo {

std::optional<CameraInfo>
detect(const char* video_file, bool metadata_only) noexcept
{
    Mp4Stream file;
    if(!file.open(video_file)) {
        return {};
    }

    CameraInfo result{};
    const auto detect_success = [&] {
        if(detect_gopro   (file, result, metadata_only)) return true;
        if(detect_insta360(file, result, metadata_only)) return true;
        return false;
    }();

    mp4utils::VideoInfo video_info{};
    std::vector<int> video_track_ids;
    const auto has_video_info = mp4utils::extract_video_info(file, video_info, &video_track_ids);

    if(!detect_success && !has_video_info) return {};
    if(has_video_info) {
        result.extras[GroupId::VideoInfo][KeyId::Duration]         = video_info.duration;
        result.extras[GroupId::VideoInfo][KeyId::Width]            = types::Int(video_info.width);
        result.extras[GroupId::VideoInfo][KeyId::Height]           = types::Int(video_info.height);
        result.extras[GroupId::VideoInfo][KeyId::DisplayRotation]  = types::Int(video_info.display_rotation);
        result.extras[GroupId::VideoInfo][KeyId::FrameCount]       = types::Int(video_info.nb_frames);
        result.extras[GroupId::VideoInfo][KeyId::FPS]              = video_info.fps;
        result.extras[GroupId::VideoInfo][KeyId::IsCFR]            = video_info.is_cfr;
        result.extras[GroupId::VideoInfo][KeyId::VideoTrackIds]    = std::move(video_track_ids);
    }

    return result;
}

std::string_view
get_vendor_name(CameraVendor type) noexcept
{
    switch(type)
    {
    case(CameraVendor::Unknown):   return "Unknown";
    case(CameraVendor::GoPro):     return "GoPro";
    case(CameraVendor::Insta360):  return "Insta360";
    }

    return {};
}

std::string
CameraInfo::describe() const noexcept
{
    std::ostringstream ret;
    ret << "Camera Vendor: " << get_vendor_name(vendor) << "; ";

    const auto metadata = extras.find(GroupId::NormalizedMetadata);
    if(metadata != extras.cend()) {
        for (const auto& [key, val] : metadata->second) {
            ret << key_to_string(key) << ": " << var_to_string(val) << "; ";
        }
    }

    ret << "Extra info: " << extras.size();

    return ret.str();
}

} // namespace caminfo
