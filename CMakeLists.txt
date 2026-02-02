cmake_minimum_required(VERSION 3.25.0 FATAL_ERROR)

if(NOT GENERATOR_IS_MULTI_CONFIG)
  # Default to Release if not set
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build.")
endif()


set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON) # let vcpkg install dependencies with install()

project(camera-utils
LANGUAGES CXX
VERSION 0.1.0)

option(CAMERAUTILS_OPTIMIZED_X86_64 "Enable optimized x86-64 builds, targetting x86-64-v3 or equivalent." ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Prevent full source file path.
# Should also be set for (statically) linked libraries.
# no full path should be the default for MSVC (i.e. no /FC)
# See https://gitlab.kitware.com/cmake/cmake/-/merge_requests/2306
add_compile_options("$<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>")

# Set source encoding to UTF-8
#add_compile_options("$<IF:$<CXX_COMPILER_ID:MSVC>,/source-charset:utf-8,-finput-charset=utf-8>")

# Optimized x86-64 build
if(CAMERAUTILS_OPTIMIZED_X86_64 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-march=x86-64-v3" COMPILER_SUPPORTS_MARCH_X86_64_V3)
  add_compile_options($<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX2,$<GENEX_EVAL:$<IF:$<BOOL:${COMPILER_SUPPORTS_MARCH_X86_64_V3}>,-march=x86-64-v3,-march=haswell>>>)
endif()

# Do not export symbols by default
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

#-------- Version & Build info
include(cmake/git-version.cmake)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

#--------

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
find_package(Boost CONFIG REQUIRED) # circular_buffer, header-only

# add some warning flags
add_compile_options("$<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall;-Wextra;-pedantic>")

# MSVC conformance mode
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/permissive->)
# Enable updated __cplusplus macro for MSVC
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>)

# Define feature test macros for POSIX
if(UNIX)
add_compile_definitions(_POSIX_C_SOURCE=200809L)
if(APPLE)
add_compile_definitions(_DARWIN_C_SOURCE=1)
endif()
endif()

set(COMMON_DIR libs/common)
configure_file(cmake/version.cpp.in ${GENERATED_SOURCE_DIR}/version.cpp @ONLY)
add_library(common STATIC
${COMMON_DIR}/range.hpp
${COMMON_DIR}/version.hpp ${GENERATED_SOURCE_DIR}/version.cpp
${COMMON_DIR}/version_print.cpp
${COMMON_DIR}/ThreadPool.hpp ${COMMON_DIR}/ThreadPoolFwd.hpp
${COMMON_DIR}/conqueue.h
${COMMON_DIR}/timer.hpp ${COMMON_DIR}/tuple_utils.hpp ${COMMON_DIR}/helper_templates.hpp
${COMMON_DIR}/sys_utils.hpp ${COMMON_DIR}/sys_utils.cpp
${COMMON_DIR}/time_utils.hpp ${COMMON_DIR}/time_utils.cpp
${COMMON_DIR}/fs.hpp ${COMMON_DIR}/fs.cpp
${COMMON_DIR}/builtins.h
${COMMON_DIR}/string_utils.hpp
)
target_link_libraries(common PRIVATE Threads::Threads PUBLIC Boost::headers)
target_include_directories(common PUBLIC ${COMMON_DIR})
target_compile_definitions(common PRIVATE CAMERAUTILS_BUILD_TYPE=\"$<CONFIG>\")

add_subdirectory(third_party/gpmf-parser)
add_subdirectory(libs/mp4utils)
add_subdirectory(libs/caminfo)
add_subdirectory(utils)

#include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${GIT_DESC}_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
if(CMAKE_BUILD_TYPE)
STRING(APPEND CPACK_PACKAGE_FILE_NAME "-${CMAKE_BUILD_TYPE}")
endif()

#set(CPACK_THREADS 0)
set(CPACK_GENERATOR "TZST")
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_STRIP_FILES ON)
include(CPack)
