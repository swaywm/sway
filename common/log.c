#define _POSIX_C_SOURCE 199506L
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "log.h"

static terminate_callback_t log_terminate = exit;

void _sway_abort(const char *format, ...) {
	va_list args;
	va_start(args, format);
	_sway_vlog(SWAY_ERROR, format, args);
	va_end(args);
	log_terminate(EXIT_FAILURE);
}

bool _sway_assert(bool condition, const char *format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	_sway_vlog(SWAY_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}

static bool colored = true;
static sway_log_importance_t log_importance = SWAY_ERROR;

static const char *verbosity_colors[] = {
	[SWAY_SILENT] = "",
	[SWAY_ERROR ] = "\x1B[1;31m",
	[SWAY_INFO  ] = "\x1B[1;34m",
	[SWAY_DEBUG ] = "\x1B[1;30m",
};

static void sway_log_stderr(sway_log_importance_t verbosity, const char *fmt,
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

	unsigned c = (verbosity < SWAY_LOG_IMPORTANCE_LAST) ? verbosity :
		SWAY_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	}

	vfprintf(stderr, fmt, args);

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");
}

void sway_log_init(sway_log_importance_t verbosity, terminate_callback_t callback) {
	if (verbosity < SWAY_LOG_IMPORTANCE_LAST) {
		log_importance = verbosity;
	}
	if (callback) {
		log_terminate = callback;
	}
}

void _sway_vlog(sway_log_importance_t verbosity, const char *fmt, va_list args) {
	sway_log_stderr(verbosity, fmt, args);
}

void _sway_log(sway_log_importance_t verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	sway_log_stderr(verbosity, fmt, args);
	va_end(args);
}
