#ifndef MEMORY_CURSOR_HPP_D2C9EA7C_C6F9_47E4_A269_9397DBD06FEB
#define MEMORY_CURSOR_HPP_D2C9EA7C_C6F9_47E4_A269_9397DBD06FEB

#include "BinaryStream.hpp"
#include <cstring> // memcpy
#include <cassert>

namespace slate {

namespace detail {

template<bool Mutable>
struct Byte {
    using type = const unsigned char;
};
template<>
struct Byte<true> {
    using type = unsigned char;
};

template<bool Mutable>
using BytePointer = typename Byte<Mutable>::type*;

template<bool Mutable>
class CursorCommon {
protected:
    using Pointer = BytePointer<Mutable>;
public:
    CursorCommon(Pointer buf, std::ptrdiff_t len) noexcept : 
        begin_(buf), len_(len), cursor_(buf) {
        assert(buf != nullptr);
        assert(len >= 0);
    }

    [[nodiscard]] bool is_open() const noexcept { return this->begin_ != nullptr; }
    [[nodiscard]] auto data() const noexcept { return this->begin_; }
    [[nodiscard]] auto len() const noexcept { return this->len_; }
    static constexpr bool is_mutable = Mutable;

    [[nodiscard]] OffsetType tell() const noexcept { return cursor_ - begin_; }
    void seek(OffsetType offset, SeekFrom from = SeekFrom::Begin)
    {
        switch (from) {
        case SeekFrom::Begin:
            if (offset > len_ || offset < 0)
                throw StreamIoError("Memory cursor seek out of range");
            cursor_ = begin_ + offset;
            break;
        case SeekFrom::Current: {
            const auto pos = cursor_ - begin_ + offset;
            if (pos > len_ || pos < 0)
                throw StreamIoError("Memory cursor seek out of range");
            cursor_ += offset;
            break;
        }
        case SeekFrom::End:
            if (offset < -len_ || offset > 0)
                throw StreamIoError("Memory cursor seek out of range");
            cursor_ = begin_ + len_ + offset;
            break;
        }
    }

    void read(void* buf, std::size_t n)
    {
        if (static_cast<std::ptrdiff_t>(n) > len_ - (cursor_ - begin_))
            throw StreamIoError("Memory cursor read overflow");
        std::memcpy(buf, cursor_, n);
        cursor_ += n;
    }

protected:
    Pointer begin_ = nullptr;
    std::ptrdiff_t len_ = 0;
    Pointer cursor_ = nullptr;
};

} // namespace detail

template<bool Mutable>
class Cursor;

template<>
class Cursor<false> : public detail::CursorCommon<false>, public ReadStreamTag, public ReadStreamMixin {
public:
    Cursor(const void* buf, std::ptrdiff_t len) noexcept : detail::CursorCommon<false>(static_cast<Pointer>(buf), len) {}
};

template<>
class Cursor<true> : public detail::CursorCommon<true>, public RWStreamTag, public RWStreamMixin {
public:
    Cursor(void* buf, std::ptrdiff_t len) noexcept : detail::CursorCommon<true>(static_cast<Pointer>(buf), len) {}
    void write(const void* buf, std::size_t n)
    {
        if (static_cast<std::ptrdiff_t>(n) > len_ - (cursor_ - begin_))
            throw StreamIoError("Memory cursor write overflow");
        std::memcpy(cursor_, buf, n);
        cursor_ += n;
    }
};

// CTAD guides
Cursor(const void*, std::ptrdiff_t) -> Cursor<false>;
Cursor(void*, std::ptrdiff_t) -> Cursor<true>;

} // namespace slate

#endif /* MEMORY_CURSOR_HPP_D2C9EA7C_C6F9_47E4_A269_9397DBD06FEB */
