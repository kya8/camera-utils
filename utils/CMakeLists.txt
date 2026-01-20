find_package(OpenCV 4.4 REQUIRED core imgcodecs imgproc calib3d)
find_package(Threads REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

set(MULTI_CALL_NAMES calibrator insta360_normalize mp4_join)
set(MAIN_NAME camerainfo) # This is arbitary

add_executable(camera_utils
camera_utils.hpp
calibrator.cpp
camerainfo.cpp
insta360_normalize.cpp
mp4_join.cpp
main.cpp
)

set_target_properties(camera_utils PROPERTIES OUTPUT_NAME ${MAIN_NAME})

target_link_libraries(camera_utils PRIVATE
common mp4utils caminfo ${OpenCV_LIBS} Eigen3::Eigen Threads::Threads)

install(TARGETS camera_utils)

# Install symlinks for the multi-call binary
foreach(name IN LISTS MULTI_CALL_NAMES)
    if(WIN32)
        set(name ${name}.exe)
    endif()

    # Create symlinks in the build tree
    add_custom_command(TARGET camera_utils POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:camera_utils> ${name}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:camera_utils>
    )

    # Copy the symlinks to the install tree
    install(PROGRAMS $<TARGET_FILE_DIR:camera_utils>/${name} TYPE BIN)
endforeach()
