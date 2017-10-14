find_package(A2X)

if (A2X_FOUND)
    add_custom_target(man ALL)

    function(add_manpage name section)
        add_custom_command(
            OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}
            COMMAND ${A2X_COMMAND}
                    --no-xmllint
                    --doctype manpage
                    --format manpage
                    -D ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
                    ${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt
            COMMENT Generating manpage for ${name}.${section}
        )

        add_custom_target(man-${name}.${section}
            DEPENDS
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}
        )
        add_dependencies(man
            man-${name}.${section}
        )

        install(
            FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}
            DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man${section}
            COMPONENT documentation
        )
    endfunction()
endif()
