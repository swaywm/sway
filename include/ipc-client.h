#ifndef _SWAY_IPC_CLIENT_H
#define _SWAY_IPC_CLIENT_H

#include "ipc.h"

char *get_socketpath(void);
char *ipc_single_command(const char *socket_path, uint32_t type, const char *payload, uint32_t len);

#endif
