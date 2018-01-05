#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H
#include <stdbool.h>
#include <wlr/util/log.h>

void _sway_abort(const char *filename, int line, const char* format, ...) __attribute__((format(printf,3,4)));
#define sway_abort(FMT, ...) \
    _sway_abort(__FILE__, __LINE__, FMT, ##__VA_ARGS__)

bool _sway_assert(bool condition, const char *filename, int line, const char* format, ...) __attribute__((format(printf,4,5)));
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, __FILE__, __LINE__, "%s:" FMT, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void error_handler(int sig);

#endif
