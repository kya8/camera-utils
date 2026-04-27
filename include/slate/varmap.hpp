#ifndef VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2
#define VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2

#include "id.hpp"
#include "detail/variant.hpp"
#include "value_types.hpp"
#include <map>
#include <stdexcept>     // std::out_of_range
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#ifdef SLATE_ENABLE_ANY
#include <any>
#endif
#include <utility>
#include <compare>
#include <ostream>
#include <memory> // addressof
#include "slate/export.h"

namespace slate {

// A recursive variant type that can hold any of the supported types,
// including nested Map and Array that contain the variant itself.
struct Value;

// The key type for the map, can be either a well-known KeyId or an arbitrary string.
using Key = std::variant<KeyId, std::string>;
using Map = std::map<Key, Value, std::less<void>>; // std::less<void> for transparent comparison.
using Array = std::vector<Value>;

class VarMap;

// The actual variant type.
// The Array and VarMap types are recursive, to support arbitarily nested heterogeneous data structures.
// Tuples (static arrays) and vectors are for large homogeneous data, to allow for more efficient storage and processing.
using Variant = detail::unique_variant_t<types::String, types::Bool, types::Int, types::UInt, types::Double,
                                         types::VecDouble, types::VecFloat, types::VecI32, types::VecI64, types::VecBytes,
                                         types::GyroVec, types::AccVec, types::ExposureVec, types::QuaternionVec, types::TimedQuaternionVec, types::GPSDataVec,
                                         std::vector<types::Tuple2d>, std::vector<types::Tuple3d>, std::vector<types::Tuple4d>, std::vector<types::Tuple5d>,
                                         std::vector<types::Array2d>, std::vector<types::Array3d>, std::vector<types::Array4d>, std::vector<types::Array5d>,
#ifdef SLATE_ENABLE_ANY
                                         std::any,
#endif
                                         Array, VarMap>;

// Requirement for looking up VarMap.
template<typename K>
concept KeyType = std::is_convertible_v<K, Key> || std::is_convertible_v<K, std::string_view>;

// Type requirement for querying a value. It must be one of the types in Variant.
template<typename T>
concept ValueType = detail::is_one_of<T, Variant>;

class VarMap : public Map {
public:
    [[nodiscard]] const Map& as_map() const & noexcept { return *this; }
    [[nodiscard]] Map& as_map() & noexcept { return *this; }
    [[nodiscard]] const Map&& as_map() const && noexcept { return std::move(*this); }
    [[nodiscard]] Map&& as_map() && noexcept { return std::move(*this); }

    template<typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto get_value(this Self& map, const K& key, const Ks& ...keys) noexcept -> std::conditional_t<std::is_const_v<Self>, const Value*, Value*> {
        const auto it = [&] {
            if constexpr (requires{ map.find(key); }) {
                return map.find(key);
            } else { // Fallback to lookup with exact key_type
                return map.find(Key(key));
            }
        }();
        if (it == map.end())
            return nullptr;
        if constexpr (sizeof...(Ks) == 0) {
            return std::addressof(it->second);
        } else {
            const auto p = std::get_if<VarMap>(&it->second);
            if (!p)
                return nullptr;
            return p->get_value(keys...);
        }
    }

    template<typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto&& get_value_ex(this Self&& map, const K& key, const Ks& ...keys) {
        const auto it = [&] {
            if constexpr (requires{ map.find(key); }) {
                return map.find(key);
            } else { // Fallback to lookup with exact key_type
                return map.find(Key(key));
            }
        }();
        if (it == map.end())
            throw std::out_of_range("Key not found");
        if constexpr (sizeof...(Ks) == 0) {
            return std::forward_like<Self>(it->second);
        } else {
            const auto p = std::get_if<VarMap>(&it->second);
            if (!p)
                throw std::out_of_range("Key does not refer to a map");
            return std::forward_like<Self>(*p).get_value_ex(keys...);
        }
    }

    template<ValueType T, typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto get(this Self& self, const K& key, const Ks& ...keys) noexcept -> std::conditional_t<std::is_const_v<Self>, const T*, T*> {
        const auto p_val = self.get_value(key, keys...);
        if (!p_val)
            return nullptr;
        return std::get_if<T>(p_val);
    }

    template<ValueType T, typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto&& get_ex(this Self&& self, const K& key, const Ks& ...keys) {
        auto& val = self.get_value_ex(key, keys...);
        const auto p = std::get_if<T>(&val);
        if (!p)
            throw std::out_of_range("Value type mismatch");
        return std::forward_like<Self>(*p);
    }
};

struct Value: Variant {
    using Variant::Variant;   // inherit constructors
    using Variant::operator=; // inherit assignment operators

