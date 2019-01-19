#ifndef _SWAY_IPC_CLIENT_H
#define _SWAY_IPC_CLIENT_H

#include <stdint.h>

#include "ipc.h"

/**
 * IPC response including type of IPC response, size of payload and the json
 * encoded payload string.
 */
struct ipc_response {
	uint32_t size;
	uint32_t type;
	char *payload;
};

/**
 * Gets the path to the IPC socket from sway.
 */
char *get_socketpath(void);
/**
 * Opens the sway socket.
 *
 * On failure, sets `*error` to a string constant, and returns -1;
 */
int ipc_open_socket(const char *socket_path, const char **error);
/**
 * Issues a single IPC command and returns the buffer. len will be updated with
 * the length of the buffer returned from sway.
 *
 * On failure, sets `*error` to a string constant, and returns NULL.
 */
char *ipc_single_command(int socketfd, uint32_t type, const char *payload,
	uint32_t *len, const char **error);
/**
 * Receives a single IPC response and returns an ipc_response.
 *
 * On failure, sets `*error` to a string constant, and returns NULL.
 */
struct ipc_response *ipc_recv_response(int socketfd, const char **error);
/**
 * Free ipc_response struct
 */
void free_ipc_response(struct ipc_response *response);

#endif
