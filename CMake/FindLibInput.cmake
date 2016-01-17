#.rst:
# FindLibInput
# -------
#
# Find LibInput library
#
# Try to find LibInpu library. The following values are defined
#
# ::
#
#   LIBINPUT_FOUND         - True if libinput is available
#   LIBINPUT_INCLUDE_DIRS  - Include directories for libinput
#   LIBINPUT_LIBRARIES     - List of libraries for libinput
#   LIBINPUT_DEFINITIONS   - List of definitions for libinput
#
# and also the following more fine grained variables
#
# ::
#
#   LIBINPUT_VERSION
#   LIBINPUT_VERSION_MAJOR
#   LIBINPUT_VERSION_MINOR
#   LIBINPUT_VERSION_MICRO
#
#=============================================================================
# Copyright (c) 2015 Jari Vetoniemi
#
# Distributed under the OSI-approved BSD License (the "License");
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

include(FeatureSummary)
set_package_properties(LibInput PROPERTIES
   URL "http://freedesktop.org/wiki/Software/libinput/"
   DESCRIPTION "Library to handle input devices")

find_package(PkgConfig)
pkg_check_modules(PC_INPUT QUIET libinput)
find_library(LIBINPUT_LIBRARIES NAMES input HINTS ${PC_INPUT_LIBRARY_DIRS})
find_path(LIBINPUT_INCLUDE_DIRS libinput.h HINTS ${PC_INPUT_INCLUDE_DIRS})

set(LIBINPUT_VERSION ${PC_INPUT_VERSION})
string(REPLACE "." ";" VERSION_LIST "${PC_INPUT_VERSION}")

LIST(LENGTH VERSION_LIST n)
if (n EQUAL 3)
   list(GET VERSION_LIST 0 LIBINPUT_VERSION_MAJOR)
   list(GET VERSION_LIST 1 LIBINPUT_VERSION_MINOR)
   list(GET VERSION_LIST 2 LIBINPUT_VERSION_MICRO)
endif ()

# This is compatible with libinput-version.h that exists in upstream
# but isn't in distribution (probably forgotten)
set(LIBINPUT_DEFINITIONS ${PC_INPUT_CFLAGS_OTHER}
   -DLIBINPUT_VERSION=\"${LIBINPUT_VERSION}\"
   -DLIBINPUT_VERSION_MAJOR=${LIBINPUT_VERSION_MAJOR}
   -DLIBINPUT_VERSION_MINOR=${LIBINPUT_VERSION_MINOR}
   -DLIBINPUT_VERSION_MICRO=${LIBINPUT_VERSION_MICRO})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBINPUT DEFAULT_MSG LIBINPUT_INCLUDE_DIRS LIBINPUT_LIBRARIES)
mark_as_advanced(LIBINPUT_INCLUDE_DIRS LIBINPUT_LIBRARIES LIBINPUT_DEFINITIONS
   LIBINPUT_VERSION LIBINPUT_VERSION_MAJOR LIBINPUT_VERSION_MICRO LIBINPUT_VERSION_MINOR)
