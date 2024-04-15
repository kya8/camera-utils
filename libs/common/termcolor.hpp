#ifndef TERMCOLOR_HPP_A73B5B43_1C3B_42A8_B5BC_9AED37F243C5
#define TERMCOLOR_HPP_A73B5B43_1C3B_42A8_B5BC_9AED37F243C5

#include <array>
#include <string_view>
#include <ostream>
#if __cplusplus >= 202002L && __has_include(<format>)
#include <format>
#endif

// Output wrapper for terminal color / display mode.
// Escape sequences are generated at compile time.

// TODO:
// Runtime styles, with max-sized static char array.
// Wrap multiple values (with tuple).
// Manual code sequence output. (with condition)

namespace tc {

// Simple static-sized string for compile-time string ops.
template<int N>
struct FixedString : std::array<char, N> {
    using Array = std::array<char, N>;
    constexpr const Array& array() const noexcept { return *this; }
    constexpr Array& array() noexcept { return *this; }

    constexpr std::string_view sv() const noexcept {
        return {Array::data(), Array::size()};
    }

    constexpr FixedString<N + 1> push(char c) const noexcept {
        FixedString<N + 1> ret{};
        for (int i = 0; i < N; ++i) {
            ret[i] = (*this)[i];
        }
        ret[N] = c;
        return ret;
    }

    // Otherwise we'll need double braces.
    template<class ...U> // (std::is_same_v<char, U> && ...) == true
    constexpr FixedString(char c, U ...args) noexcept : Array{c, args...} {}

    constexpr FixedString() noexcept = default;
};

template<class ...U>
FixedString(char c, U ...args) -> FixedString<sizeof...(U) + 1>;

template<int N>
constexpr auto mkfs(const char (&arr)[N]) noexcept
{
    FixedString<N - 1> ret{};
    for (int i = 0; i < N - 1; ++i) {
        ret[i] = arr[i];
    }
    return ret;
}

constexpr auto mkfs(char c) noexcept {
    return FixedString{c};
}

// Get the decimal digits for a non-negative number.
template<int Num>
constexpr auto digits() noexcept
{
    static_assert(Num >= 0);
    if constexpr (Num >= 10) {
        return digits<Num / 10>().push('0' + Num % 10);
    } else {
        return FixedString{'0' + Num};
    }
}

template<int A, int B>
constexpr FixedString<A + B> operator+(const FixedString<A>& lhs, const FixedString<B>& rhs) noexcept {
    FixedString<A + B> ret{};
    for (int i = 0; i < A; ++i) {
        ret[i] = lhs[i];
    }
    for (int i = 0; i < B; ++i) {
        ret[i + A] = rhs[i];
    }
    return ret;
}

template<int N, int... Ns>
constexpr auto join(char sep, const FixedString<N>& fs, const FixedString<Ns>&... args) noexcept
{
    if constexpr (sizeof...(Ns) == 0) {
        (void)sep;
        return fs;
    } else {
        return fs.push(sep) + join(sep, args...);
    }
}

template<int N>
std::ostream& operator<<(std::ostream& os, const FixedString<N>& fs)
{
    return os << fs.sv();
}


enum class AnsiColor {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
};

struct ColorCode {
    int fg;
    int bg;
};

constexpr ColorCode
get_color_code(AnsiColor color) noexcept
{
    switch (color) {
    case AnsiColor::Default: return {39, 49};
    case AnsiColor::Black:   return {30, 40};
    case AnsiColor::Red:     return {31, 41};
    case AnsiColor::Green:   return {32, 42};
    case AnsiColor::Yellow:  return {33, 43};
    case AnsiColor::Blue:    return {34, 44};
    case AnsiColor::Magenta: return {35, 45};
    case AnsiColor::Cyan:    return {36, 46};
    case AnsiColor::White:   return {37, 47};
    }
}

struct Reset {
    static constexpr auto code = digits<0>();
};

struct ColorFgBase {
    static constexpr auto reset = digits<39>();
};

struct ColorBgBase {
    static constexpr auto reset = digits<49>();
};

template<AnsiColor C>
struct ColorFg : ColorFgBase {
    static constexpr auto code = digits<get_color_code(C).fg>();
};

template<AnsiColor C>
struct ColorBg : ColorBgBase {
    static constexpr auto code = digits<get_color_code(C).bg>();
};

template<typename ...Impl>
constexpr auto seq = mkfs("\033[") + join(';', Impl::code...) + mkfs('m');

template<typename ...Impl>
constexpr auto reset = mkfs("\033[") + join(';', Impl::reset...) + mkfs('m');

template<>
constexpr auto reset<> = mkfs("\033[0m"); // Reset all

template<typename T, typename ...Impl>
struct OutputWrapper {
    const T& inner; // To prevent lifetime issues, we could move rvalues in.
    static constexpr const auto& seq = tc::seq<Impl...>; // Alternatively, apply the codes one by one. Some terminals have trouble with combined code.
    static constexpr const auto& reset = tc::reset<Impl...>;

