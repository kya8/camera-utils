#include "detectors.hpp"
#include "gpmf_helper.hpp"
#include <GPMF_parser.h>
#include "slate/fourcc.hpp"
#include "slate/mp4/track.hpp"
#include "helper_templates.hpp"
#include <algorithm>
#include <range.hpp>

using namespace slate::mp4;

namespace slate {

bool
detect_gopro(Mp4Stream& file, CameraInfo& info, bool metadata_only) noexcept
{
    { // global GPMF
    std::vector<unsigned char> data;
    try {
        file.seek(0);
        // Get the moov/udta/GPMF box, which stores global metadata.
        auto atom = file.seek_to_atom_data("moov"_fc, file.get_length());
        atom = file.seek_to_atom_data("udta"_fc, atom);
        atom = file.seek_to_atom_data("GPMF"_fc, atom);

        data = file.read_atom_data(atom);
    } catch (const StreamError&) {
        return false;
    }

    info.vendor = CameraVendor::GoPro;  // Now we're sure it's a GoPro video.
    if (data.size() < sizeof(uint32_t)) return false;

    GpmfWrapper gs(data.data(), data.size());
    if (!gs) return false;

    do {
        const auto key = GPMF_Key(gs.get());
        if (eq_one(key, (uint32_t)STR2FOURCC("CASN"), (uint32_t)STR2FOURCC("MINF"), (uint32_t)STR2FOURCC("VFOV"), (uint32_t)STR2FOURCC("FMWR"), (uint32_t)STR2FOURCC("EISA"), (uint32_t)STR2FOURCC("OREN"))) {
            const auto nb_samples = GPMF_Repeat(gs.get());
            const auto bufsize = GPMF_FormattedDataSize(gs.get());
            std::vector<char> strbuf(bufsize + 1); // add NUL
            if (GPMF_FormattedData(gs.get(), strbuf.data(), bufsize, 0, nb_samples) == GPMF_OK) {
                switch(key) {
                case STR2FOURCC("MINF"):
                    info.extras[GroupId::NormalizedMetadata][KeyId::CameraModel]        = std::string(strbuf.data());
                    break;
                case STR2FOURCC("CASN"):
                    info.extras[GroupId::NormalizedMetadata][KeyId::SerialNumber]       = std::string(strbuf.data());
                    break;
                case STR2FOURCC("VFOV"):
                    info.extras[GroupId::NormalizedMetadata][KeyId::LensType]           = std::string(strbuf.data());
                    break;
                case STR2FOURCC("FMWR"):
                    info.extras[GroupId::NormalizedMetadata][KeyId::FirmwareVersion]    = std::string(strbuf.data());
                    break;
                case STR2FOURCC("EISA"):
                {
                    const std::string s = strbuf.data();
                    info.extras[GroupId::NormalizedMetadata][KeyId::StabilizationMode]  = s;
                    const bool has_stabilizaton = (s == "N/A" || s == "N")? false : true;
                    info.extras[GroupId::NormalizedMetadata][KeyId::HasStabilization]   = has_stabilizaton;
                    break;
                }
                case STR2FOURCC("OREN"):
                    info.extras[GroupId::NormalizedMetadata][KeyId::CameraRotation]     = std::string(strbuf.data());
                    break;
                default:
                    break;
                }
            }
        }
    } while (GPMF_OK == GPMF_Next(gs.get(), GPMF_RECURSE_LEVELS));

    if (metadata_only) return true;

    // insert global udta/GPMF box data
    static_assert(std::is_same_v<decltype(data), types::RawBytes>);
    info.extras[GroupId::Other]["GPMF"] = std::move(data);
    }

    // read and parse gpmd samples one by one
    const auto tracks = get_tracks(file);
    const auto gpmd_track = std::find_if(tracks.cbegin(), tracks.cend(), [](const auto& track) { return track.stsd_first_format == "gpmd"_fc; });
    if (gpmd_track == tracks.cend()) return true;
    SampleReader sample_reader(file, *gpmd_track);
    SampleReader::SampleInfo sample;

    std::vector<double> CORIs;
    std::vector<double> Gyros;
    bool err_cori = false, err_gyro = false;

    while (sample_reader.read_next(sample)) {
        if (sample.data.size() < sizeof(uint32_t)) return true;
        GpmfWrapper gs(sample.data.data(), sample.data.size());
        if (!gs) return true;

        do { // loop once
            const auto key = GPMF_Key(gs.get());
            if (key == (uint32_t)STR2FOURCC("CORI") && !err_cori) {
                [[maybe_unused]] const auto dtype         = GPMF_Type(gs.get());
                const auto nb_axis       = GPMF_ElementsInStruct(gs.get());
                const auto sample_repeat = GPMF_Repeat(gs.get());
                if (nb_axis != 4) {
                    err_cori = true;
                    continue;
                }
                const auto old_size = CORIs.size();
                CORIs.resize(old_size + sample_repeat * nb_axis);
                if (GPMF_ScaledData(gs.get(), CORIs.data() + old_size, sample_repeat * nb_axis * sizeof(double), 0, sample_repeat, GPMF_TYPE_DOUBLE) != GPMF_OK) {
                    err_cori = true;
                }
            } else if (key == (uint32_t)STR2FOURCC("GYRO") && !err_gyro) {
                const auto sample_repeat = GPMF_Repeat(gs.get());
                constexpr auto nb_axis = 3;
                const auto old_size = Gyros.size();
                Gyros.resize(old_size + sample_repeat * nb_axis);
                if (GPMF_ScaledData(gs.get(), Gyros.data() + old_size, sample_repeat * nb_axis * sizeof(double), 0, sample_repeat, GPMF_TYPE_DOUBLE) != GPMF_OK) {
                    err_gyro = true;
                }
            }
        } while (GPMF_OK == GPMF_Next(gs.get(), GPMF_RECURSE_LEVELS));
    }

    if (!CORIs.empty() && !err_cori) {
        auto& cori_quaternions = info.extras[GroupId::SensorData][KeyId::CameraQuaternionData].emplace<types::QuaternionVec>();
        // convert to standard(OpenCV) camera frame
        // Quaternions in GPMF is stored as W,-X,-Y,-Z, relative to opencv frame.
        for (const auto id_quat : range::make_index(CORIs.size() / 4)) {
            cori_quaternions.emplace_back(  // x,y,z,w
                CORIs[4 * id_quat + 1] * -1,
                CORIs[4 * id_quat + 2] * -1,
                CORIs[4 * id_quat + 3] * -1,
                CORIs[4 * id_quat]
            );
        }
    }

    if (!Gyros.empty() && !err_gyro) {
        auto& gyro_data = info.extras[GroupId::SensorData][KeyId::GyroData].emplace<types::GyroVec>();
        for (const auto id_gyro : range::make_index(Gyros.size() / 3)) {
            // ZXY in GoPro frame, or yxz(-Y, -X, -Z) in OpenCV frame; Unit is rad/s.
            // https://github.com/gopro/gpmf-parser/issues/165#issuecomment-1207241564
            gyro_data.emplace_back(  // t,x,y,z
                0.0, // FIXME: Not implemented yet
                Gyros[3 * id_gyro + 1] * -1,
                Gyros[3 * id_gyro + 0] * -1,
                Gyros[3 * id_gyro + 2] * -1
            );
        }
    }

    return true;
}

} // namespace slate
