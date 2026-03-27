#include "detectors.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include "insta360_metadata.pb.h"
#include "helper_templates.hpp"
#include <cmath>
#include <map>
#include <utility>
#include <format>

using namespace mp4utils;

namespace {

constexpr unsigned char magic[] = "8db42d694ccc418790edff439fe026bf";
constexpr int magic_len = sizeof(magic) - 1;

enum RecordType : std::uint8_t {
    Offsets            = 0,
    Metadata           = 1,
    Thumbnail          = 2,
    Gyro               = 3,
    Exposure           = 4,
    ThumbnailExt       = 5,
    TimelapseTimestamp = 6,
    Gps                = 7,
    StarNum            = 8,
    AAAData            = 9,
    Anchors            = 10,
    AAASimulation      = 11,
    ExposureSecondary  = 12,
    Magnetic           = 13,
    Euler              = 14,
    SecGyro            = 15,
    Speed              = 16,
    TBox               = 17,
    Quaternions        = 18,
    TimeMap            = 128,
};

enum RecordFormat : std::uint8_t {
    Binary   = 0,
    Protobuf = 1,
    Json     = 2,
};

std::vector<double>
parse_offset_string(const std::string& offset)
{
    std::vector<double> result;
    constexpr auto delim = '_';
    std::string::size_type p1 = 0, p2 = 0;
    while(p1 < offset.size()) {
        if ((p2 = offset.find(delim, p1)) == offset.npos) {
            p2 = offset.size();
        }
        try {
            const auto d = std::stod(offset.substr(p1, p2-p1));
            result.push_back(d);
        }
        catch(const std::invalid_argument&) {
            return {};
        }
        catch(const std::out_of_range&) {
            return {};
        }
        p1 = p2 + 1;
    }
    return result;
}

} /* unnamed namespace */


