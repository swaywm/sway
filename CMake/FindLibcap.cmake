#.rst:
# FindLibcap
# -------
#
# Find Libcap library
#
# Try to find Libcap library. The following values are defined
#
# ::
#
#   Libcap_FOUND         - True if Libcap is available
#   Libcap_INCLUDE_DIRS  - Include directories for Libcap
#   Libcap_LIBRARIES     - List of libraries for Libcap
#   Libcap_DEFINITIONS   - List of definitions for Libcap
#
# and also the following more fine grained variables
#
# ::
#
#   Libcap_VERSION
#   Libcap_VERSION_MAJOR
#   Libcap_VERSION_MINOR
#
#=============================================================================
# Copyright (c) 2017 Jerzi Kaminsky
#
# Distributed under the OSI-approved BSD License (the "License");
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

include(FeatureSummary)
set_package_properties(Libcap PROPERTIES
   URL "https://www.kernel.org/pub/linux/libs/security/linux-privs/libcap2"
   DESCRIPTION "Library for getting and setting POSIX.1e capabilities")

find_package(PkgConfig)
pkg_check_modules(PC_CAP QUIET Libcap)
find_library(Libcap_LIBRARIES NAMES cap HINTS ${PC_CAP_LIBRARY_DIRS})
find_path(Libcap_INCLUDE_DIRS sys/capability.h HINTS ${PC_CAP_INCLUDE_DIRS})

set(Libcap_VERSION ${PC_CAP_VERSION})
string(REPLACE "." ";" VERSION_LIST "${PC_CAP_VERSION}")

LIST(LENGTH VERSION_LIST n)
if (n EQUAL 2)
   list(GET VERSION_LIST 0 Libcap_VERSION_MAJOR)
   list(GET VERSION_LIST 1 Libcap_VERSION_MINOR)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libcap DEFAULT_MSG Libcap_INCLUDE_DIRS Libcap_LIBRARIES)

IF(Libcap_FOUND)
  IF(NOT TARGET Libcap::Libcap)
    add_library(Libcap::Libcap UNKNOWN IMPORTED)
    set_target_properties(Libcap::Libcap PROPERTIES
        IMPORTED_LOCATION "${Libcap_LIBRARIES}"
        INTERFACE_COMPILE_OPTIONS "${Libcap_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libcap_INCLUDE_DIRS}"
    )
  ENDIF()
ENDIF()

mark_as_advanced(Libcap_INCLUDE_DIRS Libcap_LIBRARIES Libcap_DEFINITIONS
   Libcap_VERSION Libcap_VERSION_MAJOR Libcap_VERSION_MICRO Libcap_VERSION_MINOR)
