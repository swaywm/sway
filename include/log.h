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

void init_log(log_importance_t verbosity);
void sway_log_colors(int mode);
void sway_log(log_importance_t verbosity, const char* format, ...) __attribute__((format(printf,2,3)));
void sway_log_errno(log_importance_t verbosity, char* format, ...) __attribute__((format(printf,2,3)));
void sway_abort(const char* format, ...) __attribute__((format(printf,1,2)));

bool _sway_assert(bool condition, const char* format, ...) __attribute__((format(printf,2,3)));
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, "%s:" FMT, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void error_handler(int sig);

void layout_log(const swayc_t *c, int depth);
#endif
