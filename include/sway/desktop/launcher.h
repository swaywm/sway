#ifndef _SWAY_LAUNCHER_H
#define _SWAY_LAUNCHER_H

#include <stdlib.h>

struct sway_workspace *root_workspace_for_pid(pid_t pid);

void root_record_workspace_pid(pid_t pid);

void root_remove_workspace_pid(pid_t pid);

#endif
