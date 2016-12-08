# - Try to find the gdk-pixbuf-2.0 library
# Once done this will define
#
#  GDK_PIXBUF_FOUND - system has gdk-pixbuf-2.0
#  GDK_PIXBUF_INCLUDE_DIRS - the gdk-pixbuf-2.0 include directory
#  GDK_PIXBUF_LIBRARIES - Link these to use gdk-pixbuf-2.0
#
# Define GDK_PIXBUF_MIN_VERSION for which version desired.
#

find_package(PkgConfig)

if(GdkPixbuf_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "REQUIRED")
else(GdkPixbuf_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "")
endif(GdkPixbuf_FIND_REQUIRED)

if(GDK_PIXBUF_MIN_VERSION)
	pkg_check_modules(GDK_PIXBUF ${_pkgconfig_REQUIRED} "gdk-pixbuf-2.0>=${GDK_PIXBUF_MIN_VERSION}")
else(GDK_PIXBUF_MIN_VERSION)
	pkg_check_modules(GDK_PIXBUF ${_pkgconfig_REQUIRED} "gdk-pixbuf-2.0")
endif(GDK_PIXBUF_MIN_VERSION)

if(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)
	find_path(GDK_PIXBUF_INCLUDE_DIRS gdk-pixbuf/gdk-pixbuf.h)
	find_library(GDK_PIXBUF_LIBRARIES gdk_pixbuf-2.0)
else(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)
	SET(GdkPixbuf_FOUND 1)
	# Make paths absolute https://stackoverflow.com/a/35476270
	# Important on FreeBSD because /usr/local/lib is not on /usr/bin/ld's default path
	set(GDK_PIXBUF_LIBS_ABSOLUTE)
	foreach(lib ${GDK_PIXBUF_LIBRARIES})
		set(var_name GDK_PIXBUF_${lib}_ABS)
		find_library(${var_name} ${lib} ${GDK_PIXBUF_LIBRARY_DIRS})
		list(APPEND GDK_PIXBUF_LIBS_ABSOLUTE ${${var_name}})
	endforeach()
	set(GDK_PIXBUF_LIBRARIES ${GDK_PIXBUF_LIBS_ABSOLUTE})
endif(NOT GDK_PIXBUF_FOUND AND NOT PKG_CONFIG_FOUND)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDK_PIXBUF DEFAULT_MSG GDK_PIXBUF_LIBRARIES GDK_PIXBUF_INCLUDE_DIRS)
mark_as_advanced(GDK_PIXBUF_LIBRARIES GDK_PIXBUF_INCLUDE_DIRS)
