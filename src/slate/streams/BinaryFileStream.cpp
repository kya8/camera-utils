#ifdef _WIN32
// Use POSIX compatible functions from UCRT.
#include <sys/stat.h>
#include <sys/types.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <sys/stat.h>
#endif

#include "BinaryFileStream.hpp"
#include "slate/streams/BinaryStream.hpp"

extern "C" {
#include "stdio.h"
}

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

namespace {

const char* convert(FileStreamMode mode) noexcept
{

    switch (mode) {
    case FileStreamMode::Read          : return "rb";
    case FileStreamMode::Write         : return "wb";
    case FileStreamMode::Append        : return "ab";
    case FileStreamMode::ReadExtended  : return "r+b";
    case FileStreamMode::WriteExtended : return "w+b";
    case FileStreamMode::AppendExtended: return "a+b";
    }
    return nullptr;
}

// Assumes f is open
OffsetType get_file_size(std::FILE* f) noexcept
{
#ifdef _WIN32
    const auto fd = _fileno(f);
    struct _stat64 st;
    if (_fstat64(fd, &st) == -1) {
        return -1;
    }
    return st.st_size;
#else
    const auto fd = fileno(f);
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return -1;
    }
    return st.st_size;
#endif
}

} // namespace

BinaryFileStream::BinaryFileStream(const char* filename, FileStreamMode mode)
{
    assert(filename != nullptr);
    if (!open(filename, mode)) {
        throw StreamError("Error opening file");
    }
}

bool
BinaryFileStream::open(const char* filename, FileStreamMode mode) noexcept
{
    assert(filename != nullptr);
    if (fp) {
        return false;
    }

    const auto mode_str = convert(mode);
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
        fsize = get_file_size(fp);
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
