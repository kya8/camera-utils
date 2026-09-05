#ifndef MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A
#define MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A

#include "slate/export.h"
#include <memory> // unique_ptr
#include <string_view>

namespace slate::mp4 {

enum class MergeResult {
    Success = 0,
    InvalidConfig, // Input / output was not set.
    InvalidInput,  // Input files do not look like valid MP4 files.
                   // This is returned by an initial shallow check.
    IoError,       // An I/O error occurred.
    InternalError  // An internal error occurred.
                   // This is mostly likely caused by bad/invalid input files,
                   // e.g. input files are valid MP4 files, but they are not compatible for merging.
};

// Merge split/chaptered MP4 video files.
// The input files are assumed to be in the correct order and have identical configurations (e.g., resolution, codec, etc.).

class Mp4Merger {
public:
    /**
     * Add an input filename. The filename is a null-terminated string, in platform-native narrow encoding.
     * The string view is copied.
     * You must add at-least 2 input files for merging.
     * @return `*this`
     */
    SLATE_EXPORT Mp4Merger& add_input(std::string_view filename) noexcept;
    /**
     * Set output file path.
     * @return `*this`
     */
    SLATE_EXPORT Mp4Merger& set_output(std::string_view filename) noexcept;
    /**
     * Set the callback used for signaling progress.
     * If not set, no callback is called.
     * @return `*this`
     */
    SLATE_EXPORT Mp4Merger& set_progress_callback(void(*fn)(void*, int), void* data) noexcept;
    /**
     * Start merging.
     * @return MergeResult indicating the result of the operation.
     */
    SLATE_EXPORT MergeResult run() noexcept;

    SLATE_EXPORT Mp4Merger() noexcept;
    SLATE_EXPORT ~Mp4Merger() noexcept;
    SLATE_EXPORT Mp4Merger(Mp4Merger&&) noexcept;
    SLATE_EXPORT Mp4Merger& operator=(Mp4Merger&&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace slate::mp4

#endif /* MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A */
