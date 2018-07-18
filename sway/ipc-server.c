// See https://i3wm.org/docs/ipc.html for protocol information
#ifndef __FreeBSD__
// Any value will hide SOCK_CLOEXEC on FreeBSD (__BSD_VISIBLE=0)
#define _XOPEN_SOURCE 700
#endif
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
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
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/ipc-json.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"
#include "util.h"

static int ipc_socket = -1;
static struct wl_event_source *ipc_event_source =  NULL;
static struct sockaddr_un *ipc_sockaddr = NULL;
static list_t *ipc_client_list = NULL;
static struct wl_listener ipc_display_destroy;

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

struct ipc_client {
	struct wl_event_source *event_source;
	struct wl_event_source *writable_event_source;
	struct sway_server *server;
	int fd;
	uint32_t payload_length;
	uint32_t security_policy;
	enum ipc_command_type current_command;
	enum ipc_command_type subscribed_events;
	size_t write_buffer_len;
	size_t write_buffer_size;
	char *write_buffer;
};

struct sockaddr_un *ipc_user_sockaddr(void);
int ipc_handle_connection(int fd, uint32_t mask, void *data);
int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data);
int ipc_client_handle_writable(int client_fd, uint32_t mask, void *data);
void ipc_client_disconnect(struct ipc_client *client);
void ipc_client_handle_command(struct ipc_client *client);
bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length);

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	if (ipc_event_source) {
		wl_event_source_remove(ipc_event_source);
	}
	close(ipc_socket);
	unlink(ipc_sockaddr->sun_path);

	while (ipc_client_list->length) {
		struct ipc_client *client = ipc_client_list->items[0];
		ipc_client_disconnect(client);
	}
	list_free(ipc_client_list);

	if (ipc_sockaddr) {
		free(ipc_sockaddr);
	}

	wl_list_remove(&ipc_display_destroy.link);
}

void ipc_init(struct sway_server *server) {
	ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_socket == -1) {
		sway_abort("Unable to create IPC socket");
	}

	ipc_sockaddr = ipc_user_sockaddr();

	// We want to use socket name set by user, not existing socket from another sway instance.
	if (getenv("SWAYSOCK") != NULL && access(getenv("SWAYSOCK"), F_OK) == -1) {
		strncpy(ipc_sockaddr->sun_path, getenv("SWAYSOCK"), sizeof(ipc_sockaddr->sun_path) - 1);
		ipc_sockaddr->sun_path[sizeof(ipc_sockaddr->sun_path) - 1] = 0;
	}

	unlink(ipc_sockaddr->sun_path);
	if (bind(ipc_socket, (struct sockaddr *)ipc_sockaddr, sizeof(*ipc_sockaddr)) == -1) {
		sway_abort("Unable to bind IPC socket");
	}

	if (listen(ipc_socket, 3) == -1) {
		sway_abort("Unable to listen on IPC socket");
	}

	// Set i3 IPC socket path so that i3-msg works out of the box
	setenv("I3SOCK", ipc_sockaddr->sun_path, 1);
	setenv("SWAYSOCK", ipc_sockaddr->sun_path, 1);

	ipc_client_list = create_list();

	ipc_display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display, &ipc_display_destroy);

	ipc_event_source = wl_event_loop_add_fd(server->wl_event_loop, ipc_socket,
			WL_EVENT_READABLE, ipc_handle_connection, server);
}

struct sockaddr_un *ipc_user_sockaddr(void) {
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
			"%s/sway-ipc.%i.%i.sock", dir, getuid(), getpid())) {
		sway_abort("Socket path won't fit into ipc_sockaddr->sun_path");
	}

	return ipc_sockaddr;
}

