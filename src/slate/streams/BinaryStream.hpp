#ifndef BINARY_STREAM_HPP_AD884129_07F3_4D4A_A1B9_ECDD347E6022
#define BINARY_STREAM_HPP_AD884129_07F3_4D4A_A1B9_ECDD347E6022

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <tuple>
#include <bit>
#include <utility>
#include <concepts>

namespace slate {

using OffsetType = std::int64_t;

enum class SeekFrom {
    Begin,
    Current,
    End
};

enum class Endian {
    LE,
    BE
};

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big, "mixed endian is not supported");

inline constexpr Endian target_endian = (std::endian::native == std::endian::big) ? Endian::BE : Endian::LE;

struct StreamError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct StreamIoError : StreamError {
    using StreamError::StreamError;
};

// Tags for stream concepts.
// Implementors should inherit from these tags.
struct ReadStreamTag {};
struct WriteStreamTag {};
struct RWStreamTag : ReadStreamTag, WriteStreamTag {};

// Concept for basic stream operations.
template<typename T>
concept BasicStream = requires(T a, OffsetType off, SeekFrom seek) {
    { a.is_open() } -> std::same_as<bool>;
    a.seek(off, seek);
    { a.tell() } -> std::same_as<OffsetType>;
};

// Concept for reader stream.
template<typename T>
concept ReadStream = BasicStream<T> && requires(T a, void* p, std::size_t size) {
    a.read(p, size);
} && std::derived_from<T, ReadStreamTag>;

// Concept for writer stream.
template<typename T>
concept WriteStream = BasicStream<T> && requires(T a, const void* p, std::size_t size) {
    a.write(p, size);
} && std::derived_from<T, WriteStreamTag>;

// Mixin class for readers
class ReadStreamMixin {
public:
    template <Endian endian=Endian::BE, typename T, unsigned BytesToRead = sizeof(T), ReadStream Self>
    // signed integer with BytesToRead < sizeof(T) is not supported
    requires (
        std::is_arithmetic_v<T> && BytesToRead <= sizeof(T) && !(std::is_signed_v<T> && BytesToRead < sizeof(T))
        )
    void read_num(this Self& self, T& dest)
    {
        if constexpr (BytesToRead < sizeof(T)) dest = 0; // zero-out bytes. For unsigned integers only
        static constexpr unsigned offset = target_endian == Endian::LE ? 0 : sizeof(T) - BytesToRead;
        const auto p = reinterpret_cast<unsigned char*>(&dest) + offset;
        self.read(p, BytesToRead);
        if constexpr (endian != target_endian) std::reverse(p, p + BytesToRead);
    }

    template <Endian endian=Endian::BE, typename T, unsigned BytesToRead = sizeof(T), ReadStream Self>
    requires (std::is_arithmetic_v<T> && BytesToRead <= sizeof(T) && !(std::is_signed_v<T> && BytesToRead < sizeof(T)))
    T read_num(this Self& self)
    {
        T out{};
        static constexpr unsigned offset = endian == Endian::LE ? 0 : sizeof(T) - BytesToRead;
        const auto p = reinterpret_cast<unsigned char*>(&out) + offset;
        self.read(p, BytesToRead);
        if constexpr (endian == target_endian) {
            return out;
        } else {
            return std::byteswap(out);
        }
    }

    // runtime endianness
    template <typename T, unsigned BytesToRead = sizeof(T), ReadStream Self> // We have to deduce Self here to the actual type, otherwise read_num isn't avaiable by ourself, since it needs to meet ReadStream
    requires (std::is_arithmetic_v<T> && BytesToRead <= sizeof(T) && !(std::is_signed_v<T> && BytesToRead < sizeof(T)))
    void read_num(this Self& self, Endian endian, T& dest) {
        switch (endian) {
        case Endian::BE:
            self.template read_num<Endian::BE, T, BytesToRead>(dest);
            break;
        case Endian::LE:
            self.template read_num<Endian::LE, T, BytesToRead>(dest);
            break;
        default:
            std::unreachable();
        }
    }

    template <typename T, unsigned BytesToRead = sizeof(T), ReadStream Self>
    requires (std::is_arithmetic_v<T> && BytesToRead <= sizeof(T) && !(std::is_signed_v<T> && BytesToRead < sizeof(T)))
    T read_num(this Self& self, Endian endian) {
        switch (endian) {
        case Endian::BE:
            return self.template read_num<Endian::BE, T, BytesToRead>();
        case Endian::LE:
            return self.template read_num<Endian::LE, T, BytesToRead>();
        default:
            std::unreachable();
        }
    }

