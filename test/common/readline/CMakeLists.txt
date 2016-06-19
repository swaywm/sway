configure_test(
    SUBPROJECT common
    NAME readline
    SOURCES
        ${PROJECT_SOURCE_DIR}/common/readline.c
        readline.c
    WRAPPERS
        fgetc
)
