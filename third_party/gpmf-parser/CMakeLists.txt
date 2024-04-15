project(gpmf-parser
        LANGUAGES C
        )

set(CMAKE_C_EXTENSIONS OFF)

set(Headers lib/GPMF_bitstream.h lib/GPMF_common.h lib/GPMF_parser.h lib/GPMF_utils.h)
add_library(gpmf-parser STATIC lib/GPMF_parser.c lib/GPMF_utils.c ${Headers})

target_include_directories(gpmf-parser PUBLIC lib)
