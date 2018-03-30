#ifndef _SWAY_IPC_SERVER_H
#define _SWAY_IPC_SERVER_H
#include <sys/socket.h>
#include "sway/container.h"
#include "ipc.h"

struct sway_server;

void ipc_init(struct sway_server *server);
void ipc_terminate(void);
struct sockaddr_un *ipc_user_sockaddr(void);

void ipc_event_workspace(swayc_t *old, swayc_t *new, const char *change);
void ipc_event_window(swayc_t *window, const char *change);
void ipc_event_barconfig_update(struct bar_config *bar);
void ipc_event_mode(const char *mode);

#endif
