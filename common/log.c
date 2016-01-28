#include "log.h"
#include "sway.h"
#include "readline.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stringop.h>

int colored = 1;
log_importance_t loglevel_default = L_ERROR;
log_importance_t v = L_SILENT;

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
	signal(SIGSEGV, error_handler);
	signal(SIGABRT, error_handler);
}

void set_log_level(log_importance_t verbosity) {
	v = verbosity;
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
	sway_terminate();
}

#ifndef NDEBUG
void _sway_log(const char *filename, int line, log_importance_t verbosity, const char* format, ...) {
#else
void _sway_log(log_importance_t verbosity, const char* format, ...) {
#endif
	if (verbosity <= v) {
		unsigned int c = verbosity;
		if (c > sizeof(verbosity_colors) / sizeof(char *)) {
			c = sizeof(verbosity_colors) / sizeof(char *) - 1;
		}

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "%s", verbosity_colors[c]);
		}

		va_list args;
		va_start(args, format);
#ifndef NDEBUG
		char *file = strdup(filename);
		fprintf(stderr, "[%s:%d] ", basename(file), line);
		free(file);
#endif
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
		if (c > sizeof(verbosity_colors) / sizeof(char *)) {
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

void error_handler(int sig) {
#if SWAY_Backtrace_FOUND
	int i;
	int max_lines = 20;
	void *array[max_lines];
	char **bt;
	size_t bt_len;
	char maps_file[256];
	char maps_buffer[1024];
	FILE *maps;

	sway_log(L_ERROR, "Error: Signal %d. Printing backtrace", sig);
	bt_len = backtrace(array, max_lines);
	bt = backtrace_symbols(array, bt_len);
	if (!bt) {
		sway_log(L_ERROR, "Could not allocate sufficient memory for backtrace_symbols(), falling back to stderr");
		backtrace_symbols_fd(array, bt_len, STDERR_FILENO);
		exit(1);
	}

	for (i = 0; (size_t)i < bt_len; i++) {
		sway_log(L_ERROR, "Backtrace: %s", bt[i]);
	}

	sway_log(L_ERROR, "Maps:");
	pid_t pid = getpid();
	if (snprintf(maps_file, 255, "/proc/%zd/maps", (size_t)pid) < 255) {
		maps = fopen(maps_file, "r");
		while (!feof(maps)) {
			char *m = read_line_buffer(maps, maps_buffer, 1024);
			if (!m) {
				fclose(maps);
				sway_log(L_ERROR, "Unable to allocate memory to show maps");
				break;
			}
			sway_log(L_ERROR, m);
		}
		fclose(maps);
	}
#else
	sway_log(L_ERROR, "Error: Signal %d.", sig);
#endif
	exit(1);
}
