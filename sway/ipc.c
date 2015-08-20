// See https://i3wm.org/docs/ipc.html for protocol information

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include "ipc.h"
#include "log.h"
#include "config.h"
#include "commands.h"
#include "list.h"
#include "stringop.h"

static int ipc_socket = -1;
static struct wlc_event_source *ipc_event_source =  NULL;
static struct sockaddr_un ipc_sockaddr = {
	.sun_family = AF_UNIX,
	.sun_path = "/tmp/sway-ipc.sock"
};

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

struct ipc_client {
	struct wlc_event_source *event_source;
	int fd;
	uint32_t payload_length;
	enum ipc_command_type current_command;
};

int ipc_handle_connection(int fd, uint32_t mask, void *data);
int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data);
void ipc_client_disconnect(struct ipc_client *client);
void ipc_client_handle_command(struct ipc_client *client);
bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length);
void ipc_get_workspaces_callback(swayc_t *container, void *data);
void ipc_get_outputs_callback(swayc_t *container, void *data);

char *json_list(list_t *items);

void ipc_init(void) {
	ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_socket == -1) {
		sway_abort("Unable to create IPC socket");
	}

	if (getenv("SWAYSOCK") != NULL) {
		strncpy(ipc_sockaddr.sun_path, getenv("SWAYSOCK"), sizeof(ipc_sockaddr.sun_path));
	}

	unlink(ipc_sockaddr.sun_path);
	if (bind(ipc_socket, (struct sockaddr *)&ipc_sockaddr, sizeof(ipc_sockaddr)) == -1) {
		sway_abort("Unable to bind IPC socket");
	}

	if (listen(ipc_socket, 3) == -1) {
		sway_abort("Unable to listen on IPC socket");
	}

	// Set i3 IPC socket path so that i3-msg works out of the box
	setenv("I3SOCK", ipc_sockaddr.sun_path, 1);

	ipc_event_source = wlc_event_loop_add_fd(ipc_socket, WLC_EVENT_READABLE, ipc_handle_connection, NULL);
}

void ipc_terminate(void) {
	if (ipc_event_source) {
		wlc_event_source_remove(ipc_event_source);
	}
	close(ipc_socket);
	unlink(ipc_sockaddr.sun_path);
}

int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	(void) fd; (void) data;
	sway_log(L_DEBUG, "Event on IPC listening socket");
	assert(mask == WLC_EVENT_READABLE);

	int client_fd = accept(ipc_socket, NULL, NULL);
	if (client_fd == -1) {
		sway_log_errno(L_INFO, "Unable to accept IPC client connection");
		return 0;
	}

	int flags;
	if ((flags=fcntl(client_fd, F_GETFD)) == -1 || fcntl(client_fd, F_SETFD, flags|FD_CLOEXEC) == -1) {
		sway_log_errno(L_INFO, "Unable to set CLOEXEC on IPC client socket");
		return 0;
	}

	struct ipc_client* client = malloc(sizeof(struct ipc_client));
	client->payload_length = 0;
	client->fd = client_fd;
	client->event_source = wlc_event_loop_add_fd(client_fd, WLC_EVENT_READABLE, ipc_client_handle_readable, client);

	return 0;
}

static const int ipc_header_size = sizeof(ipc_magic)+8;

