# - Try to find the gdk-pixbuf-2.0 library
# Once done this will define
#
#  GDK_PIXBUF_FOUND - system has gdk-pixbuf-2.0
#  GDK_PIXBUF_INCLUDE_DIRS - the gdk-pixbuf-2.0 include directory
#  GDK_PIXBUF_LIBRARIES - Link these to use gdk-pixbuf-2.0
#
# Define GDK_PIXBUF_MIN_VERSION for which version desired.
#

INCLUDE(FindPkgConfig)

IF(GdkPixbuf_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "REQUIRED")
ELSE(GdkPixbuf_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "")
ENDIF(GdkPixbuf_FIND_REQUIRED)

IF(GDK_PIXBUF_MIN_VERSION)
        PKG_SEARCH_MODULE(GDK_PIXBUF ${_pkgconfig_REQUIRED} "gdk-pixbuf-2.0>=${GDK_PIXBUF_MIN_VERSION}")
ELSE(GDK_PIXBUF_MIN_VERSION)
        PKG_SEARCH_MODULE(GDK_PIXBUF ${_pkgconfig_REQUIRED} "gdk-pixbuf-2.0")
ENDIF(GDK_PIXBUF_MIN_VERSION)

IF(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)
        FIND_PATH(GDK_PIXBUF_INCLUDE_DIRS gdk-pixbuf/gdk-pixbuf.h)
        FIND_LIBRARY(GDK_PIXBUF_LIBRARIES gdk_pixbuf-2.0)

        # Report results
        IF(GDK_PIXBUF_LIBRARIES AND GDK_PIXBUF_INCLUDE_DIRS)
                SET(GDK_PIXBUF_FOUND 1)
                SET(GdkPixbuf_FOUND 1)
                IF(NOT GdkPixbuf_FIND_QUIETLY)
                        MESSAGE(STATUS "Found GdkPixbuf: ${GDK_PIXBUF_LIBRARIES}")
                ENDIF(NOT GdkPixbuf_FIND_QUIETLY)
        ELSE(GDK_PIXBUF_LIBRARIES AND GDK_PIXBUF_INCLUDE_DIRS)
                IF(GdkPixbuf_FIND_REQUIRED)
                        MESSAGE(SEND_ERROR "Could not find GdkPixbuf")
                ELSE(GdkPixbuf_FIND_REQUIRED)
                        IF(NOT GdkPixbuf_FIND_QUIETLY)
                                MESSAGE(STATUS "Could not find GdkPixbuf")
                        ENDIF(NOT GdkPixbuf_FIND_QUIETLY)
                ENDIF(GdkPixbuf_FIND_REQUIRED)
        ENDIF(GDK_PIXBUF_LIBRARIES AND GDK_PIXBUF_INCLUDE_DIRS)
ELSE(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)
        SET(GdkPixbuf_FOUND 1)
ENDIF(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED(GDK_PIXBUF_LIBRARIES GDK_PIXBUF_INCLUDE_DIRS)
