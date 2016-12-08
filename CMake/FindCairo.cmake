# - Try to find the cairo library
# Once done this will define
#
#  CAIRO_FOUND - system has cairo
#  CAIRO_INCLUDE_DIRS - the cairo include directory
#  CAIRO_LIBRARIES - Link these to use cairo
#
# Define CAIRO_MIN_VERSION for which version desired.
#

find_package(PkgConfig)

if(Cairo_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "REQUIRED")
else(Cairo_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "")
endif(Cairo_FIND_REQUIRED)

if(CAIRO_MIN_VERSION)
	pkg_check_modules(CAIRO ${_pkgconfig_REQUIRED} cairo>=${CAIRO_MIN_VERSION})
else(CAIRO_MIN_VERSION)
	pkg_check_modules(CAIRO ${_pkgconfig_REQUIRED} cairo)
endif(CAIRO_MIN_VERSION)

if(NOT CAIRO_FOUND AND NOT PKG_CONFIG_FOUND)
	find_path(CAIRO_INCLUDE_DIRS cairo.h)
	find_library(CAIRO_LIBRARIES cairo)
else(NOT CAIRO_FOUND AND NOT PKG_CONFIG_FOUND)
	# Make paths absolute https://stackoverflow.com/a/35476270
	# Important on FreeBSD because /usr/local/lib is not on /usr/bin/ld's default path
	set(CAIRO_LIBS_ABSOLUTE)
	foreach(lib ${CAIRO_LIBRARIES})
		set(var_name CAIRO_${lib}_ABS)
		find_library(${var_name} ${lib} ${CAIRO_LIBRARY_DIRS})
		list(APPEND CAIRO_LIBS_ABSOLUTE ${${var_name}})
	endforeach()
	set(CAIRO_LIBRARIES ${CAIRO_LIBS_ABSOLUTE})
endif(NOT CAIRO_FOUND AND NOT PKG_CONFIG_FOUND)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAIRO DEFAULT_MSG CAIRO_LIBRARIES CAIRO_INCLUDE_DIRS)
mark_as_advanced(CAIRO_LIBRARIES CAIRO_INCLUDE_DIRS)
