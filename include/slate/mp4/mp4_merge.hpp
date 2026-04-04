#ifndef MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A
#define MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A

namespace slate::mp4 {

enum class MergeResult {
    Success = 0,
    InvalidInput, // Input files do not look like valid MP4 files.
                  // This is returned by an initial shallow check. 
    IoError,      // An I/O error occurred.
    InternalError // An internal error occurred.
                  // This is mostly likely caused by bad/invalid input files,
                  // e.g. input files are valid MP4 files, but they are not compatible for merging.
};

struct MergeProgCb {
    void(*cb)(void*, int) = nullptr;
    void* data;

    operator bool() const noexcept {
        return cb != nullptr;
    }

    void operator()(int prog) const {
        cb(data, prog);
    }
};

/**
 * Merge split/chaptered MP4 video files.
 * The input files are assumed to be in the correct order and have identical configurations (e.g., resolution, codec, etc.).
 * 
 * @param[in] nb_input Number of input files
 * @param[in] input_files Array of input file paths. Each path is a null-terminated string, in platform-native narrow encoding.
 * @param[in] output_file Output file path. Follows the same encoding rules as input files.
 * @param[in] prog_cb Optional callback for progress updates. If not empty, the callback will be called with
 *                    the current progress percentage.
 *
 * @return MergeResult indicating the result of the operation.
 */
MergeResult merge_mp4(int nb_input, const char* const* input_files, const char* output_file, MergeProgCb prog_cb = {}) noexcept;

} // namespace slate::mp4


#endif /* MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A */
