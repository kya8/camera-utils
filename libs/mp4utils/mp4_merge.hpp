#ifndef MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A
#define MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A

namespace mp4utils {

enum class MergeResult {
    Success = 0,
    InvalidInput,
    IoError,
    InternalError
};

struct MergeProgCb {
    void(*cb)(void*, int);
    void* data;

    operator bool() const noexcept {
        return cb != nullptr;
    }

    void operator()(int prog) const {
        cb(data, prog);
    }
};

// Merge split/chaptered MP4/ISOBMFF files. Input files are assumed to have identical structures and settings.
MergeResult merge_mp4(int nb_input, const char* const* input_files, const char* output_file, MergeProgCb prog_cb) noexcept;

}


#endif /* MP4_MERGE_HPP_C328A04A_1474_4AEB_A13A_C2AFFF31804A */
