#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

typedef enum {
	L_SILENT = 0,
	L_ERROR = 1,
	L_INFO = 2,
	L_DEBUG = 3,
	L_LAST,
} log_importance_t;

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

void _sway_abort(const char *filename, ...) ATTRIB_PRINTF(1, 2);
#define sway_abort(FMT, ...) \
    _sway_abort("[%s:%d] " FMT, sway_strip_path(__FILE__), __LINE__, ##__VA_ARGS__)

bool _sway_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, "[%s:%d] %s:" FMT, sway_strip_path(__FILE__), __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void error_handler(int sig);

typedef void (*log_callback_t)(log_importance_t importance, const char *fmt, va_list args);

// Will log all messages less than or equal to `verbosity`
// If `callback` is NULL, sway will use its default logger.
void sway_log_init(log_importance_t verbosity, log_callback_t callback);

void _sway_log(log_importance_t verbosity, const char *format, ...) ATTRIB_PRINTF(2, 3);
void _sway_vlog(log_importance_t verbosity, const char *format, va_list args) ATTRIB_PRINTF(2, 0);
const char *sway_strip_path(const char *filepath);

#define sway_log(verb, fmt, ...) \
	_sway_log(verb, "[%s:%d] " fmt, sway_strip_path(__FILE__), __LINE__, ##__VA_ARGS__)

#define sway_vlog(verb, fmt, args) \
	_sway_vlog(verb, "[%s:%d] " fmt, sway_strip_path(__FILE__), __LINE__, args)

#define sway_log_errno(verb, fmt, ...) \
	sway_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#endif
