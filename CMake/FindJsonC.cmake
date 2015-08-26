# - Find json-c
# Find the json-c libraries
#
#  This module defines the following variables:
#     JSONC_FOUND        - True if JSONC is found
#     JSONC_LIBRARIES    - JSONC libraries
#     JSONC_INCLUDE_DIRS - JSONC include directories
#

find_package(PkgConfig)
pkg_check_modules(PC_JSONC QUIET JSONC)
find_path(JSONC_INCLUDE_DIRS NAMES json-c/json.h HINTS ${PC_JSONC_INCLUDE_DIRS})
find_library(JSONC_LIBRARIES NAMES json-c HINTS ${PC_JSONC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONC DEFAULT_MSG JSONC_LIBRARIES JSONC_INCLUDE_DIRS)
mark_as_advanced(JSONC_LIBRARIES JSONC_INCLUDE_DIRS)