int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	(void) fd;
	struct sway_server *server = data;
	wlr_log(WLR_DEBUG, "Event on IPC listening socket");
	assert(mask == WL_EVENT_READABLE);

	int client_fd = accept(ipc_socket, NULL, NULL);
	if (client_fd == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to accept IPC client connection");
		return 0;
	}

	int flags;
	if ((flags = fcntl(client_fd, F_GETFD)) == -1
			|| fcntl(client_fd, F_SETFD, flags|FD_CLOEXEC) == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to set CLOEXEC on IPC client socket");
		close(client_fd);
		return 0;
	}
	if ((flags = fcntl(client_fd, F_GETFL)) == -1
			|| fcntl(client_fd, F_SETFL, flags|O_NONBLOCK) == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to set NONBLOCK on IPC client socket");
		close(client_fd);
		return 0;
	}

	struct ipc_client *client = malloc(sizeof(struct ipc_client));
	if (!client) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client");
		close(client_fd);
		return 0;
	}
	client->server = server;
	client->payload_length = 0;
	client->fd = client_fd;
	client->subscribed_events = 0;
	client->event_source = wl_event_loop_add_fd(server->wl_event_loop,
			client_fd, WL_EVENT_READABLE, ipc_client_handle_readable, client);
	client->writable_event_source = NULL;

	client->write_buffer_size = 128;
	client->write_buffer_len = 0;
	client->write_buffer = malloc(client->write_buffer_size);
	if (!client->write_buffer) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client write buffer");
		close(client_fd);
		return 0;
	}

	wlr_log(WLR_DEBUG, "New client: fd %d", client_fd);
	list_add(ipc_client_list, client);
	return 0;
}

static const int ipc_header_size = sizeof(ipc_magic) + 8;

int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct ipc_client *client = data;

	if (mask & WL_EVENT_ERROR) {
		wlr_log(WLR_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WL_EVENT_HANGUP) {
		wlr_log(WLR_DEBUG, "Client %d hung up", client->fd);
		ipc_client_disconnect(client);
		return 0;
	}

	wlr_log(WLR_DEBUG, "Client %d readable", client->fd);

	int read_available;
	if (ioctl(client_fd, FIONREAD, &read_available) == -1) {
		wlr_log_errno(WLR_INFO, "Unable to read IPC socket buffer size");
		ipc_client_disconnect(client);
		return 0;
	}

	// Wait for the rest of the command payload in case the header has already been read
	if (client->payload_length > 0) {
		if ((uint32_t)read_available >= client->payload_length) {
			ipc_client_handle_command(client);
		}
		return 0;
	}

	if (read_available < ipc_header_size) {
		return 0;
	}

	uint8_t buf[ipc_header_size];
	uint32_t *buf32 = (uint32_t*)(buf + sizeof(ipc_magic));
	// Should be fully available, because read_available >= ipc_header_size
	ssize_t received = recv(client_fd, buf, ipc_header_size, 0);
	if (received == -1) {
		wlr_log_errno(WLR_INFO, "Unable to receive header from IPC client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (memcmp(buf, ipc_magic, sizeof(ipc_magic)) != 0) {
		wlr_log(WLR_DEBUG, "IPC header check failed");
		ipc_client_disconnect(client);
		return 0;
	}

	client->payload_length = buf32[0];
	client->current_command = (enum ipc_command_type)buf32[1];

	if (read_available - received >= (long)client->payload_length) {
		ipc_client_handle_command(client);
	}

	return 0;
}

static bool ipc_has_event_listeners(enum ipc_command_type event) {
	for (int i = 0; i < ipc_client_list->length; i++) {
		struct ipc_client *client = ipc_client_list->items[i];
		if ((client->subscribed_events & event_mask(event)) == 0) {
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
		client->current_command = event;
		if (!ipc_send_reply(client, json_string, (uint32_t) strlen(json_string))) {
			wlr_log_errno(WLR_INFO, "Unable to send reply to IPC client");
			/* ipc_send_reply destroys client on error, which also
			 * removes it from the list, so we need to process
			 * current index again */
			i--;
		}
	}
}

void ipc_event_workspace(struct sway_container *old,
		struct sway_container *new, const char *change) {
	if (!ipc_has_event_listeners(IPC_EVENT_WORKSPACE)) {
		return;
	}
	wlr_log(WLR_DEBUG, "Sending workspace::%s event", change);
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(change));
	if (old) {
		json_object_object_add(obj, "old",
				ipc_json_describe_container_recursive(old));
	} else {
		json_object_object_add(obj, "old", NULL);
	}

	if (new) {
		json_object_object_add(obj, "current",
				ipc_json_describe_container_recursive(new));
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
	wlr_log(WLR_DEBUG, "Sending window::%s event", change);
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(change));
	json_object_object_add(obj, "container", ipc_json_describe_container_recursive(window));

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_WINDOW);
	json_object_put(obj);
}

