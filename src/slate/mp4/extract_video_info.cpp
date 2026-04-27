#include "extract_video_info.hpp"
#include "track.hpp"
#include "slate/fourcc.hpp"
#include <optional>
#include <range.hpp>

//NOT-Implemented: handle edit lists; Multiple stsd sample desc entries; frames per sample.

namespace slate::mp4 {

namespace {
// we might want to export this function
std::optional<Mp4Stream::AtomInfo>
seek_atom_recursive(Mp4Stream& file, std::initializer_list<std::uint32_t> fourcc_list, std::uint64_t max_range) noexcept // we could pass a (templated by size) array by ref, instead of using initializer_list.
{
    if (fourcc_list.size() == 0) return {};
    Mp4Stream::AtomInfo atom;
    try {
        for (auto it = fourcc_list.begin(); it != fourcc_list.end(); ++it) {
            const auto range = it==fourcc_list.begin()? max_range : atom.data_size();
            atom = file.seek_to_atom_data(*it, range);
        }
    } catch (const StreamError&) {
        return {};
    }

    return atom;
}

} // namespace

bool
extract_video_info(Mp4Stream& file, VideoInfo& out, types::VecI32* video_track_ids) noexcept try
{
    file.seek(0);
    const auto mvhd = seek_atom_recursive(file, {"moov"_fc, "mvhd"_fc}, file.get_length());
    if (!mvhd) return false;
    const auto mvhd_timescale = [&] {
        std::uint8_t ver; std::uint32_t _flag;
        file.read_num(ver);
        file.read_num<Endian::BE, std::uint32_t, 3>(_flag);
        file.seek(ver==1? 8+8 : 4+4, SeekFrom::Current);
        std::uint32_t timescale;
        file.read_num(timescale);
        return timescale;
    }();

    const auto tracks = get_tracks(file);
    types::VecI32 video_track_indices;
    for (const auto i : range::make_index(tracks.size())) {
        if (tracks[i].hdlr_type == "vide"_fc)
            video_track_indices.push_back(static_cast<int>(i));
    }
    if (video_track_indices.empty()) return false;
    const auto& video_track = tracks[video_track_indices.front()];
    if (video_track_ids) *video_track_ids = std::move(video_track_indices);
    // Currently we only look at the first video track.

    // duration, geometry
    out.duration = double(video_track.tkhd_duration) / mvhd_timescale; // tkhd_duration includes edit lists.
    out.width  = video_track.tkhd_width  >> 16;
    out.height = video_track.tkhd_height >> 16;
    const auto a = video_track.tkhd_matrix[0] >> 16;
    const auto b = video_track.tkhd_matrix[1] >> 16;
    const auto c = video_track.tkhd_matrix[3] >> 16;
    const auto d = video_track.tkhd_matrix[4] >> 16;
    if      (std::tie(a,b,c,d) == std::forward_as_tuple( 1, 0, 0, 1)) out.display_rotation = 0;
    else if (std::tie(a,b,c,d) == std::forward_as_tuple( 0, 1,-1, 0)) out.display_rotation = 90;
    else if (std::tie(a,b,c,d) == std::forward_as_tuple(-1, 0, 0,-1)) out.display_rotation = 180;
    else if (std::tie(a,b,c,d) == std::forward_as_tuple( 0,-1, 1, 0)) out.display_rotation = 270;
    else out.display_rotation = -1;

    // samples/frames
    // TODO: handle frames_per_sample in stsd desc
    out.nb_frames = video_track.stsz_count;
    //if (video_track.stsz_count < 2) return false;

    const auto& stts = video_track.stts;
    if (stts.empty()) return false;
    bool cfr = true;
    const auto first_delta = std::get<1>(stts.front());
    std::uint64_t stts_samples = 0;
    std::uint64_t stts_total_duration= 0;
    for (std::size_t i = 0; i < stts.size(); ++i) {
        const auto& [count, delta] = stts[i];
        const bool is_single_last = (i == stts.size() - 1 && count == 1);
        if ( delta != first_delta && !is_single_last ) { // if only the last sample/frame has differente delta, consider it to be CFR
            cfr = false;
        }
        if (!(is_single_last && delta == 0)) { // last delta can be zero
            stts_samples += count;
            stts_total_duration += std::uint64_t(delta) * count;
        }
    }
    if (stts_total_duration == 0) return false;
    out.is_cfr = cfr;
    out.fps = double(stts_samples) / stts_total_duration * video_track.mdhd_timescale;

    return true;
}
catch (const StreamError&) { return false; }

} // namespace slate::mp4
