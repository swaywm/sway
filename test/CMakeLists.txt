set(test_targets "")

add_subdirectory(common)

add_custom_target(check
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_SOURCE_DIR}/test/runner)
