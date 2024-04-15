#ifndef HELPER_TEMPLATES_HPP_ED959A9F_B22F_4605_9E37_2517EDB61856
#define HELPER_TEMPLATES_HPP_ED959A9F_B22F_4605_9E37_2517EDB61856

#include <type_traits>


template<typename T, typename ...Ts>
constexpr bool eq_one(const T& x, const Ts&...args)
{
    return ((x == args)||...);
}

template<typename T, typename ...Ts>
constexpr bool eq_all(const T& x, const Ts&...args)
{
    return ((x == args)&&...);
}

template<typename T, typename ...Ts>
constexpr bool is_one_of_v = std::disjunction_v<std::is_same<T, Ts>...>;

template<typename T, typename ...Ts>
constexpr bool is_all_same_v = std::conjunction_v<std::is_same<T, Ts>...>;

template<typename ...>
constexpr bool always_false_v = false;


template <typename T, template <typename...> typename Template>
struct is_instantiation_of : std::false_type {};
template <template <typename...> typename Template, typename... Args>
struct is_instantiation_of<Template<Args...>, Template> : std::true_type {};

template<typename>
struct is_template_instantiation : std::false_type {};

template<template<typename...> typename TMPL, typename ...Ps>
struct is_template_instantiation<TMPL<Ps...>> : std::true_type {};

namespace details {

template<typename S, typename T, typename = void>
struct is_streamable_impl : std::false_type {};

template<typename S, typename T>
struct is_streamable_impl <S, T,
                           std::void_t<decltype( std::declval<S&>() << std::declval<T>() )>>
                          : std::true_type {};

}

// For some reason, this doesn't work for some stl templates, that have user-defined operator<< AFTER including this header.
template<typename S, typename T>
constexpr bool is_streamable_v = details::is_streamable_impl<S, T>::value;


template <typename T, typename...>
struct append_unique_types { // all non-unique types will eventually inherit from this
    using type = T;
};
template <template<typename...> typename TMPL, typename ...Ts, typename U, typename ...Us>
struct append_unique_types<TMPL<Ts...>, U, Us...> : std::conditional_t<(std::disjunction_v<std::is_same<U, Ts>...>),
                                                                append_unique_types<TMPL<Ts...>, Us...>,
                                                                append_unique_types<TMPL<Ts..., U>, Us...>>
{};

#endif /* HELPER_TEMPLATES_HPP_ED959A9F_B22F_4605_9E37_2517EDB61856 */
