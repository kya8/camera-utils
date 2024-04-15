#ifndef FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D
#define FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D

#include <string>
#include <cstdint>
#include <cstddef>

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


#endif /* FOURCC_HPP_B41F7CAE_9564_47EB_B9E3_0A8F343DE21D */
