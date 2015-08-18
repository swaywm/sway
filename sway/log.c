#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

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
	/* set FD_CLOEXEC flag to prevent programs called with exec to write into logs */
	int i;
	int fd[] = { STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO };
	for (i = 0; i < 3; ++i) {
		int flag = fcntl(fd[i], F_GETFD);
		if (flag != -1) {
			fcntl(fd[i], F_SETFD, flag | FD_CLOEXEC);
		}
	}
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
	exit(1);
}

void sway_log(int verbosity, const char* format, ...) {
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

void sway_log_errno(int verbosity, char* format, ...) {
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

		fprintf(stderr, ": ");
		char error[256];
		strerror_r(errno, error, sizeof(error));
		fprintf(stderr, error);

		if (colored) {
			fprintf(stderr, "\x1B[0m");
		}
		fprintf(stderr, "\n");
	}
}

bool sway_assert(bool condition, const char* format, ...) {
	if (condition) {
		return true;
	}

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	va_list args;
	va_start(args, format);
	sway_log(L_ERROR, format, args);
	va_end(args);

	return false;
}

#include "workspace.h"

/* XXX:DEBUG:XXX */
static void container_log(const swayc_t *c) {
	fprintf(stderr, "focus:%c|",
			c->is_focused ? 'F' : // Focused
			c == active_workspace ? 'W' : // active workspace
			c == &root_container  ? 'R' : // root
			'X');// not any others
	fprintf(stderr,"(%p)",c);
	fprintf(stderr,"(p:%p)",c->parent);
	fprintf(stderr,"(f:%p)",c->focused);
	fprintf(stderr,"(h:%ld)",c->handle);
	fprintf(stderr,"Type:");
	fprintf(stderr,
			c->type == C_ROOT   ? "Root|" :
			c->type == C_OUTPUT ? "Output|" :
			c->type == C_WORKSPACE ? "Workspace|" :
			c->type == C_CONTAINER ? "Container|" :
			c->type == C_VIEW   ? "View|" : "Unknown|");
	fprintf(stderr,"layout:");
	fprintf(stderr,
			c->layout == L_NONE ? "NONE|" :
			c->layout == L_HORIZ ? "Horiz|":
			c->layout == L_VERT ? "Vert|":
			c->layout == L_STACKED  ? "Stacked|":
			c->layout == L_FLOATING ? "Floating|":
			"Unknown|");
	fprintf(stderr, "w:%d|h:%d|", c->width, c->height);
	fprintf(stderr, "x:%d|y:%d|", c->x, c->y);
	fprintf(stderr, "vis:%c|", c->visible?'t':'f');
	fprintf(stderr, "name:%.16s|", c->name);
	fprintf(stderr, "children:%d\n",c->children?c->children->length:0);
}
void layout_log(const swayc_t *c, int depth) {
	int i, d;
	int e = c->children ? c->children->length : 0;
	container_log(c);
	if (e) {
		for (i = 0; i < e; ++i) {
			fputc('|',stderr);
			for (d = 0; d < depth; ++d) fputc('-', stderr);
			layout_log(c->children->items[i], depth + 1);
		}
	}
	if (c->type == C_WORKSPACE) {
		e = c->floating?c->floating->length:0;
		if (e) {
			for (i = 0; i < e; ++i) {
				fputc('|',stderr);
				for (d = 0; d < depth; ++d) fputc('=', stderr);
				layout_log(c->floating->items[i], depth + 1);
			}
		}
	}
}
/* XXX:DEBUG:XXX */
