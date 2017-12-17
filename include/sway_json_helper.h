#ifndef _SWAY_JSON_HELPER_H
#define _SWAY_JSON_HELPER_H

#include <json-c/json.h>

// Macros for checking a specific version.
#define JSON_C_VERSION_013 (13 << 8)

// json-c v0.13 uses size_t for array_list_length().
#if defined(JSON_C_VERSION_NUM) && JSON_C_VERSION_NUM >= JSON_C_VERSION_013
typedef size_t json_ar_len_t;
#else
typedef int json_ar_len_t;
#endif

#endif // _SWAY_JSON_HELPER_H
