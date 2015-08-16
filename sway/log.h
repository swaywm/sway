#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H

#ifndef __GNUC__
#  define  __attribute__(x)
#endif

typedef enum {
	L_SILENT = 0,
	L_ERROR = 1,
	L_INFO = 2,
	L_DEBUG = 3,
} log_importance_t;


void init_log(int verbosity);
void sway_log_colors(int mode);
void sway_log(int verbosity, char* format, ...)__attribute__((format (printf,2,3)));
void sway_abort(char* format, ...) __attribute__((format (printf,1,2)));

#endif
