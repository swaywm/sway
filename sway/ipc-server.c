// See https://i3wm.org/docs/ipc.html for protocol information
#define _POSIX_C_SOURCE 200112L
#include <linux/input-event-codes.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-server.h>
#include "sway/config.h"
#include "sway/ipc-json.h"
#include "sway/ipc-server.h"
#include "sway/ipc-sway.h"
#include "sway/server.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "util.h"

struct ipc {
	int socket;
	struct wl_event_source *event_source;
	struct sockaddr_un *sockaddr;
};

static struct ipc sway_ipc = { .socket = -1 };
static struct ipc i3_ipc = { .socket = -1 };

static list_t *ipc_client_list = NULL;
static struct wl_listener ipc_display_destroy;

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

#define IPC_HEADER_SIZE (sizeof(ipc_magic) + 8)

int ipc_handle_connection(int fd, uint32_t mask, void *data);
int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data);
int ipc_client_handle_writable(int client_fd, uint32_t mask, void *data);
void ipc_client_disconnect(struct ipc_client *client);
void ipc_client_handle_command(struct ipc_client *client, uint32_t payload_length,
	enum ipc_command_type payload_type);

static void ipc_destroy(struct ipc *ipc) {
	if (ipc->event_source) {
		wl_event_source_remove(ipc->event_source);
	}
	close(ipc->socket);
	unlink(ipc->sockaddr->sun_path);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	ipc_destroy(&sway_ipc);
	ipc_destroy(&i3_ipc);

	while (ipc_client_list->length) {
		ipc_client_disconnect(ipc_client_list->items[ipc_client_list->length-1]);
	}
	list_free(ipc_client_list);

	free(sway_ipc.sockaddr);
	free(i3_ipc.sockaddr);

	wl_list_remove(&ipc_display_destroy.link);
}

static void ipc_init_socket(struct sway_server *server, struct ipc *ipc,
		const char *envvar, const char *sockpath_suffix) {

	ipc->sockaddr = ipc_user_sockaddr(sockpath_suffix);
	ipc->socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc->socket == -1) {
		sway_abort("Unable to create IPC socket");
	}
	if (fcntl(ipc->socket, F_SETFD, FD_CLOEXEC) == -1) {
		sway_abort("Unable to set CLOEXEC on IPC socket");
	}
	if (fcntl(ipc->socket, F_SETFL, O_NONBLOCK) == -1) {
		sway_abort("Unable to set NONBLOCK on IPC socket");
	}

	// We want to use socket name set by user, not existing socket from another sway instance.
	if (getenv(envvar) != NULL && access(getenv(envvar), F_OK) == -1) {
		strncpy(ipc->sockaddr->sun_path, getenv(envvar), sizeof(ipc->sockaddr->sun_path) - 1);
		ipc->sockaddr->sun_path[sizeof(ipc->sockaddr->sun_path) - 1] = 0;
	}

	unlink(ipc->sockaddr->sun_path);
	if (bind(ipc->socket, (struct sockaddr *)ipc->sockaddr, sizeof(*ipc->sockaddr)) == -1) {
		sway_abort("Unable to bind IPC socket");
	}

	if (listen(ipc->socket, 3) == -1) {
		sway_abort("Unable to listen on IPC socket");
	}

	setenv(envvar, ipc->sockaddr->sun_path, 1);

	ipc->event_source = wl_event_loop_add_fd(server->wl_event_loop, ipc->socket,
			WL_EVENT_READABLE, ipc_handle_connection, server);
}

void ipc_init(struct sway_server *server) {
	ipc_client_list = create_list();

	ipc_display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display, &ipc_display_destroy);

	ipc_init_socket(server, &sway_ipc, "SWAYSOCK", "");
	// Set i3 IPC socket path so that i3-msg works out of the box
	ipc_init_socket(server, &i3_ipc, "I3SOCK", ".i3");
}

struct sockaddr_un *ipc_user_sockaddr(const char *suffix) {
	struct sockaddr_un *ipc_sockaddr = malloc(sizeof(struct sockaddr_un));
	if (ipc_sockaddr == NULL) {
		sway_abort("Can't allocate ipc_sockaddr");
	}

