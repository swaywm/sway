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
void set_log_level(log_importance_t verbosity);
void reset_log_level(void);
// returns whether debug logging is on after switching.
bool toggle_debug_logging(void);
void sway_log_colors(int mode);
void sway_log(log_importance_t verbosity, const char* format, ...) __attribute__((format(printf,2,3)));
void sway_log_errno(log_importance_t verbosity, char* format, ...) __attribute__((format(printf,2,3)));
void sway_abort(const char* format, ...) __attribute__((format(printf,1,2)));

bool _sway_assert(bool condition, const char* format, ...) __attribute__((format(printf,2,3)));
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, "%s:" FMT, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void error_handler(int sig);

void layout_log(const swayc_t *c, int depth);
const char *swayc_type_string(enum swayc_types type);
void swayc_log(log_importance_t verbosity, swayc_t *cont, const char* format, ...) __attribute__((format(printf,3,4)));
#endif