void ipc_event_barconfig_update(struct bar_config *bar) {
	if (!ipc_has_event_listeners(IPC_EVENT_BARCONFIG_UPDATE)) {
		return;
	}
	wlr_log(WLR_DEBUG, "Sending barconfig_update event");
	json_object *json = ipc_json_describe_bar_config(bar);

	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BARCONFIG_UPDATE);
	json_object_put(json);
}

void ipc_event_mode(const char *mode, bool pango) {
	if (!ipc_has_event_listeners(IPC_EVENT_MODE)) {
		return;
	}
	wlr_log(WLR_DEBUG, "Sending mode::%s event", mode);
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
	wlr_log(WLR_DEBUG, "Sending shutdown::%s event", reason);

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
	wlr_log(WLR_DEBUG, "Sending binding event");

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
	json_object_object_add(json_binding, "input_type", binding->type == BINDING_MOUSE ?
			json_object_new_string("mouse") : json_object_new_string("keyboard"));

	json_object *json = json_object_new_object();
	json_object_object_add(json, "change", json_object_new_string("run"));
	json_object_object_add(json, "binding", json_binding);
	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BINDING);
	json_object_put(json);
}

static void ipc_event_tick(const char *payload) {
	if (!ipc_has_event_listeners(IPC_EVENT_TICK)) {
		return;
	}
	wlr_log(WLR_DEBUG, "Sending tick event");

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
		wlr_log(WLR_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WL_EVENT_HANGUP) {
		wlr_log(WLR_DEBUG, "Client %d hung up", client->fd);
		ipc_client_disconnect(client);
		return 0;
	}

	if (client->write_buffer_len <= 0) {
		return 0;
	}

	wlr_log(WLR_DEBUG, "Client %d writable", client->fd);

	ssize_t written = write(client->fd, client->write_buffer, client->write_buffer_len);

	if (written == -1 && errno == EAGAIN) {
		return 0;
	} else if (written == -1) {
		wlr_log_errno(WLR_INFO, "Unable to send data from queue to IPC client");
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

	wlr_log(WLR_INFO, "IPC Client %d disconnected", client->fd);
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

static void ipc_get_workspaces_callback(struct sway_container *workspace,
		void *data) {
	if (workspace->type != C_WORKSPACE) {
		return;
	}
	json_object *workspace_json = ipc_json_describe_container(workspace);
	// override the default focused indicator because
	// it's set differently for the get_workspaces reply
	struct sway_seat *seat =
		input_manager_get_default_seat(input_manager);
	struct sway_container *focused_ws = seat_get_focus(seat);
	if (focused_ws != NULL && focused_ws->type != C_WORKSPACE) {
		focused_ws = container_parent(focused_ws, C_WORKSPACE);
	}
	bool focused = workspace == focused_ws;
	json_object_object_del(workspace_json, "focused");
	json_object_object_add(workspace_json, "focused",
			json_object_new_boolean(focused));
	json_object_array_add((json_object *)data, workspace_json);

	focused_ws = seat_get_focus_inactive(seat, workspace->parent);
	if (focused_ws->type != C_WORKSPACE) {
		focused_ws = container_parent(focused_ws, C_WORKSPACE);
	}
	bool visible = workspace == focused_ws;
	json_object_object_add(workspace_json, "visible",
			json_object_new_boolean(visible));
}

static void ipc_get_marks_callback(struct sway_container *con, void *data) {
	json_object *marks = (json_object *)data;
	if (con->type == C_VIEW && con->sway_view->marks) {
		for (int i = 0; i < con->sway_view->marks->length; ++i) {
			char *mark = (char *)con->sway_view->marks->items[i];
			json_object_array_add(marks, json_object_new_string(mark));
		}
	}
}

void ipc_client_handle_command(struct ipc_client *client) {
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	char *buf = malloc(client->payload_length + 1);
	if (!buf) {
		wlr_log_errno(WLR_INFO, "Unable to allocate IPC payload");
		ipc_client_disconnect(client);
		return;
	}
	if (client->payload_length > 0) {
		// Payload should be fully available
		ssize_t received = recv(client->fd, buf, client->payload_length, 0);
		if (received == -1)
		{
			wlr_log_errno(WLR_INFO, "Unable to receive payload from IPC client");
			ipc_client_disconnect(client);
			free(buf);
			return;
		}
	}
	buf[client->payload_length] = '\0';

	bool client_valid = true;
	switch (client->current_command) {
	case IPC_COMMAND:
	{
		struct cmd_results *results = execute_command(buf, NULL);
		transaction_commit_dirty();
		char *json = cmd_results_to_json(results);
		int length = strlen(json);
		client_valid = ipc_send_reply(client, json, (uint32_t)length);
		free(json);
		free_cmd_results(results);
		goto exit_cleanup;
	}

	case IPC_SEND_TICK:
	{
		ipc_event_tick(buf);
		ipc_send_reply(client, "{\"success\": true}", 17);
		goto exit_cleanup;
	}

	case IPC_GET_OUTPUTS:
	{
		json_object *outputs = json_object_new_array();
		for (int i = 0; i < root_container.children->length; ++i) {
			struct sway_container *container = root_container.children->items[i];
			if (container->type == C_OUTPUT) {
				json_object_array_add(outputs,
					ipc_json_describe_container(container));
			}
		}
		struct sway_output *output;
		wl_list_for_each(output, &root_container.sway_root->outputs, link) {
			if (!output->swayc) {
				json_object_array_add(outputs,
						ipc_json_describe_disabled_output(output));
			}
		}
		const char *json_string = json_object_to_json_string(outputs);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(outputs); // free
		goto exit_cleanup;
	}

	case IPC_GET_WORKSPACES:
	{
		json_object *workspaces = json_object_new_array();
		container_for_each_descendant_dfs(&root_container,
				ipc_get_workspaces_callback, workspaces);
		const char *json_string = json_object_to_json_string(workspaces);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(workspaces); // free
		goto exit_cleanup;
	}

	case IPC_SUBSCRIBE:
	{
		// TODO: Check if they're permitted to use these events
		struct json_object *request = json_tokener_parse(buf);
		if (request == NULL) {
			client_valid = ipc_send_reply(client, "{\"success\": false}", 18);
			wlr_log_errno(WLR_INFO, "Failed to read request");
			goto exit_cleanup;
		}

		bool is_tick = false;
		// parse requested event types
		for (size_t i = 0; i < json_object_array_length(request); i++) {
			const char *event_type = json_object_get_string(json_object_array_get_idx(request, i));
			if (strcmp(event_type, "workspace") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_WORKSPACE);
			} else if (strcmp(event_type, "barconfig_update") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_BARCONFIG_UPDATE);
			} else if (strcmp(event_type, "mode") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_MODE);
			} else if (strcmp(event_type, "shutdown") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_SHUTDOWN);
			} else if (strcmp(event_type, "window") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_WINDOW);
			} else if (strcmp(event_type, "modifier") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_MODIFIER);
			} else if (strcmp(event_type, "binding") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_BINDING);
			} else if (strcmp(event_type, "tick") == 0) {
				client->subscribed_events |= event_mask(IPC_EVENT_TICK);
				is_tick = true;
			} else {
				client_valid =
					ipc_send_reply(client, "{\"success\": false}", 18);
				json_object_put(request);
				wlr_log_errno(WLR_INFO, "Failed to parse request");
				goto exit_cleanup;
			}
		}

		json_object_put(request);
		client_valid = ipc_send_reply(client, "{\"success\": true}", 17);
		if (is_tick) {
			client->current_command = IPC_EVENT_TICK;
			ipc_send_reply(client, "{\"first\": true, \"payload\": \"\"}", 30);
		}
		goto exit_cleanup;
	}

	case IPC_GET_INPUTS:
	{
		json_object *inputs = json_object_new_array();
		struct sway_input_device *device = NULL;
		wl_list_for_each(device, &input_manager->devices, link) {
			json_object_array_add(inputs, ipc_json_describe_input(device));
		}
		const char *json_string = json_object_to_json_string(inputs);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(inputs); // free
		goto exit_cleanup;
	}

	case IPC_GET_SEATS:
	{
		json_object *seats = json_object_new_array();
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &input_manager->seats, link) {
			json_object_array_add(seats, ipc_json_describe_seat(seat));
		}
		const char *json_string = json_object_to_json_string(seats);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(seats); // free
		goto exit_cleanup;
	}

	case IPC_GET_TREE:
	{
		json_object *tree =
			ipc_json_describe_container_recursive(&root_container);
		const char *json_string = json_object_to_json_string(tree);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t) strlen(json_string));
		json_object_put(tree);
		goto exit_cleanup;
	}

	case IPC_GET_MARKS:
	{
		json_object *marks = json_object_new_array();
		container_descendants(&root_container, C_VIEW, ipc_get_marks_callback,
				marks);
		const char *json_string = json_object_to_json_string(marks);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(marks);
		goto exit_cleanup;
	}

	case IPC_GET_VERSION:
	{
		json_object *version = ipc_json_get_version();
		const char *json_string = json_object_to_json_string(version);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(version); // free
		goto exit_cleanup;
	}

	case IPC_GET_BAR_CONFIG:
	{
		if (!buf[0]) {
			// Send list of configured bar IDs
			json_object *bars = json_object_new_array();
			for (int i = 0; i < config->bars->length; ++i) {
				struct bar_config *bar = config->bars->items[i];
				json_object_array_add(bars, json_object_new_string(bar->id));
			}
			const char *json_string = json_object_to_json_string(bars);
			client_valid =
				ipc_send_reply(client, json_string,
					(uint32_t)strlen(json_string));
			json_object_put(bars); // free
		} else {
			// Send particular bar's details
			struct bar_config *bar = NULL;
			for (int i = 0; i < config->bars->length; ++i) {
				bar = config->bars->items[i];
				if (strcmp(buf, bar->id) == 0) {
					break;
				}
				bar = NULL;
			}
			if (!bar) {
				const char *error = "{ \"success\": false, \"error\": \"No bar with that ID\" }";
				client_valid =
					ipc_send_reply(client, error, (uint32_t)strlen(error));
				goto exit_cleanup;
			}
			json_object *json = ipc_json_describe_bar_config(bar);
			const char *json_string = json_object_to_json_string(json);
			client_valid =
				ipc_send_reply(client, json_string,
					(uint32_t)strlen(json_string));
			json_object_put(json); // free
		}
		goto exit_cleanup;
	}

	case IPC_GET_BINDING_MODES:
	{
		json_object *modes = json_object_new_array();
		for (int i = 0; i < config->modes->length; i++) {
			struct sway_mode *mode = config->modes->items[i];
			json_object_array_add(modes, json_object_new_string(mode->name));
		}
		const char *json_string = json_object_to_json_string(modes);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(modes); // free
		goto exit_cleanup;
	}

	case IPC_GET_CONFIG:
	{
		json_object *json = json_object_new_object();
		json_object_object_add(json, "config", json_object_new_string(config->current_config));
		const char *json_string = json_object_to_json_string(json);
		client_valid =
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(json); // free
		goto exit_cleanup;
    }

	default:
		wlr_log(WLR_INFO, "Unknown IPC command type %i", client->current_command);
		goto exit_cleanup;
	}

