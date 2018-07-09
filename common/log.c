#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include "log.h"

void sway_terminate(int code);

void _sway_abort(const char *format, ...) {
	va_list args;
	va_start(args, format);
	_wlr_vlog(WLR_ERROR, format, args);
	va_end(args);
	sway_terminate(EXIT_FAILURE);
}

bool _sway_assert(bool condition, const char *format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	_wlr_vlog(WLR_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}
