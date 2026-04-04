#include "BinaryFileStream.hpp"

namespace slate {

BinaryFileStream::~BinaryFileStream() noexcept
{
    if (fp) {
        fclose(fp);
    }
}

BinaryFileStream::BinaryFileStream(BinaryFileStream&& rhs) noexcept : fp(rhs.fp), fsize(rhs.fsize)
{
    rhs.fp = nullptr;
}

BinaryFileStream& BinaryFileStream::operator=(BinaryFileStream&& rhs) noexcept
{
    close();
    fp = rhs.fp;
    fsize = rhs.fsize;
    rhs.fp = nullptr;
    return *this;
}

bool
BinaryFileStream::open(const char* filename, FileStreamMode mode) noexcept
{
    if (fp) {
        return false;
    }

    const auto mode_str = [mode]() -> const char* {
        switch (mode) {
        case FileStreamMode::Read:
            return "rb";
        case FileStreamMode::Write:
            return "wb";
        case FileStreamMode::Append:
            return "ab";
        case FileStreamMode::ReadExtended:
            return "r+b";
        case FileStreamMode::WriteExtended:
            return "w+b";
        case FileStreamMode::AppendExtended:
            return "a+b";
        }
        return nullptr;
    }();
    if (!mode_str) {
        return false;
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    fp = fopen(filename, mode_str);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    if (!fp) {
        return false;
    }

    if (mode == FileStreamMode::Read || mode == FileStreamMode::ReadExtended) {
        if (detail::fseek64(fp, 0, SEEK_END) != 0) {
            return false;
        }
        fsize = detail::ftell64(fp);
        detail::fseek64(fp, 0, SEEK_SET);
        if (fsize < 0) {
            return false;
        }
    }
    return true;
}

bool BinaryFileStream::close() noexcept
{
    if (!fp) {
        return false;
    }

    fclose(fp);
    fp = nullptr;
    fsize = -1;
    return true;
}

bool BinaryFileStream::is_open() const noexcept
{
    return fp != nullptr;
}

OffsetType BinaryFileStream::get_length() const noexcept
{
    return fsize;
}

} // namespace slate