    template <Endian endian=Endian::BE, typename ...Ts, ReadStream Self>
    requires (std::is_arithmetic_v<Ts> && ...)
    void read_nums(this Self& self, Ts& ...Args) {
        (self.template read_num<endian>(Args), ...);
    }

    template<Endian endian=Endian::BE, typename ...Ts, ReadStream Self>
    requires (std::is_arithmetic_v<Ts> && ...)
    std::tuple<Ts...> read_nums(this Self& self) {
        return {self.template read_num<endian, Ts>()...};
    }

    template <typename ...Ts, ReadStream Self>
    requires (std::is_arithmetic_v<Ts> && ...)
    void read_nums(this Self& self, Endian endian, Ts& ...Args) {
        switch (endian) {
        case Endian::BE:
            self.template read_nums<Endian::BE>(Args...);
            break;
        case Endian::LE:
            self.template read_nums<Endian::LE>(Args...);
            break;
        default:
            std::unreachable();
        }
    }

    template <typename ...Ts, ReadStream Self>
    requires (std::is_arithmetic_v<Ts> && ...)
    std::tuple<Ts...> read_nums(this Self& self, Endian endian) {
        switch (endian) {
        case Endian::BE:
            return self.template read_nums<Endian::BE, Ts...>();
        case Endian::LE:
            return self.template read_nums<Endian::LE, Ts...>();
        default:
            std::unreachable();
        }
    }
};

// Mixin class for writer streams
class WriteStreamMixin {
public:
    template<ReadStream Src, WriteStream Self>
    void copy_from(this Self& self, Src& in, std::size_t n) // Generic copy via userspace buffer
    {
        if (!(in.is_open() && self.is_open())) {
            throw StreamError{"Stream not open"};
        }
        const auto buf_sz = n > 4 * 1024 * 1024 ? 4 * 1024 * 1024 : n;

#ifdef __cpp_lib_smart_ptr_for_overwrite
        const auto buf = std::make_unique_for_overwrite<unsigned char[]>(buf_sz); // Should use stack-allocated array for small buf_sz
#else
        const auto buf = std::make_unique<unsigned char[]>(buf_sz);
#endif
        while (n > 0) {
            const auto to_read = n > buf_sz ? buf_sz : n;
            in.read(buf.get(), to_read);
            self.write(buf.get(), to_read);
            n -= to_read;
        }
    }

    template<WriteStream Self>
    void patch_bytes(this Self& self, OffsetType offset, const void* buf, std::size_t n) {
        const auto mark = self.tell();
        self.seek(offset, SeekFrom::Begin);
        self.write(buf, n);
        self.seek(mark, SeekFrom::Begin);
    }

    template <Endian endian=Endian::BE, typename T, unsigned BytesToWrite = sizeof(T), WriteStream Self>
    requires (std::is_arithmetic_v<T> && BytesToWrite <= sizeof(T) && !(std::is_signed_v<T> && BytesToWrite < sizeof(T)))
    void write_num(this Self& self, const T& src)
    {
        static constexpr unsigned offset = target_endian == Endian::LE ? 0 : sizeof(T) - BytesToWrite;
        const auto p = reinterpret_cast<const unsigned char*>(&src) + offset;
        if constexpr (endian != target_endian) {
            unsigned char buf[BytesToWrite];
            std::reverse_copy(p, p + BytesToWrite, buf);
            self.write(buf, BytesToWrite);
        }
        else {
            self.write(p, BytesToWrite);
        }
    }

    template <Endian endian=Endian::BE, typename ...Ts, WriteStream Self>
    requires (std::is_arithmetic_v<Ts> && ...)
    void write_nums(this Self& self, const Ts& ...Args) {
        (self.template write_num<endian>(Args), ...);
    }

    template <Endian endian=Endian::BE, typename T, unsigned BytesToWrite = sizeof(T), WriteStream Self>
    void patch_num(this Self& self, OffsetType offset, const T &src)
    {
        const auto mark = self.tell();
        self.seek(offset);
        self.template write_num<endian, T, BytesToWrite>(src);
        self.seek(mark);
    }
};

// Mixin class for RW streams.
class RWStreamMixin : public ReadStreamMixin, public WriteStreamMixin {};

} // namespace slate

#endif /* BINARY_STREAM_HPP_AD884129_07F3_4D4A_A1B9_ECDD347E6022 */
