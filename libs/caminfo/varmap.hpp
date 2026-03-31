#ifndef VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2
#define VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2

#include "value_types.hpp"
#include <map>
#include <stdexcept>     // std::out_of_range
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#ifdef CAMINFO_ENABLE_ANY
#include <any>
#endif

namespace caminfo {

enum class GroupId {
    NormalizedMetadata, // indexed by KeyId
    Metadata,
    SensorData,
    ProcessedData,
    VideoInfo,
    Other
};

enum class KeyId {
    CameraModel,
    SubModel,
    SerialNumber,
    LensType,
    FirmwareVersion,
    StabilizationMode,
    HasStabilization,
    CameraRotation,
    LensParams,

    Width,
    Height,
    FPS,
    Duration,
    IsCFR,
    FrameCount,
    DisplayRotation,
    VideoTrackIds,

    GyroData,
    AccData,
    ExposureData,
    CameraQuaternionData,
    TimedCameraQuaternionData,
    TimelapseTimestamp,
    GpsData,
};

std::string_view get_string(GroupId id) noexcept;
std::string_view get_string(KeyId id) noexcept;

namespace details {

template <typename T, typename...>
struct append_unique_types { // all non-unique types will eventually inherit from this
    using type = T;
};
template <template<typename...> typename TMPL, typename ...Ts, typename U, typename ...Us>
struct append_unique_types<TMPL<Ts...>, U, Us...> : std::conditional_t<(std::disjunction_v<std::is_same<U, Ts>...>),
                                                                append_unique_types<TMPL<Ts...>, Us...>,
                                                                append_unique_types<TMPL<Ts..., U>, Us...>>
{};

template <typename ...Ts>
using unique_variant_t = typename append_unique_types<std::variant<std::monostate>, Ts...>::type;

} /* details ns */

using VarType = details::unique_variant_t<types::String,
                                          types::Bool,
                                          types::Int,
                                          types::UInt,
                                          types::Double,
                                          types::VecD,
                                          types::VecI,
                                          types::RawBytes,

                                          types::GyroVec,
                                          types::AccVec,
                                          types::ExposureVec,
                                          types::QuaternionVec,
                                          types::TimedQuaternionVec,
                                          types::GPSDataVec,

                                          //types::Tuple2d,
                                          //types::Tuple3d,
                                          //types::Tuple4d,
                                          //types::Tuple5d,
                                          std::vector<types::Tuple2d>,
                                          std::vector<types::Tuple3d>,
                                          std::vector<types::Tuple4d>,
                                          std::vector<types::Tuple5d>,

                                          std::vector<types::Array2d>,
                                          std::vector<types::Array3d>,
                                          std::vector<types::Array4d>,
                                          std::vector<types::Array5d>
#ifdef CAMINFO_ENABLE_ANY
                                          ,
                                          std::any
#endif
                                          >;

using VarMap = std::map<std::variant<KeyId, std::string>, VarType>; // key can be either pre-defined Ids or arbitary string.

struct GroupedVarMap : std::map<GroupId, VarMap> {

    //TODO: Allowing indexing with string view.

    template<typename T>
    const T* get(GroupId group, const VarMap::key_type& key) const noexcept { // auto instantiation in cpp?
        const auto it = find(group);
        if (it == cend())
            return nullptr;
        const auto it2 = it->second.find(key);
        if (it2 == it->second.cend())
            return nullptr;
        return std::get_if<T>(&it2->second);
    }

    template<typename T>
    const auto& get_ex(GroupId group, const VarMap::key_type& key) const {
        try {
            return std::get<T>(at(group).at(key));
        } catch (const std::bad_variant_access&) {
            throw std::out_of_range("bad value type");
        }
    }

    template<typename T>
    const T& get_or(GroupId group, const VarMap::key_type& key, const T& default_val) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return default_val;
        return *p;
    }

    template<typename T, typename = std::enable_if_t<!std::is_reference_v<T>>> // Put a constraint, otherwise this overload is always preferred.
    auto get_or(GroupId group, const VarMap::key_type& key, T&& default_val = T{}) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return std::move(default_val);
        return *p;
    }

};

std::string to_string(const VarType& var, std::size_t max_vec_len = 50) noexcept;
std::string to_string(const VarMap::key_type& key) noexcept;

} // namespace caminfo

#endif /* VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2 */