	ipc_sockaddr->sun_family = AF_UNIX;
	int path_size = sizeof(ipc_sockaddr->sun_path);

	// Env var typically set by logind, e.g. "/run/user/<user-id>"
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (!dir) {
		dir = "/tmp";
	}
	if (path_size <= snprintf(ipc_sockaddr->sun_path, path_size,
			"%s/sway-ipc%s.%i.%i.sock", dir, suffix, getuid(), getpid())) {
		sway_abort("Socket path won't fit into ipc_sockaddr->sun_path");
	}

	return ipc_sockaddr;
}

int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	struct sway_server *server = data;
	sway_log(SWAY_DEBUG, "Event on IPC listening socket");
	assert(mask == WL_EVENT_READABLE);

	int client_fd = accept(fd, NULL, NULL);
	if (client_fd == -1) {
		sway_log_errno(SWAY_ERROR, "Unable to accept IPC client connection");
		return 0;
	}

	int flags;
	if ((flags = fcntl(client_fd, F_GETFD)) == -1
			|| fcntl(client_fd, F_SETFD, flags|FD_CLOEXEC) == -1) {
		sway_log_errno(SWAY_ERROR, "Unable to set CLOEXEC on IPC client socket");
		close(client_fd);
		return 0;
	}
	if ((flags = fcntl(client_fd, F_GETFL)) == -1
			|| fcntl(client_fd, F_SETFL, flags|O_NONBLOCK) == -1) {
		sway_log_errno(SWAY_ERROR, "Unable to set NONBLOCK on IPC client socket");
		close(client_fd);
		return 0;
	}

	struct ipc_client *client = malloc(sizeof(struct ipc_client));
	if (!client) {
		sway_log(SWAY_ERROR, "Unable to allocate ipc client");
		close(client_fd);
		return 0;
	}
	client->server = server;
	client->pending_length = 0;
	client->fd = client_fd;
	client->subscribed_events = 0;
	client->event_source = wl_event_loop_add_fd(server->wl_event_loop,
			client_fd, WL_EVENT_READABLE, ipc_client_handle_readable, client);
	client->writable_event_source = NULL;

	client->write_buffer_size = 128;
	client->write_buffer_len = 0;
	client->write_buffer = malloc(client->write_buffer_size);
	if (!client->write_buffer) {
		sway_log(SWAY_ERROR, "Unable to allocate ipc client write buffer");
		close(client_fd);
		return 0;
	}

	if (fd == sway_ipc.socket) {
		client->impl = &ipc_client_sway;
	} else if (fd == i3_ipc.socket) {
		client->impl = &ipc_client_i3;
	} else {
		sway_log(SWAY_ERROR, "Got connection event from unknown source");
		close(client_fd);
		return 0;
	}

	sway_log(SWAY_DEBUG, "New client: fd %d", client_fd);
	list_add(ipc_client_list, client);
	return 0;
}

