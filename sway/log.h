#ifndef _SWAY_LOG_H
#define _SWAY_LOG_H

typedef enum {
    L_SILENT = 0,
    L_ERROR = 1,
    L_INFO = 2,
    L_DEBUG = 3,
} log_importance_t;

void init_log(int verbosity);
void sway_log(int verbosity, char* format, ...);
void sway_abort(char* format, ...);

#endif
