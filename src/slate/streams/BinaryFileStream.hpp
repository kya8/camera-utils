#ifndef BINARY_FILE_STREAM_HPP_E7292879_5125_4AAA_9BE7_DF944BD6B10A
#define BINARY_FILE_STREAM_HPP_E7292879_5125_4AAA_9BE7_DF944BD6B10A

#include "BinaryStream.hpp"
#include "lfs.h"  // 64-bit ftell/fseek

namespace slate {

enum class FileStreamMode {
    Read,
    Write,
    Append,
    ReadExtended,
    WriteExtended,
    AppendExtended
};

class BinaryFileStream : public RWStreamTag, public RWStreamMixin {
public:
    BinaryFileStream() noexcept = default;
    ~BinaryFileStream() noexcept;

    BinaryFileStream(const BinaryFileStream&) = delete;
    BinaryFileStream& operator=(const BinaryFileStream&) = delete;
    BinaryFileStream(BinaryFileStream&& rhs) noexcept;
    BinaryFileStream& operator=(BinaryFileStream&& rhs) noexcept;

    bool open(const char* filename, FileStreamMode mode = FileStreamMode::Read) noexcept;
    bool close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] OffsetType get_length() const noexcept;

    void read(void* buf, std::size_t n)
    {
        // if (!is_open) return false; // this should be a pre-condition
        if (n == 0)
            return;
        if (fread(buf, n, 1, fp) != 1)
            throw StreamIoError("File stream read error");
    }
    void write(const void* buf, std::size_t n)
    {
        // if (!is_open) return false;
        if (n == 0)
            return;
        if (fwrite(buf, n, 1, fp) != 1)
            throw StreamIoError("File stream write error");
    }
    void seek(OffsetType offset, SeekFrom from = SeekFrom::Begin)
    {
        // if (!is_open) return false;
        if (detail::fseek64(fp, offset, [from]{
            switch(from) {
                case(SeekFrom::Begin)  : return SEEK_SET;
                case(SeekFrom::Current): return SEEK_CUR;
                case(SeekFrom::End)    : return SEEK_END;
            }
            return SEEK_SET;
        }()) != 0)
            throw StreamIoError("File stream seek error");
    }
    [[nodiscard]] OffsetType tell() const
    {
        // if (!is_open) return -1;
        const auto ret = detail::ftell64(fp);
        if (ret == -1L)
            throw StreamIoError{"File stream tell error"};
        return ret;
    }

private:
    FILE* fp = nullptr;
    OffsetType fsize = -1;
};

} // namespace slate

#endif /* BINARY_FILE_STREAM_HPP_E7292879_5125_4AAA_9BE7_DF944BD6B10A */
