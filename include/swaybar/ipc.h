#ifndef _SWAYBAR_IPC_H
#define _SWAYBAR_IPC_H
#include "swaybar/bar.h"

void ipc_bar_init(struct swaybar *bar, const char *bar_id);
bool handle_ipc_event(struct swaybar *bar);
void ipc_send_workspace_command(const char *workspace_name);

#endif