exit_cleanup:
	if (client_valid) {
		client->payload_length = 0;
	}
	free(buf);
	return;
}

bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length) {
	assert(payload);

	char data[ipc_header_size];
	uint32_t *data32 = (uint32_t*)(data + sizeof(ipc_magic));

	memcpy(data, ipc_magic, sizeof(ipc_magic));
	data32[0] = payload_length;
	data32[1] = client->current_command;

	while (client->write_buffer_len + ipc_header_size + payload_length >=
				 client->write_buffer_size) {
		client->write_buffer_size *= 2;
	}

	if (client->write_buffer_size > 4e6) { // 4 MB
		wlr_log(WLR_ERROR, "Client write buffer too big, disconnecting client");
		ipc_client_disconnect(client);
		return false;
	}

	char *new_buffer = realloc(client->write_buffer, client->write_buffer_size);
	if (!new_buffer) {
		wlr_log(WLR_ERROR, "Unable to reallocate ipc client write buffer");
		ipc_client_disconnect(client);
		return false;
	}
	client->write_buffer = new_buffer;

	memcpy(client->write_buffer + client->write_buffer_len, data, ipc_header_size);
	client->write_buffer_len += ipc_header_size;
	memcpy(client->write_buffer + client->write_buffer_len, payload, payload_length);
	client->write_buffer_len += payload_length;

	if (!client->writable_event_source) {
		client->writable_event_source = wl_event_loop_add_fd(
				server.wl_event_loop, client->fd, WL_EVENT_WRITABLE,
				ipc_client_handle_writable, client);
	}

	wlr_log(WLR_DEBUG, "Added IPC reply to client %d queue: %s", client->fd, payload);
	return true;
}
