find_package(OpenCV 4.4 REQUIRED core imgcodecs imgproc calib3d)
find_package(Threads REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

set(MULTI_CALL_NAMES calibrator insta360_normalize mp4join)
set(MAIN_NAME slate) # This is arbitary

file(GLOB_RECURSE slate_utils_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(slate_utils ${slate_utils_sources})

set_target_properties(slate_utils PROPERTIES
    OUTPUT_NAME ${MAIN_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)

target_link_libraries(slate_utils
PRIVATE
    slate::common slate::slate ${OpenCV_LIBS} Eigen3::Eigen Threads::Threads
)

install(TARGETS slate_utils)

# Install symlinks for the multi-call binary
foreach(name IN LISTS MULTI_CALL_NAMES)
    if(WIN32)
        set(name ${name}.exe)
    endif()

    # Create symlinks in the build tree
    add_custom_command(TARGET slate_utils POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:slate_utils> ${name}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:slate_utils>
    )

    # Copy the symlinks to the install tree
    install(PROGRAMS $<TARGET_FILE_DIR:slate_utils>/${name} TYPE BIN)
endforeach()
