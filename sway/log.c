#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int colored = 1;
int v = 0;

static const char *verbosity_colors[] = {
	"", // L_SILENT
	"\x1B[1;31m", // L_ERROR
	"\x1B[1;34m", // L_INFO
	"\x1B[1;30m", // L_DEBUG
};

void init_log(int verbosity) {
	v = verbosity;
	/* set FD_CLOEXEC flag to prevent programs called with exec to write into
	 * logs */
	int i, flag;
	int fd[] = { STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO };
	for (i = 0; i < 3; ++i) {
		flag = fcntl(fd[i], F_GETFD);
		if (flag != -1) {
			fcntl(fd[i], F_SETFD, flag | FD_CLOEXEC);
		}
	}
}

void sway_log_colors(int mode) {
	colored = (mode == 1) ? 1 : 0;
}

void sway_abort(char *format, ...) {
	fprintf(stderr, "ERROR: ");
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

void sway_log(int verbosity, char* format, ...) {
	if (verbosity <= v) {
		int c = verbosity;
		if (c > sizeof(verbosity_colors) / sizeof(char *)) {
			c = sizeof(verbosity_colors) / sizeof(char *) - 1;
		}

		if (colored) {
			fprintf(stderr, verbosity_colors[c]);
		}

		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);

		if (colored) {
			fprintf(stderr, "\x1B[0m");
		}
		fprintf(stderr, "\n");
	}
}
