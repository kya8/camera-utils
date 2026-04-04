#ifndef RANGE_HPP_E0844149_6509_46D8_923A_EB4F29F0E6C6
#define RANGE_HPP_E0844149_6509_46D8_923A_EB4F29F0E6C6

#include <type_traits>
#include <cstddef>  // ptrdiff_t
#include <iterator> // std::size()

namespace range {

template<typename T,
std::enable_if_t<
    std::is_integral_v<T>
, bool> = true
>
struct IRange {
    using int_type = T;
    T i_begin, i_end;
    struct IRangeIterator {
        T current, end;
        auto operator*() const noexcept {
            return current;
        }
        auto& operator++() noexcept {
            ++current;
            return *this;
        };
        bool operator==(const IRangeIterator& rhs) const noexcept {
            if (rhs.current == rhs.end && current >= end) return true;
            return current == rhs.current;
        }
        bool operator!=(const IRangeIterator& rhs) const noexcept {
            return !(*this == rhs);
        }
    };

    auto begin() const noexcept {
        return IRangeIterator{i_begin, i_end};
    }
    auto end() const noexcept {
        return IRangeIterator{i_end, i_end};
    }

    IRange(T end) noexcept : i_begin(0), i_end(end) {}
    IRange(T begin, T end) noexcept : i_begin(begin), i_end(end) {}
};

using Index = IRange<std::ptrdiff_t>;

template<typename T,
std::enable_if_t<
    std::is_integral_v<T>
, bool> = true>
auto make_index(const T& end) noexcept
{
    using ReturnType = std::common_type_t<std::ptrdiff_t, std::make_signed_t<T>>;
    return IRange(static_cast<ReturnType>(end));
}

template<typename T,
typename = std::void_t<
    decltype(std::size(std::declval<T>()))
>>
auto make_index(const T& container) noexcept(noexcept(std::size(container)))
{
    return make_index(std::size(container));
}

}


#endif /* RANGE_HPP_E0844149_6509_46D8_923A_EB4F29F0E6C6 */
