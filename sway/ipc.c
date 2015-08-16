#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "log.h"
#include "config.h"
#include "commands.h"

static int ipc_socket = -1;

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

int ipc_handle_connection(int fd, uint32_t mask, void *data);
size_t ipc_handle_command(char **reply_data, char *data, ssize_t length);
size_t ipc_format_reply(char **data, enum ipc_command_type command_type, const char *payload, uint32_t payload_length);


void init_ipc() {
	ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_socket == -1) {
		sway_abort("Unable to create IPC socket");
	}

	struct sockaddr_un ipc_sockaddr = {
		.sun_family = AF_UNIX,
		// TODO: use a proper socket path
		.sun_path = "/tmp/sway.sock"
	};

	unlink(ipc_sockaddr.sun_path);
	if (bind(ipc_socket, (struct sockaddr *)&ipc_sockaddr, sizeof(ipc_sockaddr)) == -1) {
		sway_abort("Unable to bind IPC socket");
	}

	if (listen(ipc_socket, 3) == -1) {
		sway_abort("Unable to listen on IPC socket");
	}

	wlc_event_loop_add_fd(ipc_socket, WLC_EVENT_READABLE, ipc_handle_connection, NULL);
}

int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	sway_log(L_DEBUG, "Event on IPC listening socket");
	int client_socket = accept(ipc_socket, NULL, NULL);
	if (client_socket == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to accept IPC client connection: %s", error);
		return 0;
	}

	char buf[1024];
	// Leave one byte of space at the end of the buffer for NULL terminator
	ssize_t received = recv(client_socket, buf, sizeof(buf) - 1, 0);
	if (received == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to receive from IPC client: %s", error);
		close(client_socket);
		return 0;
	}

	char *reply_buf;
	size_t reply_length = ipc_handle_command(&reply_buf, buf, received);
	sway_log(L_DEBUG, "IPC reply: %s", reply_buf);

	if (send(client_socket, reply_buf, reply_length, 0) == -1) {
		char error[256];
		strerror_r(errno, error, sizeof(error));
		sway_log(L_INFO, "Unable to send to IPC client: %s", error);
	}

	free(reply_buf);
	close(client_socket);
	return 0;
}

static const int ipc_header_size = sizeof(ipc_magic)+8;

size_t ipc_handle_command(char **reply_data, char *data, ssize_t length) {
	// See https://i3wm.org/docs/ipc.html for protocol details

	if (length < ipc_header_size) {
		sway_log(L_DEBUG, "IPC data too short");
		return false;
	}

	if (memcmp(data, ipc_magic, sizeof(ipc_magic)) != 0) {
		sway_log(L_DEBUG, "IPC header check failed");
		return false;
	}

	uint32_t payload_length = *(uint32_t *)&data[sizeof(ipc_magic)];
	uint32_t command_type = *(uint32_t *)&data[sizeof(ipc_magic)+4];

	if (length != payload_length + ipc_header_size) {
		// TODO: try to read enough data
		sway_log(L_DEBUG, "IPC payload size mismatch");
		return false;
	}

	switch (command_type) {
		case IPC_COMMAND:
		{
			char *cmd = &data[ipc_header_size];
			data[ipc_header_size + payload_length] = '\0';
			bool success = handle_command(config, cmd);
			char buf[64];
			int length = snprintf(buf, sizeof(buf), "{\"success\":%s}", success ? "true" : "false");
			return ipc_format_reply(reply_data, IPC_COMMAND, buf, (uint32_t) length);
		}
		default:
			sway_log(L_INFO, "Unknown IPC command type %i", command_type);
			return false;
	}
}

size_t ipc_format_reply(char **data, enum ipc_command_type command_type, const char *payload, uint32_t payload_length) {
	assert(data);
	assert(payload);

	size_t length = ipc_header_size + payload_length;
	*data = malloc(length);

	memcpy(*data, ipc_magic, sizeof(ipc_magic));
	*(uint32_t *)&((*data)[sizeof(ipc_magic)]) = payload_length;
	*(uint32_t *)&((*data)[sizeof(ipc_magic)+4]) = command_type;
	memcpy(&(*data)[ipc_header_size], payload, payload_length);

	return length;
}
