find_package(A2X)

if (A2X_FOUND)
    add_custom_target(man ALL)
endif()

function(add_manpage name section locale)
    if (NOT A2X_FOUND)
        return()
    endif()

    if (${locale} STREQUAL "en")
        set(
            input
            ${CMAKE_CURRENT_SOURCE_DIR}/man/${name}.${section}.txt
        )
        set(
            output
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}
        )
        set(
            destination
            ${CMAKE_INSTALL_FULL_DATAROOTDIR}/man/man${section}
        )
    else()
        set(
            input
            ${CMAKE_CURRENT_SOURCE_DIR}/man/${name}.${section}.${locale}.txt
        )
        set(
            output
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}.${locale}
        )
        set(
            destination
            ${CMAKE_INSTALL_FULL_DATAROOTDIR}/man/${locale}/man${section}
        )
    endif()

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${A2X_COMMAND}
                --no-xmllint
                --doctype manpage
                --format manpage
                -D ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
                ${input}
        DEPENDS ${input}
        COMMENT Generating manpage for ${name}.${section}.${locale}
    )

    add_custom_target(man-${name}.${section}.${locale}
        DEPENDS ${output}
    )
    add_dependencies(man
        man-${name}.${section}.${locale}
    )

    install(
        FILES ${output}
        DESTINATION ${destination}
        COMPONENT documentation
        RENAME ${name}.${section}
    )
endfunction()
