cmake_minimum_required(VERSION 3.13.0)
project(caminfo LANGUAGES CXX)

option(CAMINFO_ENABLE_ANY "Include std::any" OFF)

find_package(Protobuf REQUIRED)
find_package(Threads)
find_package(Eigen3 CONFIG REQUIRED)

add_library(caminfo STATIC
varmap.hpp varmap.cpp value_types.hpp
caminfo_fwd.hpp caminfo.hpp caminfo.cpp
detectors.hpp gopro.cpp insta360.cpp insta360_metadata.proto
gpmf_helper.hpp
extras/insta360_params.hpp extras/insta360_params.cpp
extras/insta360_tf.hpp extras/insta360_tf.cpp
)

set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
protobuf_generate(
  TARGET caminfo
  LANGUAGE cpp
  PROTOC_OUT_DIR ${PROTO_GEN_DIR}
)
target_include_directories(caminfo PUBLIC . PRIVATE ${PROTO_GEN_DIR})

target_link_libraries(caminfo
PRIVATE mp4utils gpmf-parser protobuf::libprotobuf common
PUBLIC Eigen3::Eigen # Insta360 transforms
)

# Workaround a GCC/libstdc++ bug, triggered by protobuf.
# Ref:
# https://github.com/protocolbuffers/protobuf/issues/6479
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55394
# (Only for linking to static protobuf library.)
if(Threads_FOUND)
target_link_libraries(caminfo PRIVATE Threads::Threads)
endif()

if(CAMINFO_ENABLE_ANY)
target_compile_definitions(caminfo PUBLIC CAMINFO_ENABLE_ANY)
endif()