    OutputWrapper(const T& inner) noexcept: inner(inner) {};
};

template<typename T, typename ...Impl>
std::ostream& operator<<(std::ostream& os, const OutputWrapper<T, Impl...>& wrapper) {
    os << wrapper.seq;
    os << wrapper.inner;
    os << wrapper.reset;
    return os;
}

template<typename ...Impl, typename T>
constexpr auto wrap(const T& val) noexcept // Can't use partial CTAD for OutputWrapper
{
    return OutputWrapper<T, Impl...>(val);
}

// Only insert sequences when condition is met (e.g., output is tty)
template<typename T, typename F, typename ...Impl>
struct ConditionalOutputWrapper : OutputWrapper<T, Impl...> {
    F cond; // ref or value(moved in)
};

template<typename ...Impl, typename T, typename F>
constexpr auto wrap(const T& val, F&& cond) noexcept
{
    return ConditionalOutputWrapper<T, F, Impl...>{val, std::forward<F>(cond)};
}

template<typename T, typename F, typename ...Impl>
std::ostream& operator<<(std::ostream& os, const ConditionalOutputWrapper<T, F, Impl...>& wrapper) {
    if (wrapper.cond()) {
        return os << static_cast<const OutputWrapper<T, Impl...>&>(wrapper);
    }
    return os << wrapper.inner;
}

// Alias

using BlackFg   = ColorFg<AnsiColor::Black>;
using RedFg     = ColorFg<AnsiColor::Red>;
using GreenFg   = ColorFg<AnsiColor::Green>;
using YellowFg  = ColorFg<AnsiColor::Yellow>;
using BlueFg    = ColorFg<AnsiColor::Blue>;
using MagentaFg = ColorFg<AnsiColor::Magenta>;
using CyanFg    = ColorFg<AnsiColor::Cyan>;
using WhiteFg   = ColorFg<AnsiColor::White>;

using BlackBg   = ColorBg<AnsiColor::Black>;
using RedBg     = ColorBg<AnsiColor::Red>;
using GreenBg   = ColorBg<AnsiColor::Green>;
using YellowBg  = ColorBg<AnsiColor::Yellow>;
using BlueBg    = ColorBg<AnsiColor::Blue>;
using MagentaBg = ColorBg<AnsiColor::Magenta>;
using CyanBg    = ColorBg<AnsiColor::Cyan>;
using WhiteBg   = ColorBg<AnsiColor::White>;

// 256 colors
template<int Id>
struct Fg256 : ColorFgBase {
    static constexpr auto code = mkfs("38;5;") + digits<Id>();
};

template<int Id>
struct Bg256 : ColorBgBase {
    static constexpr auto code = mkfs("48;5;") + digits<Id>();
};

// 24 bit True color
template<int R, int G, int B>
struct RgbFg : ColorFgBase {
    static constexpr auto code = mkfs("38;2;") + join(';', digits<R>(), digits<G>(), digits<B>());
};

template<int R, int G, int B>
struct RgbBg : ColorBgBase {
    static constexpr auto code = mkfs("48;2;") + join(';', digits<R>(), digits<G>(), digits<B>());
};

struct Bold {
    static constexpr auto code = digits<1>();
    static constexpr auto reset = digits<22>(); // 21
};

struct Dim {
    static constexpr auto code = digits<2>();
    static constexpr auto reset = digits<22>();
};

struct Italic {
    static constexpr auto code = digits<3>();
    static constexpr auto reset = digits<23>();
};

struct Underline {
    static constexpr auto code = digits<4>();
    static constexpr auto reset = digits<24>();
};

struct Blinking {
    static constexpr auto code = digits<5>();
    static constexpr auto reset = digits<25>();
};

struct Inverse {
    static constexpr auto code = digits<7>();
    static constexpr auto reset = digits<27>();
};

struct Hidden {
    static constexpr auto code = digits<8>();
    static constexpr auto reset = digits<28>();
};

struct Strikethrough  {
    static constexpr auto code = digits<9>();
    static constexpr auto reset = digits<29>();
};


// std::format support
#if defined(__cpp_lib_format)
template<int N>
struct std::formatter<FixedString<N>> { // it shouldn't consume any format-spec
    constexpr auto parse(const std::format_parse_context& ctx) const noexcept {
        return ctx.begin();
    }
    constexpr auto format(const FixedString<N>& arg, std::format_context& ctx) const {
        auto it = ctx.out();
        for (const auto& c : arg) {
            (*it++) = c;
        }
        return it;
    }
};

template<typename T, typename ...Impl>
struct std::formatter<OutputWrapper<T, Impl...>> : std::formatter<T> { // Use spec-parsing for T
    constexpr auto format(const OutputWrapper<T, Impl...>& arg, std::format_context& ctx) const {
        auto it = ctx.out();
        for (const auto& c : arg.seq) {
            (*it++) = c;
        }
        ctx.advance_to(it);
        it = std::formatter<T>::format(arg.inner, ctx);
        for (const auto& c : arg.reset) {
            (*it++) = c;
        }
        return it;
    }
};

template<typename T, typename F, typename ...Impl>
struct std::formatter<ConditionalOutputWrapper<T, F, Impl...>> : std::formatter<OutputWrapper<T, Impl...>> {
    constexpr auto format(const ConditionalOutputWrapper<T, F, Impl...>& arg, std::format_context& ctx) const {
        if (arg.cond()) {
            return std::formatter<OutputWrapper<T, Impl...>>::format(arg, ctx);
        } else {
            return std::formatter<T>::format(arg.inner, ctx);
        }
    }
};
#endif

} // namespace tc

#endif /* TERMCOLOR_HPP_A73B5B43_1C3B_42A8_B5BC_9AED37F243C5 */
