#ifndef _SWAYBAR_IPC_H
#define _SWAYBAR_IPC_H
#include <stdbool.h>
#include "swaybar/bar.h"

bool ipc_initialize(struct swaybar *bar);
bool handle_ipc_readable(struct swaybar *bar);
bool ipc_get_workspaces(struct swaybar *bar);
void ipc_send_workspace_command(struct swaybar *bar, const char *ws);
void ipc_execute_binding(struct swaybar *bar, struct swaybar_binding *bind);

#endif
