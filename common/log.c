#define _POSIX_C_SOURCE 200112L
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
static struct timespec start_time = {-1, -1};

static const char *verbosity_colors[] = {
	[SWAY_SILENT] = "",
	[SWAY_ERROR ] = "\x1B[1;31m",
	[SWAY_INFO  ] = "\x1B[1;34m",
	[SWAY_DEBUG ] = "\x1B[1;90m",
};

static const char *verbosity_headers[] = {
	[SWAY_SILENT] = "",
	[SWAY_ERROR] = "[ERROR]",
	[SWAY_INFO] = "[INFO]",
	[SWAY_DEBUG] = "[DEBUG]",
};

static void timespec_sub(struct timespec *r, const struct timespec *a,
		const struct timespec *b) {
	const long NSEC_PER_SEC = 1000000000;
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

static void init_start_time(void) {
	if (start_time.tv_sec >= 0) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &start_time);
}

static void sway_log_stderr(sway_log_importance_t verbosity, const char *fmt,
		va_list args) {
	init_start_time();

	if (verbosity > log_importance) {
		return;
	}

	struct timespec ts = {0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	timespec_sub(&ts, &ts, &start_time);

	fprintf(stderr, "%02d:%02d:%02d.%03ld ", (int)(ts.tv_sec / 60 / 60),
		(int)(ts.tv_sec / 60 % 60), (int)(ts.tv_sec % 60),
		ts.tv_nsec / 1000000);

	unsigned c = (verbosity < SWAY_LOG_IMPORTANCE_LAST) ? verbosity :
		SWAY_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	} else {
		fprintf(stderr, "%s ", verbosity_headers[c]);
	}

	vfprintf(stderr, fmt, args);

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");
}

void sway_log_init(sway_log_importance_t verbosity, terminate_callback_t callback) {
	init_start_time();

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
