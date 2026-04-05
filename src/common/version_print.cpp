#include "version.hpp"
#include <cstdio>

void slate::version::print_info() noexcept
{
    const Version& v = get();
    std::printf("slate %s, %s.\nBuild type %s, %s %s, compiled with %s %s\n",
                v.git_desc, v.commit_date,
                v.build_type,
                v.target_os, v.target_arch,
                v.compiler_name, v.compiler_version);
}
