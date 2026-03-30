#include "track.hpp"
#include "fourcc.hpp"
#include <optional>
#include "helper_templates.hpp"

using std::uint32_t, std::uint64_t, std::int32_t, std::uint8_t;

namespace mp4utils {

namespace {

std::optional<TrackInfo>
parse_trak(Mp4Stream& file, const Mp4Stream::AtomInfo& trak_atom) noexcept try {
{
    if (trak_atom.fourcc != "trak"_fc) return {};
    TrackInfo track_info;

    //tkhd
    {
        file.seek_to_atom_data("tkhd"_fc, trak_atom);
        uint8_t ver; uint32_t _flag;
        file.read_num(ver);
        file.read_num<Endian::BE, uint32_t, 3>(_flag);
        if (ver == 1) {
            file.seek(8+8, SeekFrom::Current);
            file.read_num(track_info.trak_id);
            file.seek(4, SeekFrom::Current);
            file.read_num(track_info.tkhd_duration);
        } else { // version == 0
            file.seek(4+4, SeekFrom::Current);
            file.read_num(track_info.trak_id);
            file.seek(4, SeekFrom::Current);
            uint32_t duration;
            file.read_num(duration);
            track_info.tkhd_duration = duration;
        }
        file.seek(4*2+2+2+2+2, SeekFrom::Current);
        for(auto i=0u; i<9; ++i) {
            file.read_num(track_info.tkhd_matrix[i]);
        }
        file.read_num(track_info.tkhd_width);
        file.read_num(track_info.tkhd_height);
    }

    const auto mdia_atom = file.seek_to_atom_data("mdia"_fc, trak_atom);
    //mdhd
    {
        file.seek_to_atom_data("mdhd"_fc, mdia_atom);
        uint8_t ver; uint32_t _flag;
        file.read_num(ver);
        file.read_num<Endian::BE, uint32_t, 3>(_flag);
        file.seek(ver==1? 8+8 : 4+4, SeekFrom::Current);
        file.read_num(track_info.mdhd_timescale);
        if (ver==1) {
            file.read_num(track_info.mdhd_duration);
        } else {
            uint32_t duration;
            file.read_num(duration);
            track_info.mdhd_duration = duration;
        }
    }
    //hdlr
    {
        file.seek_to_atom_data("hdlr"_fc, mdia_atom);
        uint8_t ver; uint32_t _flag;
        file.read_num(ver);
        file.read_num<Endian::BE, uint32_t, 3>(_flag);
        file.seek(4, SeekFrom::Current);
        file.read_num(track_info.hdlr_type);
    }
    const auto minf_atom = file.seek_to_atom_data("minf"_fc, mdia_atom);
    // vmhd/smhd, dinf...
    const auto stbl_atom = file.seek_to_atom_data("stbl"_fc, minf_atom);

    //stsd
    {
        file.seek_to_atom_data("stsd"_fc, stbl_atom);
        file.seek(4, SeekFrom::Current);
        file.read_num(track_info.sample_desc_count);
        file.seek(4, SeekFrom::Current);
        file.read_num(track_info.stsd_first_format);
    }

    //loop over boxes in stbl
    //Existence of required atoms is not checked.
    file.seek(stbl_atom.data_offset());
    while(file.tell() < (OffsetType)stbl_atom.end_offset()) { // int64_t?
        const auto atom = file.parse_atom();
        uint8_t ver; uint32_t _flag; // not used here...
        file.read_num(ver);
        file.read_num<Endian::BE, uint32_t, 3>(_flag);

        if (atom.fourcc == "stsz"_fc) { // `stz2' is not supported
            file.read_num(track_info.stsz_sample_size);
            file.read_num(track_info.stsz_count);
            if (track_info.stsz_sample_size == 0) {
                for (auto i = 0u; i < track_info.stsz_count; ++i) {
                    uint32_t size;
                    file.read_num(size);
                    track_info.stsz.push_back(size);
                }
            }
        }

        else if (eq_one(atom.fourcc, "stco"_fc, "co64"_fc, "stts"_fc, "stsc"_fc)) {
            uint32_t count;
            file.read_num(count);
            while(count-- > 0) {
                if (atom.fourcc == "stts"_fc) {
                    uint32_t consecutive_samples, sample_duration;
                    file.read_num(consecutive_samples);
                    file.read_num(sample_duration);
                    track_info.stts.emplace_back(consecutive_samples, sample_duration);
                }
                else if (atom.fourcc == "stco"_fc) {
                    uint32_t chunk_offset;
                    file.read_num(chunk_offset);
                    track_info.stco.push_back(chunk_offset);
                }
                else if (atom.fourcc == "co64"_fc) {
                    uint64_t chunk_offset;
                    file.read_num(chunk_offset);
                    track_info.stco.push_back(chunk_offset);
                }
                else if (atom.fourcc == "stsc"_fc) {
                    uint32_t first_chunk, samples_per_chunk, sample_desc_id;
                    file.read_num(first_chunk);
                    file.read_num(samples_per_chunk);
                    file.read_num(sample_desc_id);
                    track_info.stsc.emplace_back(first_chunk, samples_per_chunk, sample_desc_id);
                }
            }
        }

        file.seek(atom.end_offset());
    }

    return track_info;
}
} catch (const StreamError&) {
    return {};
}

} /* unnamed ns */


std::vector<TrackInfo>
get_tracks(Mp4Stream& file) noexcept try {
{
    file.seek(0);
    std::vector<TrackInfo> result_tracks;

    const auto moov = file.seek_to_atom_data("moov"_fc, file.get_length());
    const auto atoms_in_moov = file.get_all_atoms(moov.data_size());

    for (const auto& atom : atoms_in_moov) {
        if (atom.fourcc == "trak"_fc) {
            const auto info = parse_trak(file, atom);
            if (info) result_tracks.push_back(std::move(*info));
        }
    }

    return result_tracks;
}
} catch (const StreamError&) {
    return {};
}

SampleReader::SampleReader(Mp4Stream& file, const TrackInfo& track_info) noexcept : file(file), track_info(track_info) {}

bool
SampleReader::read_next(SampleInfo& out) noexcept try {
{
    if (sample_idx >= track_info.stsz_count || chunk_idx >= track_info.stco.size()) return false;
    if (track_info.stsz_sample_size == 0 && sample_idx >= track_info.stsz.size()) return false;
    // in case of stsz_sample_size being 0, stsz_count and stsz.size() should be equal.

    // look up stts
    {
        uint64_t total_count = 0; uint64_t total_duration = 0;
        auto it = track_info.stts.cbegin();
        for(; it < track_info.stts.cend(); ++it) {
            const auto& [count, duration] = *it;
            if (total_count + count > sample_idx) break;
            total_count += count;
            total_duration += duration * count;
        }
        if (it == track_info.stts.cend()) return false;
        const auto current_duration = std::get<1>(*it);
        out.duration = double(current_duration) / track_info.mdhd_timescale;
        out.timestamp = double(total_duration + current_duration*(sample_idx-total_count)) / track_info.mdhd_timescale;
    }
    // find stsc
    if (track_info.stsc.empty()) return false;
    auto it = track_info.stsc.cbegin();
    for(; it < track_info.stsc.cend(); ++it) {
        const auto i1 = std::get<0>(*it) - 1;
        if (it + 1 == track_info.stsc.cend()) {
            if (chunk_idx >= i1) break;
        }
        else {
            const auto i2 = std::get<0>(*(it+1)) - 1;
            if (chunk_idx >= i1 && chunk_idx < i2) break;
        }
    }
    if (it == track_info.stsc.cend()) return false;

    const auto& [_first_chunk_id, samples_in_chunk, sample_desc_id] = *it;
    if (sample_idx_in_chunk >= samples_in_chunk) return false;
    out.sample_desc_id = sample_desc_id;
    out.sample_idx     = sample_idx;

    uint64_t sample_offset_in_chunk = 0;
    for(auto i = sample_idx - sample_idx_in_chunk; i < sample_idx; ++i) {
        sample_offset_in_chunk += (track_info.stsz_sample_size == 0? track_info.stsz[i] : track_info.stsz_sample_size);
    }
    file.seek(track_info.stco[chunk_idx] + sample_offset_in_chunk);
    out.data.resize(track_info.stsz[sample_idx]);
    file.read(out.data.data(), out.data.size());

    //update state for next read:
    ++sample_idx;
    if (sample_idx_in_chunk + 1 == samples_in_chunk) {
        ++chunk_idx;
        sample_idx_in_chunk = 0;
    }
    else {
        ++sample_idx_in_chunk;
    }

    return true;
}
} catch (const StreamError&) {
    return false;
}

} /* namespace mp4utils */