namespace caminfo {

namespace {

void insert_protobuf_metadata(CameraInfo& info,
                              const google::protobuf::Message& message,
                              const std::string& prepend)
{
    const auto desc = message.GetDescriptor();
    const auto refl = message.GetReflection();
    const bool is_top_level = prepend.empty();
    for (auto i = 0; i < desc->field_count(); ++i) {
        const auto field_desc = desc->field(i);
        const auto field_name = field_desc->name();
        const auto cpp_type = field_desc->cpp_type();
        const auto type     = field_desc->type();
        if (!field_desc->is_optional())
            continue;
        if (!refl->HasField(message, field_desc))
            continue;
        if (is_top_level && eq_one(field_name, "offset", "offset_v2", "offset_v3"))
            continue;

        const auto concat_name = auto(prepend) += field_name;
        switch(cpp_type) {
        using T  = google::protobuf::FieldDescriptor::CppType;
        using T_ = google::protobuf::FieldDescriptor::Type;
        case(T::CPPTYPE_INT32):
            info.extras[GroupId::Metadata][concat_name] = (types::Int)refl->GetInt32(message, field_desc);
            break;
        case(T::CPPTYPE_INT64):
            info.extras[GroupId::Metadata][concat_name] = (types::Int)refl->GetInt64(message, field_desc);
            break;
        case(T::CPPTYPE_UINT32):
            info.extras[GroupId::Metadata][concat_name] = (types::UInt)refl->GetUInt32(message, field_desc);
            break;
        case(T::CPPTYPE_UINT64):
            info.extras[GroupId::Metadata][concat_name] = (types::UInt)refl->GetUInt64(message, field_desc);
            break;
        case(T::CPPTYPE_DOUBLE):
            info.extras[GroupId::Metadata][concat_name] = refl->GetDouble(message, field_desc);
            break;
        case(T::CPPTYPE_FLOAT):
            info.extras[GroupId::Metadata][concat_name] = refl->GetFloat(message, field_desc);
            break;
        case(T::CPPTYPE_BOOL):
            info.extras[GroupId::Metadata][concat_name] = refl->GetBool(message, field_desc);
            break;
        case(T::CPPTYPE_STRING):
            if (type == T_::TYPE_STRING) {
                info.extras[GroupId::Metadata][concat_name] = refl->GetString(message, field_desc);
            } else if (type == T_::TYPE_BYTES) {
                const auto str = refl->GetString(message, field_desc);
                types::RawBytes vec(str.cbegin(), str.cend());
                info.extras[GroupId::Metadata][concat_name] = std::move(vec);
            }
            break;
        case(T::CPPTYPE_ENUM):
        {
            const auto enum_desc = field_desc->enum_type();
            const auto enum_num = refl->GetEnumValue(message, field_desc);
            const auto enum_val_desc = enum_desc->FindValueByNumber(enum_num); // nullptr if num is invalid
            std::string display_name = std::format("{} ({})", (enum_val_desc ? enum_val_desc->name() : "Unknown value"),  enum_num);
            info.extras[GroupId::Metadata][concat_name] = std::move(display_name);
            break;
        }
        case(T::CPPTYPE_MESSAGE):
        {
            const auto& sub_msg = refl->GetMessage(message, field_desc);
            //const auto sub_msg_desc = field_desc->message_type();
            const auto new_prepend = concat_name + '.';
            insert_protobuf_metadata(info, sub_msg, new_prepend);
            break;
        }
        default:
            break;
        }
    }
}

} // namespace

// end of mp4 | data | 32(unknown padding) + 4(size of data) + 4(version) | 32(magic) | EOF

bool
detect_insta360(mp4utils::Mp4Stream& file, CameraInfo& info, bool metadata_only) noexcept try
{
    file.seek(-magic_len, SeekFrom::End);
    unsigned char buf[magic_len] {0};
    file.read(buf, magic_len);
    if (std::memcmp(buf, magic, magic_len)) return false;

    file.seek(-magic_len-4-4, SeekFrom::End);
    std::uint32_t extra_size, _version;
    file.read_nums<Endian::LE>(extra_size, _version);
    info.vendor = CameraVendor::Insta360;

    // keep some info for re-use
    [[maybe_unused]] bool has_metadata = false;      // Have we already encountered and parsed the metadata block?
    bool is_raw_gyro  = false;      // Older models may lack this field, default to false.
    double t_scale = 1e-3;
    double first_frame_t = 0.0;

    std::map<RecordType, std::pair<std::uint32_t, std::uint32_t>> offsets; // (id, (offset, size)). offset is begin of data block

    std::int64_t offset = magic_len + 32+4+4 + 1+1+4;
    file.seek(-offset + 1, SeekFrom::End);
    std::uint8_t first_id;
    file.read_num<Endian::LE>(first_id);
    if (first_id == RecordType::Offsets) { // the `offsets` block specifies offsets for subsequent fields
        std::uint32_t size;
        file.read_num<Endian::LE>(size);
        file.seek(-offset - size, SeekFrom::End);
        std::uint32_t read = 0;
        while ((read += 1+1+4+4) <= size) {
            std::uint8_t id, _format;
            std::uint32_t size, offset;
            file.read_nums<Endian::LE>(id, _format, size, offset);
            if (id > 0) offsets[(RecordType)id] = {offset, size};
        }

        if (offsets.empty()) return false;
    }

    auto iter_offsets = offsets.cbegin();
    const auto end_offsets = offsets.cend();
    while (1) {
        std::uint8_t format, id;
        std::uint32_t size;

        if (offsets.empty()) { // No offsets
            if (offset >= (std::int64_t)extra_size) break;
            file.seek(-offset, SeekFrom::End);
            file.read_nums<Endian::LE>(format, id, size);
            file.seek(-offset - size, SeekFrom::End);
            offset += (size + 1+1+4); // next block header start
        }
        else {
            if (iter_offsets == end_offsets) break;
            const auto data_start = -(std::int64_t)extra_size + iter_offsets->second.first;
            file.seek(data_start + iter_offsets->second.second, SeekFrom::End); // seek to header start
            file.read_nums<Endian::LE>(format, id, size);
            const bool skip = id != iter_offsets->first || size != iter_offsets->second.second;
            ++iter_offsets;
            if (skip) continue;
            file.seek(data_start, SeekFrom::End);
        }

        if (id == RecordType::Metadata && format == RecordFormat::Protobuf) { // Normally, this should be the first record.
            std::vector<unsigned char> buf(size);
            file.read(buf.data(), buf.size());
            Insta360Metadata metadata;
            if (metadata.ParseFromArray(buf.data(), (int)buf.size())) {

                insert_protobuf_metadata(info, metadata, "");

                // ...
                if (metadata.has_offset()) {
                    info.extras[GroupId::Metadata]["offset"] = parse_offset_string(metadata.offset());
                }
                if (metadata.has_offset_v2()) {
                    info.extras[GroupId::Metadata]["offset_v2"] = parse_offset_string(metadata.offset_v2());
                }
                if (metadata.has_offset_v3()) {
                    info.extras[GroupId::Metadata]["offset_v3"] = parse_offset_string(metadata.offset_v3());
                }

                const auto normalize_metadata = [&map = info.extras](const VarMap::key_type& key, KeyId key_normalized, GroupId from = GroupId::Metadata) {
                    const auto group = map.find(from);
                    if (group == map.cend()) return false;
                    const auto it = group->second.find(key);
                    if (it == group->second.cend()) return false;
                    map[GroupId::NormalizedMetadata][key_normalized] = std::move(it->second);
                    group->second.erase(it);
                    return true;
                };
                normalize_metadata("camera_type",         KeyId::CameraModel);
                normalize_metadata("serial_number",       KeyId::SerialNumber);
                normalize_metadata("fov_type",            KeyId::LensType);
                normalize_metadata("fw_version",          KeyId::FirmwareVersion);
                normalize_metadata("is_flowstate_online", KeyId::HasStabilization);
                normalize_metadata("cam_posture",         KeyId::CameraRotation);

                // Sub-model
                if (info.extras.get_or<std::string>(GroupId::NormalizedMetadata, KeyId::CameraModel, "") == "Insta360 OneRS") {
                    const auto offset_v3 = info.extras.get<types::VecD>(GroupId::Metadata, "offset_v3");
                    const auto offset = info.extras.get<types::VecD>(GroupId::Metadata, "offset");
                    if (offset_v3 && offset_v3->size() == 40 && int((*offset_v3)[19]) == 62) {
                        info.extras[GroupId::NormalizedMetadata][KeyId::SubModel] = std::string("1-Inch 360 Edition");
                    }
                    else if (offset && offset->size() == 16 && offset->front() == 2) {
                        info.extras[GroupId::NormalizedMetadata][KeyId::SubModel] = std::string("360 Lens");
                    }
                    // else: 4K Boost Lens, 1-Inch Wide Angle Lens
                }

                has_metadata = true;
                is_raw_gyro = metadata.has_is_raw_gyro()? metadata.is_raw_gyro() : false; // Older models may lack this field, default to false.
                t_scale = is_raw_gyro? 1e-6 : 1e-3;
                if (metadata.has_first_frame_timestamp()) first_frame_t = metadata.first_frame_timestamp() * t_scale;
                else first_frame_t = 0.0;
            }

            if (metadata_only) break;
        }
        else if (id == RecordType::Gyro && format == RecordFormat::Binary) {
            // ranges are only relevant for raw format, where int16_min/max corresponds to -range/+range.
            constexpr auto deg2rad = 3.14159265358979323846 / 180.0;
            const auto p_gyro_range = info.extras.get<types::UInt>(GroupId::Metadata, "gyro_range_info.gyro_range");
            const auto gyro_scale = (p_gyro_range? *p_gyro_range : 2000) / 32768.0 * deg2rad; // +-2000 degree per second
            const auto p_acc_range = info.extras.get<types::UInt>(GroupId::Metadata, "gyro_range_info.acc_range");
            const auto acc_scale = (p_acc_range? *p_acc_range : 16) / 32768.0;                // +-16g

            const auto gyro_ts = [&info] {  // some sort of time offset...
                const auto p_relevant = info.extras.get<bool>(GroupId::Metadata, "if_gyro_timestamp");
                const auto p_val = info.extras.get<double>(GroupId::Metadata, "gyro_timestamp");
                if (!(p_relevant && p_val)) return 0.0;
                if (!*p_relevant) return 0.0;
                return *p_val * 1e-3;
            }();

            types::GyroVec gyro_data;
            types::AccVec  acc_data;

            const std::uint32_t sample_size = is_raw_gyro ? 8+6*2 : 8+6*8;
            const auto sample_count = size / sample_size;
            for (auto i = 0u; i < sample_count; ++i) {
                std::uint64_t timestamp;
                file.read_num<Endian::LE>(timestamp);
                const auto td = timestamp * t_scale - first_frame_t - gyro_ts;
                if (is_raw_gyro) { // 8+6*2
                    std::uint16_t ax, ay, az,
                             gx, gy, gz;
                    file.read_nums<Endian::LE>(ax, ay, az, gx, gy, gz);
                    const auto axd = acc_scale*(ax - 32768.0), ayd = acc_scale*(ay - 32768.0), azd = acc_scale*(az - 32768.0);
                    const auto gxd = gyro_scale*(gx - 32768.0), gyd = gyro_scale*(gy - 32768.0), gzd = gyro_scale*(gz - 32768.0);
                    acc_data.emplace_back(td, axd, ayd, azd);
                    gyro_data.emplace_back(td, gxd, gyd, gzd);
                }
                else { // !is_raw_gyro, 8+6*8
                    double ax, ay, az,
                           gx, gy, gz;
                    file.read_nums<Endian::LE>(ax, ay, az, gx, gy, gz);
                    acc_data.emplace_back(td, ax, ay, az);
                    gyro_data.emplace_back(td, gx, gy, gz);
                }
            }

            info.extras[GroupId::SensorData][KeyId::GyroData] = std::move(gyro_data);
            info.extras[GroupId::SensorData][KeyId::AccData]  = std::move(acc_data);
        }
        else if ((id == RecordType::Exposure || id == RecordType::ExposureSecondary) && format == RecordFormat::Binary) {
            types::ExposureVec exposure_data;
            const auto sample_count = size / (8+8);
            for (auto i = 0u; i < sample_count; ++i) {
                std::uint64_t timestamp;
                double   shutter;
                file.read_nums<Endian::LE>(timestamp, shutter);
                const auto td = timestamp * t_scale - first_frame_t;
                exposure_data.emplace_back(td, shutter);
            }

            info.extras[GroupId::SensorData][KeyId::ExposureData] = std::move(exposure_data);
        }
        else if ((id == RecordType::TimelapseTimestamp ) && format == RecordFormat::Binary) {
            const auto count = size / 8; // u64 timestamp
            types::VecD ts;
            for (auto i = 0u; i < count; ++i) {
                std::uint64_t timestamp;
                file.read_num<Endian::LE>(timestamp);
                ts.push_back(timestamp * t_scale - first_frame_t);
            }
            info.extras[GroupId::SensorData][KeyId::TimelapseTimestamp] = std::move(ts);
        }
        else if ((id == RecordType::Gps ) && format == RecordFormat::Binary) {
            const auto count  = size / 53; // 53 bytes
            types::GPSDataVec gps_data;
            for (auto i = 0u; i < count; ++i) {
                std::uint64_t t1; std::uint16_t t2;
                std::uint8_t fix;
                double lat, lon;
                std::uint8_t lat_direction, lon_direction;
                double speed;    // m/s
                double track;    // speed direction
                double altitude; // Geoid undulation
                file.read_nums<Endian::LE>(t1, t2, fix, lat, lat_direction, lon, lon_direction, speed, track, altitude);

                if (lat_direction == 'S') lat = std::abs(lat) * -1;
                if (lon_direction == 'W') lon = std::abs(lon) * -1;
                const double timestamp = double(t1) + double(t2) * 1e-3;

                gps_data.emplace_back(fix=='A', timestamp, lat, lon, altitude, speed, track);
            }

            info.extras[GroupId::SensorData][KeyId::GpsData] = std::move(gps_data);
        }
    }

    // Post-process some data here...

    return true;
} catch (const StreamError&) {
    return false;
}

} /* namespace caminfo */
