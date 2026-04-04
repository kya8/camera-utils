#ifndef MP4_HPP_DE4096F4_7B89_48AC_80FE_63F4719F37EF
#define MP4_HPP_DE4096F4_7B89_48AC_80FE_63F4719F37EF

#include "slate/streams/BinaryFileStream.hpp"
#include <stdexcept>
#include <vector>

namespace slate::mp4 {

// base exception class
class error : public StreamError {
public:
    using StreamError::StreamError;
};


// Binary stream for MP4 file.
// This class is mainly for mp4_merge.
class Mp4Stream : public BinaryFileStream {
public:
    using BinaryFileStream::BinaryFileStream;

    bool open(const char* filename) noexcept;

    // This holds stream-specific info for an atom.
    struct AtomInfo {
        uint64_t offset;       // offset to header within file
        uint64_t size;         // size reported in header
        uint32_t fourcc;       // atom type
        uint32_t header_size;  // size of header, either 8 or 16

        uint64_t data_size()   const { return size - header_size; }
        uint64_t data_offset() const { return offset + header_size; }
        uint64_t end_offset()  const { return offset + size; }
    };

    // Must be used at the beginning of an atom header.
    // Maybe this (and below) should be changed to no-throwing, e.g. std::optional.
    AtomInfo parse_atom();
    //AtomInfo peekAtom();

    // Must be used at the start of a container atom, or beginning of file.
    // Throws if not found.
    AtomInfo seek_to_atom_data(uint32_t fourcc, uint64_t max_range);

    // Additional overload that seeks inside a container box.
    // Will seek to container data first.
    AtomInfo seek_to_atom_data(uint32_t fourcc, const AtomInfo& container);

    // Get all valid atoms within a range.
    // Should be used at the start of a container atom, or beginning of file.
    std::vector<AtomInfo> get_all_atoms(uint64_t max_range) noexcept;

    // Simple check for mp4 file format.
    // This will loop over all top-level atoms, and look for the 'moov' box.
    bool verify() noexcept;

    std::vector<unsigned char> read_atom_data(const AtomInfo& atom);

};

} // namespace slate::mp4


#endif /* MP4_HPP_DE4096F4_7B89_48AC_80FE_63F4719F37EF */
