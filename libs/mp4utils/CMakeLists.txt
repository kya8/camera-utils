cmake_minimum_required(VERSION 3.15.0)
project(mp4utils VERSION 0.1.0 LANGUAGES CXX)

#file(GLOB ALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h *.hpp *.cpp)

add_library(binary_streams STATIC
binary_streams/lfs.h binary_streams/BinaryFileStream.hpp
binary_streams/BinaryFileStream.cpp
binary_streams/BinaryStream.hpp binary_streams/endian.h
binary_streams/MemoryCursor.hpp
)
target_include_directories(binary_streams INTERFACE .)

add_library(mp4utils STATIC
mp4.hpp mp4.cpp mp4_merge.hpp mp4_merge.cpp fourcc.hpp
track.hpp track.cpp video_info.hpp extract_video_info.hpp extract_video_info.cpp
)
target_include_directories(mp4utils PUBLIC .)
target_link_libraries(mp4utils PRIVATE common binary_streams)
