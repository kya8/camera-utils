#ifndef STRING_UTILS_HPP_FE77D7B6_3F11_455E_9090_237AAD53F00E
#define STRING_UTILS_HPP_FE77D7B6_3F11_455E_9090_237AAD53F00E

#include <type_traits>
#include <string_view>

template<typename ...Ts, std::enable_if_t<(std::is_convertible_v<Ts, std::string_view> && ...) && (sizeof...(Ts) > 0), int> = 0>
bool match(std::string_view arg, const Ts& ...Args) noexcept
{
    return ((arg == Args) || ...);
}

#endif /* STRING_UTILS_HPP_FE77D7B6_3F11_455E_9090_237AAD53F00E */
