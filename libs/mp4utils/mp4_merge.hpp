#ifndef MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A
#define MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A

#include <functional>

namespace mp4utils {

enum class MergeResult {
    Success = 0,
    InvalidInput,
    IoError,
    InternalError
};

using MergeProgCb = std::function<void(int prog)>;

// Merge split/chaptered MP4/ISOBMFF files. Input files are assumed to have identical structures and settings.
MergeResult merge_mp4(int nb_input, const char* const* input_files, const char* output_file, const MergeProgCb& prog_cb = {}) noexcept;

}


#endif /* MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A */
