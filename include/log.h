#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H
#include <stdbool.h>

typedef enum {
	L_SILENT = 0,
	L_ERROR = 1,
	L_INFO = 2,
	L_DEBUG = 3,
} log_importance_t;

void init_log(log_importance_t verbosity);
void set_log_level(log_importance_t verbosity);
log_importance_t get_log_level(void);
void reset_log_level(void);
// returns whether debug logging is on after switching.
bool toggle_debug_logging(void);
void sway_log_colors(int mode);
void sway_log_errno(log_importance_t verbosity, char* format, ...) __attribute__((format(printf,2,3)));
void sway_abort(const char* format, ...) __attribute__((format(printf,1,2)));

bool _sway_assert(bool condition, const char *filename, int line, const char* format, ...) __attribute__((format(printf,4,5)));
#define sway_assert(COND, FMT, ...) \
	_sway_assert(COND, __FILE__, __LINE__, "%s:" FMT, __PRETTY_FUNCTION__, ##__VA_ARGS__)

void _sway_log(const char *filename, int line, log_importance_t verbosity, const char* format, ...) __attribute__((format(printf,4,5)));

#define sway_log(VERBOSITY, FMT, ...) \
	_sway_log(__FILE__, __LINE__, VERBOSITY, FMT, ##__VA_ARGS__)

#define sway_vlog(VERBOSITY, FMT, VA_ARGS) \
    _sway_vlog(__FILE__, __LINE__, VERBOSITY, FMT, VA_ARGS)

void error_handler(int sig);

#endif
