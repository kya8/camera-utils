#ifndef LFS_H_FCE95A8D_403B_46B6_A38D_026EA17CCAA1
#define LFS_H_FCE95A8D_403B_46B6_A38D_026EA17CCAA1

#ifndef _WIN32 // Assume POSIX
#define _FILE_OFFSET_BITS 64
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

extern "C" {
#include <stdio.h>
}

// Wrappers for 64-bit fseek/ftell

namespace slate::detail {

#ifdef _WIN32
inline auto fseek64(FILE* stream, long long offset, int whence) {
    return _fseeki64(stream, offset, whence);
}
inline auto ftell64(FILE* stream) {
    return _ftelli64(stream);
}
#else
inline auto fseek64(FILE* stream, off_t offset, int whence) {
    return fseeko(stream, offset, whence);
}
inline auto ftell64(FILE* stream) {
    return ftello(stream);
}
#endif

} // namespace slate::detail

#endif /* LFS_H_FCE95A8D_403B_46B6_A38D_026EA17CCAA1 */