int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct ipc_client *client = data;

	if (mask & WL_EVENT_ERROR) {
		sway_log(SWAY_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WL_EVENT_HANGUP) {
		sway_log(SWAY_DEBUG, "Client %d hung up", client->fd);
		ipc_client_disconnect(client);
		return 0;
	}

	sway_log(SWAY_DEBUG, "Client %d readable", client->fd);

	int read_available;
	if (ioctl(client_fd, FIONREAD, &read_available) == -1) {
		sway_log_errno(SWAY_INFO, "Unable to read IPC socket buffer size");
		ipc_client_disconnect(client);
		return 0;
	}

	// Wait for the rest of the command payload in case the header has already been read
	if (client->pending_length > 0) {
		if ((uint32_t)read_available >= client->pending_length) {
			// Reset pending values.
			uint32_t pending_length = client->pending_length;
			enum ipc_command_type pending_type = client->pending_type;
			client->pending_length = 0;
			ipc_client_handle_command(client, pending_length, pending_type);
		}
		return 0;
	}

	if (read_available < (int) IPC_HEADER_SIZE) {
		return 0;
	}

	uint8_t buf[IPC_HEADER_SIZE];
	uint32_t *buf32 = (uint32_t*)(buf + sizeof(ipc_magic));
	// Should be fully available, because read_available >= IPC_HEADER_SIZE
	ssize_t received = recv(client_fd, buf, IPC_HEADER_SIZE, 0);
	if (received == -1) {
		sway_log_errno(SWAY_INFO, "Unable to receive header from IPC client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (memcmp(buf, ipc_magic, sizeof(ipc_magic)) != 0) {
		sway_log(SWAY_DEBUG, "IPC header check failed");
		ipc_client_disconnect(client);
		return 0;
	}

	memcpy(&client->pending_length, &buf32[0], sizeof(buf32[0]));
	memcpy(&client->pending_type, &buf32[1], sizeof(buf32[1]));

	if (read_available - received >= (long)client->pending_length) {
		// Reset pending values.
		uint32_t pending_length = client->pending_length;
		enum ipc_command_type pending_type = client->pending_type;
		client->pending_length = 0;
		ipc_client_handle_command(client, pending_length, pending_type);
	}

	return 0;
}

static bool ipc_has_event_listeners(enum ipc_command_type event) {
	for (int i = 0; i < ipc_client_list->length; i++) {
		struct ipc_client *client = ipc_client_list->items[i];
		if ((client->subscribed_events & event_mask(event)) != 0) {
			return true;
		}
	}
	return false;
}

static void ipc_send_event(const char *json_string, enum ipc_command_type event) {
	struct ipc_client *client;
	for (int i = 0; i < ipc_client_list->length; i++) {
		client = ipc_client_list->items[i];
		if ((client->subscribed_events & event_mask(event)) == 0) {
			continue;
		}
		if (!ipc_send_reply(client, event, json_string,
				(uint32_t)strlen(json_string))) {
			sway_log_errno(SWAY_INFO, "Unable to send reply to IPC client");
			/* ipc_send_reply destroys client on error, which also
			 * removes it from the list, so we need to process
			 * current index again */
			i--;
		}
	}
}

void ipc_event_workspace(struct sway_workspace *old,
		struct sway_workspace *new, const char *change) {
	if (!ipc_has_event_listeners(IPC_EVENT_WORKSPACE)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending workspace::%s event", change);
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(change));
	if (old) {
		json_object_object_add(obj, "old",
				ipc_json_describe_node_recursive(&old->node, &ipc_json_sway_descriptors));
	} else {
		json_object_object_add(obj, "old", NULL);
	}

	if (new) {
		json_object_object_add(obj, "current",
				ipc_json_describe_node_recursive(&new->node, &ipc_json_sway_descriptors));
	} else {
		json_object_object_add(obj, "current", NULL);
	}

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_WORKSPACE);
	json_object_put(obj);
}

void ipc_event_window(struct sway_container *window, const char *change) {
	if (!ipc_has_event_listeners(IPC_EVENT_WINDOW)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending window::%s event", change);
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(change));
	json_object_object_add(obj, "container",
			ipc_json_describe_node_recursive(&window->node, &ipc_json_sway_descriptors));

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_WINDOW);
	json_object_put(obj);
}

void ipc_event_barconfig_update(struct bar_config *bar) {
	if (!ipc_has_event_listeners(IPC_EVENT_BARCONFIG_UPDATE)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending barconfig_update event");
	json_object *json = ipc_json_describe_bar_config(bar);

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BARCONFIG_UPDATE);
	json_object_put(json);
}

void ipc_event_bar_state_update(struct bar_config *bar) {
	if (!ipc_has_event_listeners(IPC_EVENT_BAR_STATE_UPDATE)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending bar_state_update event");

	json_object *json = json_object_new_object();
	json_object_object_add(json, "id", json_object_new_string(bar->id));
	json_object_object_add(json, "visible_by_modifier",
			json_object_new_boolean(bar->visible_by_modifier));

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BAR_STATE_UPDATE);
	json_object_put(json);
}

void ipc_event_mode(const char *mode, bool pango) {
	if (!ipc_has_event_listeners(IPC_EVENT_MODE)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending mode::%s event", mode);
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(mode));
	json_object_object_add(obj, "pango_markup",
			json_object_new_boolean(pango));

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_MODE);
	json_object_put(obj);
}

