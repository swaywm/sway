# - Find wlroots
# Find the wlroots libraries
#
#  This module defines the following variables:
#     WLR_FOUND        - True if wlroots is found
#     WLR_LIBRARIES    - wlroots libraries
#     WLR_INCLUDE_DIRS - wlroots include directories
#     WLR_DEFINITIONS  - Compiler switches required for using wlroots
#

find_package(PkgConfig)
pkg_check_modules(PC_WLR QUIET wlroots)
find_path(WLR_INCLUDE_DIRS NAMES wlr/config.h HINTS ${PC_WLR_INCLUDE_DIRS})
find_library(WLR_LIBRARIES NAMES wlroots HINTS ${PC_WLR_LIBRARY_DIRS})

set(WLR_DEFINITIONS ${PC_WLR_CFLAGS_OTHER})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(wlr DEFAULT_MSG WLR_LIBRARIES WLR_INCLUDE_DIRS)
mark_as_advanced(WLR_LIBRARIES WLR_INCLUDE_DIRS)
