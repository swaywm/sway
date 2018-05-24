#define _POSIX_C_SOURCE 199506L
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "log.h"

void sway_terminate(int code);



void _sway_abort(const char *format, ...) {
	va_list args;
	va_start(args, format);
	_sway_vlog(L_ERROR, format, args);
	va_end(args);
	sway_terminate(EXIT_FAILURE);
}

bool _sway_assert(bool condition, const char *format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	_sway_vlog(L_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}

static bool colored = true;
static log_importance_t log_importance = L_ERROR;

static const char *verbosity_colors[] = {
	[L_SILENT] = "",
	[L_ERROR ] = "\x1B[1;31m",
	[L_INFO  ] = "\x1B[1;34m",
	[L_DEBUG ] = "\x1B[1;30m",
};

static void log_stderr(log_importance_t verbosity, const char *fmt,
		va_list args) {
	if (verbosity > log_importance) {
		return;
	}
	// prefix the time to the log message
	struct tm result;
	time_t t = time(NULL);
	struct tm *tm_info = localtime_r(&t, &result);
	char buffer[26];

	// generate time prefix
	strftime(buffer, sizeof(buffer), "%F %T - ", tm_info);
	fprintf(stderr, "%s", buffer);

	unsigned c = (verbosity < L_LAST) ? verbosity : L_LAST - 1;

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	}

	vfprintf(stderr, fmt, args);

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");
}

static log_callback_t log_callback = log_stderr;

void sway_log_init(log_importance_t verbosity, log_callback_t callback) {
	if (verbosity < L_LAST) {
		log_importance = verbosity;
	}
	if (callback) {
		log_callback = callback;
	}
}

void _sway_vlog(log_importance_t verbosity, const char *fmt, va_list args) {
	log_callback(verbosity, fmt, args);
}

void _sway_log(log_importance_t verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_callback(verbosity, fmt, args);
	va_end(args);
}

// strips the path prefix from filepath
// will try to strip SWAY_SRC_DIR as well as a relative src dir
// e.g. '/src/build/sway/util/log.c' and
// '../util/log.c' will both be stripped to
// 'util/log.c'
const char *sway_strip_path(const char *filepath) {
	static int srclen = sizeof(SWAY_SRC_DIR);
	if (strstr(filepath, SWAY_SRC_DIR) == filepath) {
		filepath += srclen;
	} else if (*filepath == '.') {
		while (*filepath == '.' || *filepath == '/') {
			++filepath;
		}
	}
	return filepath;
}
