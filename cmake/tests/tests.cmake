include(CheckCXXSourceCompiles)

# set(CMAKE_REQUIRED_QUIET ON)

function(test_and_set file define)
    file(READ ${file} test_file_content)
    check_cxx_source_compiles("${test_file_content}" test_${define})
    if(test_${define})
        set(${define} TRUE PARENT_SCOPE)
        add_compile_definitions(${define}=1)
        message(STATUS "Test for ${define} passed.")
    else()
        message(STATUS "Test for ${define} failed.")
    endif()
endfunction()

test_and_set(${SLATE_CMAKE_DIR}/tests/move_only_function.cpp HAVE_MOVE_ONLY_FUNCTION)
