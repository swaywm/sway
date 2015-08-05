# - Find wlc
# Find the wlc libraries
#
#  This module defines the following variables:
#     WLC_FOUND        - True if wlc is found
#     WLC_LIBRARIES    - wlc libraries
#     WLC_INCLUDE_DIRS - wlc include directories
#     WLC_DEFINITIONS  - Compiler switches required for using wlc
#

find_package(PkgConfig)
pkg_check_modules(PC_WLC QUIET wlc)
find_path(WLC_INCLUDE_DIRS NAMES wlc/wlc.h HINTS ${PC_WLC_INCLUDE_DIRS})
find_library(WLC_LIBRARIES NAMES wlc HINTS ${PC_WLC_LIBRARY_DIRS})

set(WLC_DEFINITIONS ${PC_WLC_CFLAGS_OTHER})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(wlc DEFAULT_MSG WLC_LIBRARIES WLC_INCLUDE_DIRS)
mark_as_advanced(WLC_LIBRARIES WLC_INCLUDE_DIRS)
