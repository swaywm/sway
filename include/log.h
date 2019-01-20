#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

typedef enum {
	SWAY_SILENT = 0,
	SWAY_ERROR = 1,
	SWAY_INFO = 2,
	SWAY_DEBUG = 3,
	SWAY_LOG_IMPORTANCE_LAST,
} sway_log_importance_t;

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

void error_handler(int sig);

typedef void (*terminate_callback_t)(int exit_code);

// Will log all messages less than or equal to `verbosity`
// The `terminate` callback is called by `sway_abort`
void sway_log_init(sway_log_importance_t verbosity, terminate_callback_t terminate);

void _sway_log(sway_log_importance_t verbosity, const char *format, ...) ATTRIB_PRINTF(2, 3);
void _sway_vlog(sway_log_importance_t verbosity, const char *format, va_list args) ATTRIB_PRINTF(2, 0);
void _sway_abort(const char *filename, ...) ATTRIB_PRINTF(1, 2);
bool _sway_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);

// TODO: get meson to precompute this, for better reproducibility/less overhead
const char *_sway_strip_path(const char *filepath);

#define sway_log(verb, fmt, ...) \
	_sway_log(verb, "[%s:%d] " fmt, _sway_strip_path(__FILE__), __LINE__, ##__VA_ARGS__)

#define sway_vlog(verb, fmt, args) \
	_sway_vlog(verb, "[%s:%d] " fmt, _sway_strip_path(__FILE__), __LINE__, args)

#define sway_log_errno(verb, fmt, ...) \
	sway_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#define sway_abort(FMT, ...) \
	_sway_abort("[%s:%d] " FMT, _sway_strip_path(__FILE__), __LINE__, ##__VA_ARGS__)

#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, "[%s:%d] %s:" FMT, _sway_strip_path(__FILE__), __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#endif
