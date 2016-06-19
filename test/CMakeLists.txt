set(test_targets "")

add_subdirectory(common)

add_custom_target(check
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_SOURCE_DIR}/test/runner "${CMAKE_BINARY_DIR}")

if(enable-coverage)
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    if(NOT GCOV_PATH)
        MESSAGE(FATAL_ERROR "gcov not found! Aborting...")
    endif()
    if(NOT LCOV_PATH)
        MESSAGE(FATAL_ERROR "gcov not found! Aborting...")
    endif()
    if(NOT GENHTML_PATH)
        MESSAGE(FATAL_ERROR "gcov not found! Aborting...")
    endif()
endif()
