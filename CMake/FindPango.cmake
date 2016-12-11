# - Try to find the pango library
# Once done this will define
#
#  PANGO_FOUND - system has pango
#  PANGO_INCLUDE_DIRS - the pango include directory
#  PANGO_LIBRARIES - Link these to use pango
#
# Define PANGO_MIN_VERSION for which version desired.
#

find_package(PkgConfig)

if(Pango_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "REQUIRED")
else(Pango_FIND_REQUIRED)
	set(_pkgconfig_REQUIRED "")
endif(Pango_FIND_REQUIRED)

if(PANGO_MIN_VERSION)
	pkg_check_modules(PANGO ${_pkgconfig_REQUIRED} "pango>=${PANGO_MIN_VERSION}" "pangocairo>=${PANGO_MIN_VERSION}")
else(PANGO_MIN_VERSION)
	pkg_check_modules(PANGO ${_pkgconfig_REQUIRED} pango pangocairo)
endif(PANGO_MIN_VERSION)

if(NOT PANGO_FOUND AND NOT PKG_CONFIG_FOUND)
	find_path(PANGO_INCLUDE_DIRS pango.h)
	find_library(PANGO_LIBRARIES pango pangocairo)
else(NOT PANGO_FOUND AND NOT PKG_CONFIG_FOUND)
	# Make paths absolute https://stackoverflow.com/a/35476270
	# Important on FreeBSD because /usr/local/lib is not on /usr/bin/ld's default path
	set(PANGO_LIBS_ABSOLUTE)
	foreach(lib ${PANGO_LIBRARIES})
		set(var_name PANGO_${lib}_ABS)
		find_library(${var_name} ${lib} ${PANGO_LIBRARY_DIRS})
		list(APPEND PANGO_LIBS_ABSOLUTE ${${var_name}})
	endforeach()
	set(PANGO_LIBRARIES ${PANGO_LIBS_ABSOLUTE})
endif(NOT PANGO_FOUND AND NOT PKG_CONFIG_FOUND)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PANGO DEFAULT_MSG PANGO_LIBRARIES PANGO_INCLUDE_DIRS)
mark_as_advanced(PANGO_LIBRARIES PANGO_INCLUDE_DIRS)
