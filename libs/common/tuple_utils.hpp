#ifndef TUPLE_UTILS_HPP_CBE4287E_C976_4BA5_B5CA_37C0BB287E17
#define TUPLE_UTILS_HPP_CBE4287E_C976_4BA5_B5CA_37C0BB287E17

#include <utility>
#include <tuple>

namespace util
{

template<typename T, auto...>
using always_T_with_values = T;

namespace details {

template<bool Decay, std::size_t ...Is, class T>
constexpr auto range_tuple_impl(T&& range, std::size_t begin, std::index_sequence<Is...>)
{
    if constexpr (Decay) {
        return std::make_tuple(std::forward<T>(range)[begin+Is]...);
    }
    else {
        return std::tie(std::forward<T>(range)[begin+Is]...);
    }
}

template<std::size_t ...Is, class IterT>
constexpr auto iter_to_tuple_impl(IterT begin, std::index_sequence<Is...>)
{
    return std::tuple<always_T_with_values<decltype(*begin), Is>...>{ ((void)Is, *(begin++))... }; // guarantee evaluation order
}

// We allow tie'ing to elements of a temporary range, since a temporary range might refer to some outer non-temp values (e.g. std::span).
// So be careful when passing things like rvalue std::vector to range_as_tuple(). They have to be used before the end of the full expression.

} // namespace details

template<std::size_t N, class T>
constexpr auto range_as_tuple(T&& range, std::size_t begin = 0)
{
    return details::range_tuple_impl<false>(std::forward<T>(range), begin, std::make_index_sequence<N>{});
}

template<std::size_t N, class T>
constexpr auto range_to_tuple(T&& range, std::size_t begin = 0)
{
    return details::range_tuple_impl<true>(std::forward<T>(range), begin, std::make_index_sequence<N>{});
}

template<std::size_t N, class IterT>
constexpr auto iter_to_tuple(IterT begin)
{
    return details::iter_to_tuple_impl(begin, std::make_index_sequence<N>{});
}

} // namespace util


#endif /* TUPLE_UTILS_HPP_CBE4287E_C976_4BA5_B5CA_37C0BB287E17 */
