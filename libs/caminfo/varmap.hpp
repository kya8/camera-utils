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
#include <compare>
#include <ostream>
#include <cassert>

namespace caminfo {

// Well-known group id.
enum class GroupId {
    NormalizedMetadata, // Holds well-known metadata represented by KeyId.
    Metadata,
    SensorData,
    ProcessedData,
    VideoInfo,
    Other
};

// Well-know key tag for commonly used types of metadata.
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

/**
 * Get a informative string of the given GroupId.
 */
std::string_view to_string(GroupId id) noexcept;
/**
 * Get a informative string of the given KeyId.
 */
std::string_view to_string(KeyId id) noexcept;

namespace detail {

// Append types while skipping duplicates.
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

// Helper to get the index of a type in a parameter pack.
template<typename T, typename T0, typename ...Ts>
requires std::is_same_v<T, T0> || (std::is_same_v<T, Ts>||...)
constexpr int get_index() {
    if constexpr (std::is_same_v<T, T0>) {
        return 0;
    } else {
        return 1 + get_index<T, Ts...>();
    }
}

} /* namespace detail */

// A recursive variant type that can hold any of the supported types,
// including nested Map and Array that contain the variant itself.
struct Value;

// The key type for the map, can be either a well-known KeyId or an arbitrary string.
using Key = std::variant<KeyId, std::string>;
using VarMap = std::map<Key, Value, std::less<void>>; // std::less<void> for transparent comparison.
using Array = std::vector<Value>;

// The actual variant type.
// The Array and VarMap types are recursive, to support arbitarily nested heterogeneous data structures.
// Tuples (static arrays) and vectors are for large homogeneous data, to allow for more efficient storage and processing.
using Variant = detail::unique_variant_t<types::String,
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

// constexpr std::strong_ordering operator<=>(const Key& key, KeyId id) noexcept {
//     return std::visit([&]<typename T>(const T& val) {
//         if constexpr (std::is_same_v<T, KeyId>) {
//             return val <=> id;
//         } else {
//             return 1 <=> 0;
//         }
//     }, key);
// }

// A generic comparison operator for std::variant that allows comparing
// the contained value directly with a type T, if it is one of the alternatives.
// Note that this does not allow implicit conversions from T.
template<typename ...Ts, typename T>
requires (std::is_same_v<T, Ts> || ...)
constexpr auto operator<=>(const std::variant<Ts...>& var, const T& rhs)
{
    return std::visit(
        [&]<typename V>(const V& val) ->
        std::common_comparison_category_t<std::compare_three_way_result_t<std::size_t>, std::compare_three_way_result_t<T>> {
            if constexpr (std::is_same_v<V, T>) {
                return val <=> rhs;
            } else {
                return detail::get_index<V, Ts...>() <=> detail::get_index<T, Ts...>();
            }
        }
    , var);
}

// Requirement for looking up VarMap.
template<typename K>
concept KeyType = std::is_convertible_v<K, Key> || std::is_convertible_v<K, std::string_view>;

// An outer map that gives a group id for each VarMap, to allow better organization of the metadata.
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
    /**
     * Get a pointer to the value requested by group id and Key.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`. 
     * @param group Group id.
     * @param key   Key to look up.
     * @return A pointer to the requested data if found and of the correct type, or nullptr otherwise.
     */
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

    /**
     * Get a reference to the value requested by group id and Key.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`. 
     * @param group Group id.
     * @param key   Key to look up.
     * @return Reference to the requested value.
     * @exception std::out_of_range if the group or key is not found, or the value type does not match.
     */
    template<typename T, KeyType K>
    const auto& get_ex(GroupId group, const K& key) const {
        if constexpr (requires(VarMap m){ m.find(key); }) {
            return get_ex_impl<T>(group, key);
        } else { // Fallback to lookup with exact key_type
            return get_ex_impl<T>(group, Key(key));
        }
    }

    /**
     * Get a reference to the value requested by group id and Key, or return a default value if not found or type mismatch.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`.
     * @param group Group id.
     * @param key   Key to look up.
     * @param default_val The default value to return if the requested value is not found or has a different type.
     * @return Reference to the requested value or the default value.
     */
    template<typename T, KeyType K>
    const T& get_or(GroupId group, const K& key, const T& default_val) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return default_val;
        return *p;
    }

    /**
     * Overload for temporary default value.
     * Returns by value, so be careful with copying large data from the map.
     */
    template<typename T, KeyType K>
    requires (!std::is_reference_v<T>) // T&& must be rvalue ref
    auto get_or(GroupId group, const K& key, T&& default_val = T{}) const noexcept {
        const auto p = get<T>(group, key);
        if (!p) return std::move(default_val);
        return *p;
    }

};

/**
 * Converts a Value to a string representation.
 * @param[in] var The value to convert.
 * @param[in] max_vec_len The maximum number of scalars to include in the string representation for vectors.
 *                        0 means no limit.
 */
std::string to_string(const Value& var, std::size_t max_vec_len = 50) noexcept;
/**
 * Converts a Key to a string representation.
 * The returned string_view references a static string if the key is a KeyId.
 * If the key is a string, the returned string_view references the string stored in the key.
 */
std::string_view to_string(const Key& key) noexcept;

std::ostream& operator<<(std::ostream& os, const VarMap& map);
std::ostream& operator<<(std::ostream& os, const Value& val);

// TODO: Output Value/VarMap to std::FILE*

// Unsafe getter for std::variant.
template<typename T, typename ...Ts>
requires (std::is_same_v<T, Ts> || ...)
const T& get_unsafe(const std::variant<Ts...>& var) {
    if (const auto p = std::get_if<T>(&var)) {
        return *p;
    } else {
        assert(("Type mismatch in get_unsafe()", false));
        std::unreachable();
    }
}

// Unsafe getter for std::variant.
template<typename T, typename ...Ts>
requires (std::is_same_v<T, Ts> || ...)
T& get_unsafe(std::variant<Ts...>& var) {
    return const_cast<T&>(get_unsafe<T>(std::as_const(var)));
}

} // namespace caminfo

#endif /* VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2 */
