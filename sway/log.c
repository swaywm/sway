#include "log.h"
#include "sway.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <execinfo.h>

int colored = 1;
log_importance_t v = L_SILENT;

static const char *verbosity_colors[] = {
	"", // L_SILENT
	"\x1B[1;31m", // L_ERROR
	"\x1B[1;34m", // L_INFO
	"\x1B[1;30m", // L_DEBUG
};

void init_log(log_importance_t verbosity) {
	v = verbosity;
	signal(SIGSEGV, error_handler);
	signal(SIGABRT, error_handler);
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

void sway_log(log_importance_t verbosity, const char* format, ...) {
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
		char error[256];
		strerror_r(errno, error, sizeof(error));
		fprintf(stderr, "%s", error);

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
	int i;
	int max_lines = 20;
	void *array[max_lines];
	char **bt;
	size_t bt_len;

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
	exit(1);
}

#include "workspace.h"

/* XXX:DEBUG:XXX */
static void container_log(const swayc_t *c) {
	fprintf(stderr, "focus:%c|",
			c == get_focused_view(&root_container) ? 'K':
			c == get_focused_container(&root_container) ? 'F' : // Focused
			c == swayc_active_workspace() ? 'W' : // active workspace
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
	fprintf(stderr, "w:%f|h:%f|", c->width, c->height);
	fprintf(stderr, "x:%f|y:%f|", c->x, c->y);
	fprintf(stderr, "vis:%c|", c->visible?'t':'f');
	fprintf(stderr, "name:%.16s|", c->name);
	fprintf(stderr, "children:%d\n",c->children?c->children->length:0);
}
void layout_log(const swayc_t *c, int depth) {
	if (L_DEBUG > v) return;
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
