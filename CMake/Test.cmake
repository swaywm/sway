find_package(A2X REQUIRED)

function(configure_test)
    set(options)
    set(oneValueArgs NAME SUBPROJECT)
    set(multiValueArgs WRAPPERS SOURCES INCLUDES LIBRARIES)
    cmake_parse_arguments(CONFIGURE_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    include_directories(
        ${CMOCKA_INCLUDE_DIR}
        ${CONFIGURE_TEST_INCLUDES}
    )
    add_definitions(${CMOCKA_DEFINITIONS})

    set(
        CMAKE_RUNTIME_OUTPUT_DIRECTORY
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test/${CONFIGURE_TEST_SUBPROJECT}/${CONFIGURE_TEST_NAME}
    )

    add_executable(${CONFIGURE_TEST_NAME}_test
        ${CMAKE_SOURCE_DIR}/test/util.c
        ${CONFIGURE_TEST_SOURCES}
    )

    list(APPEND CONFIGURE_TEST_WRAPPERS "malloc" "calloc" "realloc" "free")

    list(LENGTH CONFIGURE_TEST_WRAPPERS WRAPPED_COUNT)

    if(NOT ${WRAPPED_COUNT} STREQUAL "0")
        set(WRAPPED "")

        foreach(WRAPPER ${CONFIGURE_TEST_WRAPPERS})
            string(REGEX REPLACE "\\n" "" WRAPPER "${WRAPPER}")
            set(WRAPPED
                "${WRAPPED} \
                -Wl,--wrap=${WRAPPER}"
            )
        endforeach()

        set_target_properties(${CONFIGURE_TEST_NAME}_test
            PROPERTIES
            LINK_FLAGS "${WRAPPED}"
        )
    endif()

    target_link_libraries(${CONFIGURE_TEST_NAME}_test ${CMOCKA_LIBRARIES} ${CONFIGURE_TEST_LIBRARIES})

    set(test_targets ${test_targets} ${CONFIGURE_TEST_NAME}_test PARENT_SCOPE)
endfunction()
