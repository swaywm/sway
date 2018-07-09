#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H
#include <stdbool.h>
#include <wlr/util/log.h>

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

void _sway_abort(const char *filename, ...) ATTRIB_PRINTF(1, 2);
#define sway_abort(FMT, ...) \
    _sway_abort("[%s:%d] " FMT, _wlr_strip_path(__FILE__), __LINE__, ##__VA_ARGS__)

bool _sway_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, "[%s:%d] %s:" FMT, _wlr_strip_path(__FILE__), __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void error_handler(int sig);

#endif
