#ifndef _SWAY_IPC_SERVER_H
#define _SWAY_IPC_SERVER_H

#include "container.h"
#include "config.h"
#include "ipc.h"

void ipc_init(void);
void ipc_terminate(void);
struct sockaddr_un *ipc_user_sockaddr(void);

void ipc_event_workspace(swayc_t *old, swayc_t *new);
void ipc_event_barconfig_update(struct bar_config *bar);
const char *swayc_type_string(enum swayc_types type);

#endif