    [[nodiscard]] const Variant& as_variant() const & noexcept { return *this; }
    [[nodiscard]] Variant& as_variant() & noexcept { return *this; }
    [[nodiscard]] const Variant&& as_variant() const && noexcept { return std::move(*this); }
    [[nodiscard]] Variant&& as_variant() && noexcept { return std::move(*this); }
};

// Allows comparing variant keys with string_view directly.
// They can be found by ADL, since Key's template parameter KeyId is in our namespace.
inline constexpr std::strong_ordering operator<=>(const Key& key, std::string_view sv) noexcept {
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

// An outer map that gives a group id for each VarMap, to allow better organization of the metadata.
class GroupedVarMap : public std::map<GroupId, VarMap> {
public:
    /**
     * Get a pointer to the value requested by group id and Key.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`.
     * @param group Group id.
     * @param key, keys Key to look up. If more than 1 key is provided, recurses into nested maps.
     * @return A pointer to the requested data if found and of the correct type, or nullptr otherwise.
     */
    template<ValueType T, typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto get(this Self& self, GroupId group, const K& key, const Ks& ...keys) noexcept -> std::conditional_t<std::is_const_v<Self>, const T*, T*>{
        const auto it = self.find(group);
        if (it == self.end())
            return nullptr;
        return it->second.template get<T>(key, keys...);
    }

    /**
     * Get a reference to the value requested by group id and Key.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`.
     * @param group Group id.
     * @param key, keys Key to look up. If more than 1 key is provided, recurses into nested maps.
     * @return Reference to the requested value.
     * @exception std::out_of_range if the group or key is not found, or the value type does not match.
     */
    template<ValueType T, typename Self, KeyType K, KeyType ...Ks>
    [[nodiscard]] auto&& get_ex(this Self&& self, GroupId group, const K& key, const Ks& ...keys) {
        // Unfortunately, std::map::at doesn't support heterogeneous lookup before C++26, so we have to check manually with find.
        const auto it = self.find(group);
        if (it == self.end())
            throw std::out_of_range("GroupId not found");
        return std::forward_like<Self>(it->second).template get_ex<T>(key, keys...);
    }

    /**
     * Get a reference to the value requested by group id and Key, or return a default value if not found or type mismatch.
     * @tparam T  The expected type of the value. Must be one of the alternative types in `Value`.
     * @param group Group id.
     * @param key, keys Key to look up. If more than 1 key is provided, recurses into nested maps.
     * @param default_val The default value to return if the requested value is not found or has a different type.
     * @return Reference to the requested value or the default value.
     */
    template<ValueType T, KeyType K, KeyType ...Ks>
    [[nodiscard]] const T& get_or(const T& default_val, GroupId group, const K& key, const Ks& ...keys) const noexcept {
        const auto p = get<T>(group, key, keys...);
        if (!p) return default_val;
        return *p;
    }

    /**
     * Overload for temporary default value.
     * Returns by value, so be careful with copying large data from the map.
     */
    template<ValueType T, KeyType K, KeyType ...Ks>
    requires (!std::is_reference_v<T>) // T&& must be rvalue ref, in case T is deduced
    [[nodiscard]] auto get_or(T&& default_val, GroupId group, const K& key, const Ks& ...keys) const noexcept {
        const auto p = get<T>(group, key, keys...);
        if (!p) return std::move(default_val);
        return *p;
    }

    template<typename Self>
    [[nodiscard]] auto get(this Self& self, GroupId group) noexcept -> std::conditional_t<std::is_const_v<Self>, const VarMap*, VarMap*> {
        const auto it = self.find(group);
        if (it == self.end()) {
            return nullptr;
        }
        return std::addressof(it->second);
    }

    template<typename Self>
    [[nodiscard]] auto&& get_ex(this Self&& self, GroupId group) {
        const auto it = self.find(group);
        if (it == self.end()) {
            throw std::out_of_range("GroupId not found");
        }
        return std::forward_like<Self>(it->second);
    }
};

/**
 * Converts a Value to a string representation.
 * @param[in] var The value to convert.
 * @param[in] max_vec_len The maximum number of scalars to include in the string representation for vectors.
 *                        0 means no limit.
 */
SLATE_EXPORT [[nodiscard]] std::string to_string(const Value& var, std::size_t max_vec_len = 50) noexcept;
/**
 * Converts a Key to a string representation.
 * The returned string_view references a static string if the key is a KeyId.
 * If the key is a string, the returned string_view references the string stored in the key.
 */
SLATE_EXPORT [[nodiscard]] std::string_view to_string(const Key& key) noexcept;

SLATE_EXPORT std::ostream& operator<<(std::ostream& os, const VarMap& map);
SLATE_EXPORT std::ostream& operator<<(std::ostream& os, const Value& val);

// TODO: Output Value/VarMap to std::FILE*

} // namespace slate

#endif /* VARMAP_HPP_E5E2828D_2421_4F81_B079_A3D97AF2F7C2 */
