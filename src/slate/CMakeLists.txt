option(SLATE_ENABLE_ANY "Include std::any" OFF)

find_package(Protobuf REQUIRED)
find_package(Threads)
find_package(Eigen3 CONFIG REQUIRED)

file(GLOB_RECURSE slate_sources CONFIGURE_DEPENDS
    ${SLATE_PUBLIC_INCLUDE_DIR}/slate/*.hpp
    ${SLATE_PUBLIC_INCLUDE_DIR}/slate/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.proto
)

add_library(slate ${slate_sources})
add_library(slate::slate ALIAS slate)

set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
protobuf_generate(
  TARGET slate
  LANGUAGE cpp
  PROTOC_OUT_DIR ${PROTO_GEN_DIR}
)

target_include_directories(slate
PUBLIC
    $<BUILD_INTERFACE:${SLATE_PUBLIC_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
PRIVATE
    ${PROTO_GEN_DIR}
    ${SLATE_SRC_DIR}
)

target_link_libraries(slate
PRIVATE gpmf-parser protobuf::libprotobuf slate::common
PUBLIC Eigen3::Eigen # Insta360 transforms
)

# export macros
target_compile_definitions(slate PRIVATE SLATE_SOURCE)
if(BUILD_SHARED_LIBS)
target_compile_definitions(slate PUBLIC SLATE_SHARED_LIB)
endif()

# Workaround a GCC/libstdc++ bug, triggered by protobuf.
# Ref:
# https://github.com/protocolbuffers/protobuf/issues/6479
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55394
# (Only for linking to static protobuf library.)
if(Threads_FOUND)
target_link_libraries(slate PRIVATE Threads::Threads)
endif()

if(SLATE_ENABLE_ANY)
target_compile_definitions(slate PUBLIC SLATE_ENABLE_ANY)
endif()

install(TARGETS slate)
install(DIRECTORY ${SLATE_PUBLIC_INCLUDE_DIR}/slate TYPE INCLUDE)
