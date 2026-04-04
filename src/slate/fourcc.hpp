#ifndef FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D
#define FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D

#include <string>
#include <cstdint>
#include <cstddef>

namespace slate {

template <std::size_t N>
constexpr std::uint32_t
fourcc(const char(&s)[N]) noexcept
{
    static_assert(N >= 4, "FourCC must contain 4 characters");

    return std::uint32_t(s[0]) << 24 | std::uint32_t(s[1]) << 16 | std::uint32_t(s[2]) << 8 | std::uint32_t(s[3]);
}


inline std::string
fourcc_str(std::uint32_t u) noexcept {
    return {
        char(u >> 24 & 0xffu),  // implementation-defined, if out of range for char. We don't care about this.
        char(u >> 16 & 0xffu),
        char(u >> 8  & 0xffu),
        char(u       & 0xffu)
    };
}

inline namespace fourcc_literal {

namespace detail {

// Owning wrapper for converting a string literal
template<std::size_t N>
struct StringLiteral {
    static_assert(N > 0);
    static constexpr auto size = N - 1;
    char str[N];
    constexpr StringLiteral(const char(&s)[N]) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            str[i] = s[i];
        }
    }
};

} // namespace detail

template<detail::StringLiteral Str>
constexpr std::uint32_t operator""_fc() noexcept {
    static_assert(Str.size == 4, "FourCC must contain exactly 4 characters");
    return static_cast<std::uint32_t>(Str.str[0]) << 24 |
           static_cast<std::uint32_t>(Str.str[1]) << 16 |
           static_cast<std::uint32_t>(Str.str[2]) << 8 |
           static_cast<std::uint32_t>(Str.str[3]);
}

} // namespace fourcc_literal

} // namespace slate


#endif /* FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D */
