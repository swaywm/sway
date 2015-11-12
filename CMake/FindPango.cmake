# - Try to find the pango library
# Once done this will define
#
#  PANGO_FOUND - system has pango
#  PANGO_INCLUDE_DIRS - the pango include directory
#  PANGO_LIBRARIES - Link these to use pango
#
# Define PANGO_MIN_VERSION for which version desired.
#

INCLUDE(FindPkgConfig)

IF(Pango_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "REQUIRED")
ELSE(Pango_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "")
ENDIF(Pango_FIND_REQUIRED)

IF(PANGO_MIN_VERSION)
        PKG_SEARCH_MODULE(PANGO ${_pkgconfig_REQUIRED} "pango>=${PANGO_MIN_VERSION} pangocairo>=${PANGO_MIN_VERSION}")
ELSE(PANGO_MIN_VERSION)
        PKG_SEARCH_MODULE(PANGO ${_pkgconfig_REQUIRED} "pango pangocairo")
ENDIF(PANGO_MIN_VERSION)

IF(NOT PANGO_FOUND AND NOT PKG_CONFIG_FOUND)
        FIND_PATH(PANGO_INCLUDE_DIRS pango.h)
        FIND_LIBRARY(PANGO_LIBRARIES pango pangocairo)

        # Report results
        IF(PANGO_LIBRARIES AND PANGO_INCLUDE_DIRS)
                SET(PANGO_FOUND 1)
                IF(NOT Pango_FIND_QUIETLY)
                        MESSAGE(STATUS "Found Pango: ${PANGO_LIBRARIES}")
                ENDIF(NOT Pango_FIND_QUIETLY)
        ELSE(PANGO_LIBRARIES AND PANGO_INCLUDE_DIRS)
                IF(Pango_FIND_REQUIRED)
                        MESSAGE(SEND_ERROR "Could not find Pango")
                ELSE(Pango_FIND_REQUIRED)
                        IF(NOT Pango_FIND_QUIETLY)
                                MESSAGE(STATUS "Could not find Pango")
                        ENDIF(NOT Pango_FIND_QUIETLY)
                ENDIF(Pango_FIND_REQUIRED)
        ENDIF(PANGO_LIBRARIES AND PANGO_INCLUDE_DIRS)
ENDIF(NOT PANGO_FOUND AND NOT PKG_CONFIG_FOUND)

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED(PANGO_LIBRARIES PANGO_INCLUDE_DIRS)