int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct ipc_client *client = data;
	sway_log(L_DEBUG, "Event on IPC client socket %d", client_fd);

	if (mask & WLC_EVENT_ERROR) {
		sway_log(L_INFO, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WLC_EVENT_HANGUP) {
		ipc_client_disconnect(client);
		return 0;
	}

	int read_available;
	ioctl(client_fd, FIONREAD, &read_available);

	// Wait for the rest of the command payload in case the header has already been read
	if (client->payload_length > 0) {
		if (read_available >= client->payload_length) {
			ipc_client_handle_command(client);
		}
		else {
			sway_log(L_DEBUG, "Too little data to read payload on IPC Client socket, waiting for more (%d < %d)", read_available, client->payload_length);
		}
		return 0;
	}

	if (read_available < ipc_header_size) {
		sway_log(L_DEBUG, "Too little data to read header on IPC Client socket, waiting for more (%d < %d)", read_available, ipc_header_size);
		return 0;
	}

	char buf[ipc_header_size];
	ssize_t received = recv(client_fd, buf, ipc_header_size, 0);
	if (received == -1) {
		sway_log_errno(L_INFO, "Unable to receive header from IPC client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (memcmp(buf, ipc_magic, sizeof(ipc_magic)) != 0) {
		sway_log(L_DEBUG, "IPC header check failed");
		ipc_client_disconnect(client);
		return 0;
	}

	client->payload_length = *(uint32_t *)&buf[sizeof(ipc_magic)];
	client->current_command = (enum ipc_command_type) *(uint32_t *)&buf[sizeof(ipc_magic)+4];

	if (read_available - received >= client->payload_length) {
		ipc_client_handle_command(client);
	}

	return 0;
}

void ipc_client_disconnect(struct ipc_client *client)
{
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	sway_log(L_INFO, "IPC Client %d disconnected", client->fd);
	wlc_event_source_remove(client->event_source);
	close(client->fd);
	free(client);
}

void ipc_client_handle_command(struct ipc_client *client) {
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	char buf[client->payload_length + 1];
	if (client->payload_length > 0)
	{
		ssize_t received = recv(client->fd, buf, client->payload_length, 0);
		if (received == -1)
		{
			sway_log_errno(L_INFO, "Unable to receive payload from IPC client");
			ipc_client_disconnect(client);
			return;
		}
	}

	switch (client->current_command) {
	case IPC_COMMAND:
	{
		buf[client->payload_length] = '\0';
		bool success = handle_command(config, buf);
		char reply[64];
		int length = snprintf(reply, sizeof(reply), "{\"success\":%s}", success ? "true" : "false");
		ipc_send_reply(client, reply, (uint32_t) length);
		break;
	}
	case IPC_GET_WORKSPACES:
	{
		list_t *workspaces = create_list();
		container_map(&root_container, ipc_get_workspaces_callback, workspaces);
		char *json = json_list(workspaces);
		free_flat_list(workspaces);
		ipc_send_reply(client, json, strlen(json));
		free(json);
		break;
	}
	case IPC_GET_OUTPUTS:
	{
		list_t *outputs = create_list();
		container_map(&root_container, ipc_get_outputs_callback, outputs);
		char *json = json_list(outputs);
		free_flat_list(outputs);
		ipc_send_reply(client, json, strlen(json));
		free(json);
		break;
	}
	default:
		sway_log(L_INFO, "Unknown IPC command type %i", client->current_command);
		ipc_client_disconnect(client);
		break;
	}

	client->payload_length = 0;
}

bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length) {
	assert(payload);

	char data[ipc_header_size];

	memcpy(data, ipc_magic, sizeof(ipc_magic));
	*(uint32_t *)&(data[sizeof(ipc_magic)]) = payload_length;
	*(uint32_t *)&(data[sizeof(ipc_magic)+4]) = client->current_command;

	if (write(client->fd, data, ipc_header_size) == -1) {
		sway_log_errno(L_INFO, "Unable to send header to IPC client");
		ipc_client_disconnect(client);
		return false;
	}

	if (write(client->fd, payload, payload_length) == -1) {
		sway_log_errno(L_INFO, "Unable to send payload to IPC client");
		ipc_client_disconnect(client);
		return false;
	}

	return true;
}

char *json_list(list_t *items) {
	char *json_elements = join_list(items, ",");
	size_t len = strlen(json_elements);
	char *json = malloc(len + 3);
	json[0] = '[';
	memcpy(json + 1, json_elements, len);
	json[len+1] = ']';
	json[len+2] = '\0';
	free(json_elements);
	return json;
}

void ipc_get_workspaces_callback(swayc_t *container, void *data) {
	if (container->type == C_WORKSPACE) {
		char *json = malloc(512); // Output should usually be around 180 chars
		int num = isdigit(container->name[0]) ? atoi(container->name) : -1;
		// TODO: escape the name (quotation marks, unicode)
		sprintf(json,
			"{"
				"\"num\":%d,"
				"\"name\":\"%s\","
				"\"visible\":%s,"
				"\"focused\":%s,"
				"\"rect\":{"
					"\"x\":%d,"
					"\"y\":%d,"
					"\"width\":%d,"
					"\"height\":%d"
				"},"
				"\"output\":\"%s\","
				"\"urgent\":%s"
			"}",
			num, container->name, container->visible ? "true" : "false", container->is_focused ? "true" : "false",
			container->x, container->y, container->width, container->height,
			container->parent->name, "false" // TODO: urgent hint
		);
		list_add((list_t *)data, json);
	}
}

void ipc_get_outputs_callback(swayc_t *container, void *data) {
	if (container->type == C_OUTPUT) {
		char *json = malloc(512); // Output should usually be around 130 chars
		// TODO: escape the name (quotation marks, unicode)
		sprintf(json,
			"{"
				"\"name\":\"%s\","
				"\"active\":%s,"
				"\"primary\":%s,"
				"\"rect\":{"
					"\"x\":%d,"
					"\"y\":%d,"
					"\"width\":%d,"
					"\"height\":%d"
				"},"
				"\"current_workspace\":\"%s\""
			"}",
			container->name, "true", "false", // TODO: active, primary
			container->x, container->y, container->width, container->height,
			container->focused ? container->focused->name : ""
		);
		list_add((list_t *)data, json);
	}
}
