# - Find XKBCommon
# Once done, this will define
#
#   XKBCOMMON_FOUND - System has XKBCommon
#   XKBCOMMON_INCLUDE_DIRS - The XKBCommon include directories
#   XKBCOMMON_LIBRARIES - The libraries needed to use XKBCommon
#   XKBCOMMON_DEFINITIONS - Compiler switches required for using XKBCommon

find_package(PkgConfig)
pkg_check_modules(PC_XKBCOMMON QUIET xkbcommon)
find_path(XKBCOMMON_INCLUDE_DIRS NAMES xkbcommon/xkbcommon.h HINTS ${PC_XKBCOMMON_INCLUDE_DIRS})
find_library(XKBCOMMON_LIBRARIES NAMES xkbcommon HINTS ${PC_XKBCOMMON_LIBRARY_DIRS})

set(XKBCOMMON_DEFINITIONS ${PC_XKBCOMMON_CFLAGS_OTHER})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XKBCOMMON DEFAULT_MSG XKBCOMMON_LIBRARIES XKBCOMMON_INCLUDE_DIRS)
mark_as_advanced(XKBCOMMON_LIBRARIES XKBCOMMON_INCLUDE_DIRS)

