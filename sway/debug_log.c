#include "log.h"
#include "sway.h"
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
#include "sway/workspace.h"

/* XXX:DEBUG:XXX */
static void container_log(const swayc_t *c, int depth) {
	fprintf(stderr, "focus:%c",
			c == get_focused_view(&root_container) ? 'K':
			c == get_focused_container(&root_container) ? 'F' : // Focused
			c == swayc_active_workspace() ? 'W' : // active workspace
			c == &root_container  ? 'R' : // root
			'X');// not any others
	for (int i = 6; i > depth; i--) { fprintf(stderr, " "); }
	fprintf(stderr,"|(%p)",c);
	fprintf(stderr,"(p:%-8p)",c->parent);
	fprintf(stderr,"(f:%-8p)",c->focused);
	fprintf(stderr,"(h:%2" PRIuPTR ")",c->handle);
	fprintf(stderr,"Type:%-4s|",
			c->type == C_ROOT   ? "root" :
			c->type == C_OUTPUT ? "op" :
			c->type == C_WORKSPACE ? "ws" :
			c->type == C_CONTAINER ? "cont" :
			c->type == C_VIEW   ? "view" : "?");
	fprintf(stderr,"layout:%-5s|",
			c->layout == L_NONE ? "-" :
			c->layout == L_HORIZ ? "Horiz":
			c->layout == L_VERT ? "Vert":
			c->layout == L_STACKED  ? "Stack":
			c->layout == L_TABBED  ? "Tab":
			c->layout == L_FLOATING ? "Float":
			c->layout == L_AUTO_LEFT ? "A_lft":
			c->layout == L_AUTO_RIGHT ? "A_rgt":
			c->layout == L_AUTO_TOP ? "A_top":
			c->layout == L_AUTO_BOTTOM ? "A_bot":
			"Unknown");
	fprintf(stderr, "w:%4.f|h:%4.f|", c->width, c->height);
	fprintf(stderr, "x:%4.f|y:%4.f|", c->x, c->y);
	fprintf(stderr, "g:%3d|",c->gaps);
	fprintf(stderr, "vis:%c|", c->visible?'t':'f');
	fprintf(stderr, "children:%2zu|",c->children?c->children->length:0);
	fprintf(stderr, "name:%.16s\n", c->name);
}
void layout_log(const swayc_t *c, int depth) {
	if (L_DEBUG > get_log_level()) return;
	int i, d;
	int e = c->children ? c->children->length : 0;
	container_log(c, depth);
	if (e) {
		for (i = 0; i < e; ++i) {
			fputc('|',stderr);
			for (d = 0; d < depth; ++d) fputc('-', stderr);
			layout_log(*(swayc_t **)list_get(c->children, i), depth + 1);
		}
	}
	if (c->type == C_WORKSPACE) {
		e = c->floating?c->floating->length:0;
		if (e) {
			for (i = 0; i < e; ++i) {
				fputc('|',stderr);
				for (d = 0; d < depth; ++d) fputc('=', stderr);
				layout_log(*(swayc_t **)list_get(c->floating, i), depth + 1);
			}
		}
	}
}

const char *swayc_type_string(enum swayc_types type) {
	return type == C_ROOT ? "ROOT" :
		type == C_OUTPUT ? "OUTPUT" :
		type == C_WORKSPACE ? "WORKSPACE" :
		type == C_CONTAINER ? "CONTAINER" :
		type == C_VIEW   ? "VIEW" :
		"UNKNOWN";
}

// Like sway_log, but also appends some info about given container to log output.
void swayc_log(log_importance_t verbosity, swayc_t *cont, const char* format, ...) {
	sway_assert(cont, "swayc_log: no container ...");
	va_list args;
	va_start(args, format);
	char *txt = malloc(128);
	vsprintf(txt, format, args);
	va_end(args);

	char *debug_txt = malloc(32);
	snprintf(debug_txt, 32, "%s '%s'", swayc_type_string(cont->type), cont->name);

	sway_log(verbosity, "%s (%s)", txt, debug_txt);
	free(txt);
	free(debug_txt);
}

/* XXX:DEBUG:XXX */
