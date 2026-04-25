#ifndef VARIANT_HPP_C3629885_4A17_4B79_8B27_89249A6D97B6
#define VARIANT_HPP_C3629885_4A17_4B79_8B27_89249A6D97B6

#include <variant>
#include <type_traits>
#include <utility>
#include <cassert>

namespace slate::detail {

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

// Make a std::variant type, removing duplicate types.
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

// Unsafe getter for std::variant.
template<typename T, typename ...Ts>
requires (std::is_same_v<T, Ts> || ...)
[[nodiscard]] const T& get_unsafe(const std::variant<Ts...>& var) {
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
[[nodiscard]] T& get_unsafe(std::variant<Ts...>& var) {
    return const_cast<T&>(get_unsafe<T>(std::as_const(var)));
}

} // namespace slate::detail

#endif /* VARIANT_HPP_C3629885_4A17_4B79_8B27_89249A6D97B6 */
