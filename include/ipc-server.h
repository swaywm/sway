#ifndef _SWAY_IPC_SERVER_H
#define _SWAY_IPC_SERVER_H

#include "container.h"
#include "ipc.h"

void ipc_init(void);
void ipc_terminate(void);
struct sockaddr_un *ipc_user_sockaddr(void);

void ipc_event_workspace(swayc_t *old, swayc_t *new);

#endif
