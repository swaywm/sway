#include <stdio.h>
#include "log.h"


void sway_log_init(sway_log_importance_t verbosity, terminate_callback_t callback) {
	return;
}

void _sway_log(sway_log_importance_t verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
    vfprintf(stderr, fmt, args);
	va_end(args);
}

void _sway_vlog(sway_log_importance_t verbosity, const char *fmt, va_list args) {
	vfprintf(stderr, fmt, args);
}

bool _sway_assert(bool condition, const char *format, ...) {
	return true;
}

void _sway_abort(const char *format, ...) {
    return;
}