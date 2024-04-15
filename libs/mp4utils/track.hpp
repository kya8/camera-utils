#ifndef TRACK_HPP_B8710831_6B74_4E3B_AB2A_A407EC7E9F81
#define TRACK_HPP_B8710831_6B74_4E3B_AB2A_A407EC7E9F81

#include "mp4.hpp"
#include <vector>
#include <cstdint>
#include <tuple>

namespace mp4utils {

struct TrackInfo {
    std::uint32_t trak_id;  // track_ID in tkhd, starts from 1
    std::uint64_t tkhd_duration;
    std::uint32_t tkhd_width;
    std::uint32_t tkhd_height;
    std::int32_t  tkhd_matrix[9];

    std::uint64_t mdhd_duration;
    std::uint32_t mdhd_timescale;
    //std::uint64_t elst_segment_duration;

    std::uint32_t hdlr_type; // 'vide', 'soun', ...
                             // or check for minf/vmhd, minf/smhd
    std::uint32_t sample_desc_count;
    std::uint32_t stsd_first_format; // format of the first sample description entry in stsd

    std::vector<std::tuple<std::uint32_t, std::uint32_t>> stts; // sample count, sample duration
    std::vector<uint32_t> stsz;                                 // if stsz_sample_size is 0
    std::uint32_t stsz_sample_size;
    std::uint32_t stsz_count;
    std::vector<std::uint64_t> stco;
    std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> stsc; // first_chunk, samples_per_chunk, sample_description_id

};

std::vector<TrackInfo> get_tracks(Mp4Stream& file) noexcept;

// read samples

class SampleReader {
public:
    SampleReader(Mp4Stream& file, const TrackInfo& track_info) noexcept;

    struct SampleInfo {
        std::uint32_t sample_idx;
        std::uint32_t sample_desc_id;
        double timestamp;
        double duration;
        std::vector<unsigned char> data;
    };
    bool read_next(SampleInfo& out) noexcept;
private:
    Mp4Stream& file;
    const TrackInfo& track_info;
    std::uint32_t sample_idx = 0;
    std::uint32_t chunk_idx  = 0;
    std::uint32_t sample_idx_in_chunk = 0;
    // We could optimize more by storing more state info here...
};

} /* namespace mp4utils */

#endif /* TRACK_HPP_B8710831_6B74_4E3B_AB2A_A407EC7E9F81 */
