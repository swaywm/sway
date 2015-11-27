#ifndef _SWAY_IPC_CLIENT_H
#define _SWAY_IPC_CLIENT_H

#include "ipc.h"

/**
 * Gets the path to the IPC socket from sway.
 */
char *get_socketpath(void);
/**
 * Opens the sway socket.
 */
int ipc_open_socket(const char *socket_path);
/**
 * Issues a single IPC command and returns the buffer. len will be updated with
 * the length of the buffer returned from sway.
 */
char *ipc_single_command(int socketfd, uint32_t type, const char *payload, uint32_t *len);

#endif
