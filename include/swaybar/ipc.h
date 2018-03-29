#ifndef _SWAYBAR_IPC_H
#define _SWAYBAR_IPC_H
#include "swaybar/bar.h"

void ipc_get_config(struct swaybar *bar, const char *bar_id);
void handle_ipc_event(struct swaybar *bar);

#endif
