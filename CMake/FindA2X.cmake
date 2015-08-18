#
# (c)2015 KiCad Developers
# (c)2015 Brian Sidebotham <brian.sidebotham@gmail.com>
#
# CMake module to find a2x (part of the asciidoc toolchain).
#
# Variables generated:
#
# A2X_FOUND     true when A2X_COMMAND is valid
# A2X_COMMAND   The command to run a2x (may be a list including an interpreter)
# A2X_VERSION   The a2x version that has been found
#

# Have a go at finding a a2x executable
find_program( A2X_PROGRAM a2x )

# Found something, attempt to try and use it...
if( A2X_PROGRAM )
    execute_process(
        COMMAND ${A2X_PROGRAM} --version
        OUTPUT_VARIABLE _OUT
        ERROR_VARIABLE _ERR
        RESULT_VARIABLE _RES
        OUTPUT_STRIP_TRAILING_WHITESPACE )

    # If it worked, set the A2X_COMMAND
    if( _RES MATCHES 0 )
        set( A2X_COMMAND "${A2X_PROGRAM}" )
    endif()
endif()

# If nothing could be found, test to see if we can just find the script file,
# that we'll then run with the python interpreter
if( NOT A2X_COMMAND )
    find_file( A2X_SCRIPT a2x.py )

    if( A2X_SCRIPT )
        # Find the python interpreter quietly
        if( NOT PYTHONINTERP_FOUND )
            find_package( PYTHONINTERP QUIET )
        endif()

        if( NOT PYTHONINTERP_FOUND )
            # Python's not available so can't find a2x...
            set( A2X_COMMAND "" )
        else()
            # Build the python based command
            set( A2X_COMMAND "${PYTHON_EXECUTABLE}" "${A2X_SCRIPT}" )

            execute_process(
                COMMAND ${A2X_COMMAND} --version
                OUTPUT_VARIABLE _OUT
                ERROR_VARIABLE _ERR
                RESULT_VARIABLE _RES
                OUTPUT_STRIP_TRAILING_WHITESPACE )

            # If it still can't be run, then give up
            if( NOT _RES MATCHES 0 )
                set( A2X_COMMAND "" )
            endif()
        endif()
    endif()
endif()

# If we've found a command that works, check the version
if( A2X_COMMAND )
    string(REGEX REPLACE ".*a2x[^0-9.]*\([0-9.]+\).*" "\\1" A2X_VERSION "${_OUT}")
endif()

# Generate the *_FOUND as necessary, etc.
include( FindPackageHandleStandardArgs )
find_package_handle_standard_args(
    A2X
    REQUIRED_VARS A2X_COMMAND
    VERSION_VAR A2X_VERSION )
