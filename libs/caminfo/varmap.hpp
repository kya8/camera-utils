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
#include <utility>

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

std::string_view to_string(GroupId id) noexcept;
std::string_view to_string(KeyId id) noexcept;

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

// A recursive variant type that can hold any of the supported types,
// including nested Maps and Arrays that contain the variant itself.
struct Value;

using Key = std::variant<KeyId, std::string>;

// Allows comparing variant keys with string_view directly.
// They can be found by ADL, since Key's template parameter KeyId is in our namespace.
constexpr std::strong_ordering operator<=>(const Key& key, std::string_view sv) noexcept {
    return std::visit([&sv]<typename T>(const T& val) {
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            return std::string_view(val) <=> sv; // Convert val to string_view to avoid recursion into this function.
        } else {
            return 0 <=> 1; // following std::variant's operator<=>
        }
    }, key);
}

constexpr std::strong_ordering operator<=>(const Key& key, KeyId id) noexcept {
    return std::visit([&]<typename T>(const T& val) {
        if constexpr (std::is_same_v<T, KeyId>) {
            return val <=> id;
        } else {
            return 1 <=> 0;
        }
    }, key);
}

using VarMap = std::map<Key, Value, std::less<void>>; // The key can be either a well-known tag (KeyId) or an arbitrary string.
                                                      // std::less<void> for transparent comparison.
using Array = std::vector<Value>;

// The Array and VarMap types are recursive, to support arbitarily nested heterogeneous data structures.
// Tuples (static arrays) and vectors are for large homogeneous data, to allow for more efficient storage and processing.
using Variant = details::unique_variant_t<types::String,
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
                                          ,
                                          Array,
                                          VarMap
                                          >;

struct Value: Variant {
    using Variant::Variant;   // inherit constructors
    using Variant::operator=; // inherit assignment operators

    const Variant& as_variant() const & noexcept { return *this; }
    Variant& as_variant() & noexcept { return *this; }
    const Variant&& as_variant() const && noexcept { return std::move(*this); }
    Variant&& as_variant() && noexcept { return std::move(*this); }
};

template<typename K>
concept KeyType = std::is_convertible_v<K, Key> || std::is_convertible_v<K, std::string_view>;

struct GroupedVarMap : std::map<GroupId, VarMap> {

private:
    template<typename T, typename Self, typename K>
    auto get_impl(this Self& self, GroupId group, const K& key) noexcept -> std::conditional_t<std::is_const_v<Self>, const T*, T*>{
        const auto it = self.find(group);
        if (it == self.end())
            return nullptr;
        const auto it2 = it->second.find(key);
        if (it2 == it->second.end())
            return nullptr;
        return std::get_if<T>(&it2->second);
    }

    template<typename T, typename Self, typename K>
    auto& get_ex_impl(this Self& self, GroupId group, const K& key) {
        // Unfortunately, std::map::at doesn't support heterogeneous lookup before C++26, so we have to check manually with find.
        const auto it = self.find(group);
        if (it == self.end())
            throw std::out_of_range("GroupId not found");
        const auto it2 = it->second.find(key);
        if (it2 == it->second.end())
            throw std::out_of_range("Key not found");
        const auto p = std::get_if<T>(&it2->second);
        if (!p)
            throw std::out_of_range("Value type mismatch");
        return *p;
    }

public:
    template<typename T, KeyType K>
    const T* get(GroupId group, const K& key) const noexcept {
        // if constexpr (std::is_convertible_v<K, std::string_view>) {
        //     return get_impl<T>(group, (const std::string_view&)(key)); // cast to reference, to avoid copying
        if constexpr (requires(VarMap m){ m.find(key); }) {
            return get_impl<T>(group, key);
        } else { // Fallback to lookup with exact key_type
            return get_impl<T>(group, Key(key));
        }
    }

    template<typename T, KeyType K>
    const auto& get_ex(GroupId group, const K& key) const {
        if constexpr (requires(VarMap m){ m.find(key); }) {
            return get_ex_impl<T>(group, key);
        } else { // Fallback to lookup with exact key_type
            return get_ex_impl<T>(group, Key(key));
        }
    }

    template<typename T, KeyType K>
    const T& get_or(GroupId group, const K& key, const T& default_val) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return default_val;
        return *p;
    }

    template<typename T, KeyType K>
    requires (!std::is_reference_v<T>) // T&& must be rvalue ref
    auto get_or(GroupId group, const K& key, T&& default_val = T{}) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return std::move(default_val);
        return *p;
    }

};

std::string to_string(const Value& var, std::size_t max_vec_len = 50) noexcept;
std::string_view to_string(const Key& key) noexcept;

template<typename T, typename ...Ts>
const T& get_unsafe(const std::variant<Ts...>& var) {
    if (const auto p = std::get_if<T>(&var)) {
        return *p;
    } else {
        std::unreachable();
    }
}

template<typename T, typename ...Ts>
T& get_unsafe(std::variant<Ts...>& var) {
    return const_cast<T&>(get_unsafe<T>(std::as_const(var)));
}

} // namespace caminfo

#endif /* VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2 */