void ipc_event_shutdown(const char *reason) {
	if (!ipc_has_event_listeners(IPC_EVENT_SHUTDOWN)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending shutdown::%s event", reason);

	json_object *json = json_object_new_object();
	json_object_object_add(json, "change", json_object_new_string(reason));

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_SHUTDOWN);
	json_object_put(json);
}

void ipc_event_binding(struct sway_binding *binding) {
	if (!ipc_has_event_listeners(IPC_EVENT_BINDING)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending binding event");

	json_object *json_binding = json_object_new_object();
	json_object_object_add(json_binding, "command", json_object_new_string(binding->command));

	const char *names[10];
	int len = get_modifier_names(names, binding->modifiers);
	json_object *modifiers = json_object_new_array();
	for (int i = 0; i < len; ++i) {
		json_object_array_add(modifiers, json_object_new_string(names[i]));
	}
	json_object_object_add(json_binding, "event_state_mask", modifiers);

	json_object *input_codes = json_object_new_array();
	int input_code = 0;
	json_object *symbols = json_object_new_array();
	json_object *symbol = NULL;

	if (binding->type == BINDING_KEYCODE) { // bindcode: populate input_codes
		uint32_t keycode;
		for (int i = 0; i < binding->keys->length; ++i) {
			keycode = *(uint32_t *)binding->keys->items[i];
			json_object_array_add(input_codes, json_object_new_int(keycode));
			if (i == 0) {
				input_code = keycode;
			}
		}
	} else { // bindsym/mouse: populate symbols
		uint32_t keysym;
		char buffer[64];
		for (int i = 0; i < binding->keys->length; ++i) {
			keysym = *(uint32_t *)binding->keys->items[i];
			if (keysym >= BTN_LEFT && keysym <= BTN_LEFT + 8) {
				snprintf(buffer, 64, "button%u", keysym - BTN_LEFT + 1);
			} else if (xkb_keysym_get_name(keysym, buffer, 64) < 0) {
				continue;
			}

			json_object *str = json_object_new_string(buffer);
			if (i == 0) {
				// str is owned by both symbol and symbols. Make sure
				// to bump the ref count.
				json_object_array_add(symbols, json_object_get(str));
				symbol = str;
			} else {
				json_object_array_add(symbols, str);
			}
		}
	}

	json_object_object_add(json_binding, "input_codes", input_codes);
	json_object_object_add(json_binding, "input_code", json_object_new_int(input_code));
	json_object_object_add(json_binding, "symbols", symbols);
	json_object_object_add(json_binding, "symbol", symbol);

	bool mouse = binding->type == BINDING_MOUSECODE ||
		binding->type == BINDING_MOUSESYM;
	json_object_object_add(json_binding, "input_type", mouse
			? json_object_new_string("mouse")
			: json_object_new_string("keyboard"));

	json_object *json = json_object_new_object();
	json_object_object_add(json, "change", json_object_new_string("run"));
	json_object_object_add(json, "binding", json_binding);
	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BINDING);
	json_object_put(json);
}

void ipc_event_tick(const char *payload) {
	if (!ipc_has_event_listeners(IPC_EVENT_TICK)) {
		return;
	}
	sway_log(SWAY_DEBUG, "Sending tick event");

	json_object *json = json_object_new_object();
	json_object_object_add(json, "first", json_object_new_boolean(false));
	json_object_object_add(json, "payload", json_object_new_string(payload));

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_TICK);
	json_object_put(json);
}

