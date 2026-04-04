#include "version.hpp"
#include <cstdio>

void slate::version::print_info() noexcept
{
    std::printf("slate %s, %s. Build type %s, %s %s, compiled with %s %s\n",
                GIT_DESC, COMMIT_DATE,
                BUILD_TYPE,
                TARGET_OS, TARGET_ARCH,
                COMPILER_NAME, COMPILER_VERSION);
}
