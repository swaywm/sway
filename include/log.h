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
void sway_log_func(log_importance_t verbosity, const char *func, const char* format, ...) __attribute__((format(printf,3,4)));
void sway_log_errno(log_importance_t verbosity, char* format, ...) __attribute__((format(printf,2,3)));
void sway_abort(const char* format, ...) __attribute__((format(printf,1,2)));
bool sway_assert_func(bool condition, const char *func, const char* format, ...) __attribute__((format(printf,3,4)));

#define sway_log(V, FMT, ...) \
	sway_log_func(V, __PRETTY_FUNCTION__, FMT, ##__VA_ARGS__)

#define sway_assert(COND, FMT, ...) \
	sway_assert_func(COND, __PRETTY_FUNCTION__, FMT, ##__VA_ARGS__)

void error_handler(int sig);

void layout_log(const swayc_t *c, int depth);
#endif
