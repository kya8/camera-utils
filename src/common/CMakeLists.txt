find_package(Boost CONFIG REQUIRED) # circular_buffer, header-only

file(GLOB_RECURSE slate_common_sources CONFIGURE_DEPENDS *.h *.hpp *.cpp *.c)

add_library(slate_common STATIC ${slate_common_sources} ${GENERATED_SOURCE_DIR}/version.cpp)
add_library(slate::common ALIAS slate_common)

target_link_libraries(slate_common PRIVATE Threads::Threads PUBLIC Boost::headers)
target_include_directories(slate_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(slate_common PRIVATE SLATE_BUILD_TYPE=\"$<CONFIG>\")
