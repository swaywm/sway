#ifndef _SWAY_LAUNCHER_H
#define _SWAY_LAUNCHER_H

#include <stdlib.h>

struct sway_workspace *workspace_for_pid(pid_t pid);

void launcher_ctx_create(pid_t pid);

void remove_workspace_pid(pid_t pid);

#endif
