#define _POSIX_C_SOURCE 1
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "log.h"
#include "sway.h"
#include "readline.h"

static int colored = 1;
static log_importance_t loglevel_default = L_ERROR;
static log_importance_t v = L_SILENT;

static const char *verbosity_colors[] = {
	[L_SILENT] = "",
	[L_ERROR ] = "\x1B[1;31m",
	[L_INFO  ] = "\x1B[1;34m",
	[L_DEBUG ] = "\x1B[1;30m",
};

void init_log(log_importance_t verbosity) {
	if (verbosity != L_DEBUG) {
		// command "debuglog" needs to know the user specified log level when
		// turning off debug logging.
		loglevel_default = verbosity;
	}
	v = verbosity;
}

void set_log_level(log_importance_t verbosity) {
	v = verbosity;
}

log_importance_t get_log_level(void) {
	return v;
}

void reset_log_level(void) {
	v = loglevel_default;
}

bool toggle_debug_logging(void) {
	v = (v == L_DEBUG) ? loglevel_default : L_DEBUG;
	return (v == L_DEBUG);
}

void sway_log_colors(int mode) {
	colored = (mode == 1) ? 1 : 0;
}

void sway_abort(const char *format, ...) {
	fprintf(stderr, "ERROR: ");
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	sway_terminate(EXIT_FAILURE);
}

void _sway_log(const char *filename, int line, log_importance_t verbosity, const char* format, ...) {
	if (verbosity <= v) {
		// prefix the time to the log message
		static struct tm result;
		static time_t t;
		static struct tm *tm_info;
		char buffer[26];

		// get current time
		t = time(NULL);
		// convert time to local time (determined by the locale)
		tm_info = localtime_r(&t, &result);
		// generate time prefix
		strftime(buffer, sizeof(buffer), "%x %X - ", tm_info);
		fprintf(stderr, "%s", buffer);

		unsigned int c = verbosity;
		if (c > sizeof(verbosity_colors) / sizeof(char *) - 1) {
			c = sizeof(verbosity_colors) / sizeof(char *) - 1;
		}

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "%s", verbosity_colors[c]);
		}

		if (filename && line) {
			const char *file = filename + strlen(filename);
			while (file != filename && *file != '/') {
				--file;
			}
			if (*file == '/') {
				++file;
			}
			fprintf(stderr, "[%s:%d] ", file, line);
		}

		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "\x1B[0m");
		}
		fprintf(stderr, "\n");
	}
}

void sway_log_errno(log_importance_t verbosity, char* format, ...) {
	if (verbosity <= v) {
		unsigned int c = verbosity;
		if (c > sizeof(verbosity_colors) / sizeof(char *) - 1) {
			c = sizeof(verbosity_colors) / sizeof(char *) - 1;
		}

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "%s", verbosity_colors[c]);
		}

		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);

		fprintf(stderr, ": ");
		fprintf(stderr, "%s", strerror(errno));

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "\x1B[0m");
		}
		fprintf(stderr, "\n");
	}
}

bool _sway_assert(bool condition, const char* format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	sway_log(L_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}
