# - Try to find the cairo library
# Once done this will define
#
#  CAIRO_FOUND - system has cairo
#  CAIRO_INCLUDE_DIRS - the cairo include directory
#  CAIRO_LIBRARIES - Link these to use cairo
#
# Define CAIRO_MIN_VERSION for which version desired.
#

INCLUDE(FindPkgConfig)

IF(Cairo_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "REQUIRED")
ELSE(Cairo_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "")
ENDIF(Cairo_FIND_REQUIRED)

IF(CAIRO_MIN_VERSION)
        PKG_SEARCH_MODULE(CAIRO ${_pkgconfig_REQUIRED} cairo>=${CAIRO_MIN_VERSION})
ELSE(CAIRO_MIN_VERSION)
        PKG_SEARCH_MODULE(CAIRO ${_pkgconfig_REQUIRED} cairo)
ENDIF(CAIRO_MIN_VERSION)

IF(NOT CAIRO_FOUND AND NOT PKG_CONFIG_FOUND)
        FIND_PATH(CAIRO_INCLUDE_DIRS cairo.h)
        FIND_LIBRARY(CAIRO_LIBRARIES cairo)

        # Report results
        IF(CAIRO_LIBRARIES AND CAIRO_INCLUDE_DIRS)
                SET(CAIRO_FOUND 1)
                IF(NOT Cairo_FIND_QUIETLY)
                        MESSAGE(STATUS "Found Cairo: ${CAIRO_LIBRARIES}")
                ENDIF(NOT Cairo_FIND_QUIETLY)
        ELSE(CAIRO_LIBRARIES AND CAIRO_INCLUDE_DIRS)
                IF(Cairo_FIND_REQUIRED)
                        MESSAGE(SEND_ERROR "Could not find Cairo")
                ELSE(Cairo_FIND_REQUIRED)
                        IF(NOT Cairo_FIND_QUIETLY)
                                MESSAGE(STATUS "Could not find Cairo")
                        ENDIF(NOT Cairo_FIND_QUIETLY)
                ENDIF(Cairo_FIND_REQUIRED)
        ENDIF(CAIRO_LIBRARIES AND CAIRO_INCLUDE_DIRS)
ENDIF(NOT CAIRO_FOUND AND NOT PKG_CONFIG_FOUND)

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED(CAIRO_LIBRARIES CAIRO_INCLUDE_DIRS)
