#ifndef GPMF_HELPER_HPP_C6C4D424_779A_4F4C_93C7_BA4B6CCCCF08
#define GPMF_HELPER_HPP_C6C4D424_779A_4F4C_93C7_BA4B6CCCCF08

#include <GPMF_parser.h>
#include <cstddef>

namespace slate {

#if 0
// Could use IIFE

// alloc and init a gpmf-parser stream
inline std::unique_ptr<GPMF_stream, decltype(&GPMF_Free)>
makeGpmfStream(const std::vector<unsigned char>& data) noexcept
{
    auto deleter = [](GPMF_stream* gs) noexcept {
        auto ret = GPMF_Free(gs);
        delete gs;
        return ret;
    };

    const auto p = new(std::nothrow) GPMF_stream;
    if(!p) return {nullptr, deleter};

    if (GPMF_Init(p, reinterpret_cast<uint32_t*>(const_cast<unsigned char*>(data.data())), static_cast<uint32_t>(data.size())) == GPMF_OK) {
        return {p, deleter};
    }
    else {
        delete p;
        return {nullptr, deleter};
    }
}
#endif

// Owning wrapper for GPMF_stream
class GpmfWrapper {
public:
    GpmfWrapper(const void* data, std::ptrdiff_t size) noexcept : init_ok_{GPMF_Init(&stream_, (uint32_t*)data, (uint32_t)size) == GPMF_OK} {}
    ~GpmfWrapper() noexcept
    {
        if (init_ok_) GPMF_Free(&stream_);
    }
    auto get() const noexcept { return &stream_; }
    auto get() noexcept { return &stream_; }
    auto is_ok() const noexcept { return init_ok_; }
    operator bool() const noexcept { return is_ok(); }

    // Copy is disabled.
    GpmfWrapper(const GpmfWrapper&) = delete;
    GpmfWrapper& operator=(const GpmfWrapper&) = delete;

private:
    GPMF_stream stream_;
    bool init_ok_;
};

// RAII scope guard for gpmf stream
class GpmfGuard {
public:
    GpmfGuard(GPMF_stream& stream, const void* data, std::ptrdiff_t size) noexcept : stream_{stream}
    {
        if (GPMF_Init(&stream_, (uint32_t*)data, (uint32_t)size) == GPMF_OK)
            init_ok_ = true;
    }
    ~GpmfGuard() noexcept
    {
        if (init_ok_) GPMF_Free(&stream_);
    }
    auto is_open() const noexcept { return init_ok_; }
    auto get_stream() const noexcept { return &stream_; }

    // Copy is disabled.
    GpmfGuard(const GpmfGuard&) = delete;
    GpmfGuard& operator=(const GpmfGuard&) = delete;

private:
    GPMF_stream& stream_;
    bool init_ok_ = false;
};

#endif /* GPMF_HELPER_HPP_C6C4D424_779A_4F4C_93C7_BA4B6CCCCF08 */

} // namespace slate