int ipc_client_handle_writable(int client_fd, uint32_t mask, void *data) {
	struct ipc_client *client = data;

	if (mask & WL_EVENT_ERROR) {
		sway_log(SWAY_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WL_EVENT_HANGUP) {
		sway_log(SWAY_DEBUG, "Client %d hung up", client->fd);
		ipc_client_disconnect(client);
		return 0;
	}

	if (client->write_buffer_len <= 0) {
		return 0;
	}

	sway_log(SWAY_DEBUG, "Client %d writable", client->fd);

	ssize_t written = write(client->fd, client->write_buffer, client->write_buffer_len);

	if (written == -1 && errno == EAGAIN) {
		return 0;
	} else if (written == -1) {
		sway_log_errno(SWAY_INFO, "Unable to send data from queue to IPC client");
		ipc_client_disconnect(client);
		return 0;
	}

	memmove(client->write_buffer, client->write_buffer + written, client->write_buffer_len - written);
	client->write_buffer_len -= written;

	if (client->write_buffer_len == 0 && client->writable_event_source) {
		wl_event_source_remove(client->writable_event_source);
		client->writable_event_source = NULL;
	}

	return 0;
}

void ipc_client_disconnect(struct ipc_client *client) {
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	shutdown(client->fd, SHUT_RDWR);

	sway_log(SWAY_INFO, "IPC Client %d disconnected", client->fd);
	wl_event_source_remove(client->event_source);
	if (client->writable_event_source) {
		wl_event_source_remove(client->writable_event_source);
	}
	int i = 0;
	while (i < ipc_client_list->length && ipc_client_list->items[i] != client) {
		i++;
	}
	list_del(ipc_client_list, i);
	free(client->write_buffer);
	close(client->fd);
	free(client);
}

void ipc_client_handle_command(struct ipc_client *client, uint32_t payload_length,
		enum ipc_command_type payload_type) {
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	char *buf = malloc(payload_length + 1);
	if (!buf) {
		sway_log_errno(SWAY_INFO, "Unable to allocate IPC payload");
		ipc_client_disconnect(client);
		return;
	}
	if (payload_length > 0) {
		// Payload should be fully available
		ssize_t received = recv(client->fd, buf, payload_length, 0);
		if (received == -1)
		{
			sway_log_errno(SWAY_INFO, "Unable to receive payload from IPC client");
			ipc_client_disconnect(client);
			free(buf);
			return;
		}
	}
	buf[payload_length] = '\0';

	ipc_handler handler = NULL;
	if (payload_type >= 0 && (size_t) payload_type < client->impl->num_commands) {
		handler = client->impl->commands[payload_type];
	}

	if (handler) {
		json_object *json = handler(client, &payload_type, buf);
		if (json) {
			const char *json_string = json_object_to_json_string(json);
			ipc_send_reply(client, payload_type, json_string,
				(uint32_t)strlen(json_string));
			json_object_put(json);
		}
	} else {
		sway_log(SWAY_INFO, "Unknown IPC command type %x", payload_type);
	}

	free(buf);
	return;
}

bool ipc_send_reply(struct ipc_client *client, enum ipc_command_type payload_type,
		const char *payload, uint32_t payload_length) {
	assert(payload);

	char data[IPC_HEADER_SIZE];
	uint32_t *data32 = (uint32_t*)(data + sizeof(ipc_magic));

	memcpy(data, ipc_magic, sizeof(ipc_magic));
	memcpy(&data32[0], &payload_length, sizeof(payload_length));
	memcpy(&data32[1], &payload_type, sizeof(payload_type));

	while (client->write_buffer_len + IPC_HEADER_SIZE + payload_length >=
				 client->write_buffer_size) {
		client->write_buffer_size *= 2;
	}

	if (client->write_buffer_size > 4e6) { // 4 MB
		sway_log(SWAY_ERROR, "Client write buffer too big, disconnecting client");
		ipc_client_disconnect(client);
		return false;
	}

	char *new_buffer = realloc(client->write_buffer, client->write_buffer_size);
	if (!new_buffer) {
		sway_log(SWAY_ERROR, "Unable to reallocate ipc client write buffer");
		ipc_client_disconnect(client);
		return false;
	}
	client->write_buffer = new_buffer;

	memcpy(client->write_buffer + client->write_buffer_len, data, IPC_HEADER_SIZE);
	client->write_buffer_len += IPC_HEADER_SIZE;
	memcpy(client->write_buffer + client->write_buffer_len, payload, payload_length);
	client->write_buffer_len += payload_length;

	if (!client->writable_event_source) {
		client->writable_event_source = wl_event_loop_add_fd(
				server.wl_event_loop, client->fd, WL_EVENT_WRITABLE,
				ipc_client_handle_writable, client);
	}

	sway_log(SWAY_DEBUG, "Added IPC reply of type 0x%x to client %d queue: %s",
		payload_type, client->fd, payload);
	return true;
}
