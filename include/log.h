#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H
#include <stdbool.h>
#include "container.h"

typedef enum {
	L_SILENT = 0,
	L_ERROR = 1,
	L_INFO = 2,
	L_DEBUG = 3,
} log_importance_t;

void init_log(int verbosity);
void sway_log_colors(int mode);
void sway_log(int verbosity, const char* format, ...) __attribute__((format(printf,2,3)));
void sway_abort(const char* format, ...) __attribute__((format(printf,1,2)));
bool sway_assert(bool condition, const char* format, ...) __attribute__((format(printf,2,3)));

void layout_log(const swayc_t *c, int depth);
#endif
