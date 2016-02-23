// See https://i3wm.org/docs/ipc.html for protocol information

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <wlc/wlc-render.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <json-c/json.h>
#include <list.h>
#include <libinput.h>
#include "ipc-server.h"
#include "log.h"
#include "config.h"
#include "commands.h"
#include "list.h"
#include "stringop.h"
#include "util.h"
#include "input.h"

static int ipc_socket = -1;
static struct wlc_event_source *ipc_event_source =  NULL;
static struct sockaddr_un *ipc_sockaddr = NULL;
static list_t *ipc_client_list = NULL;

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

struct ipc_client {
	struct wlc_event_source *event_source;
	int fd;
	uint32_t payload_length;
	enum ipc_command_type current_command;
	enum ipc_command_type subscribed_events;
};

static list_t *ipc_get_pixel_requests = NULL;

struct get_pixels_request {
	struct ipc_client *client;
	wlc_handle output;
};

struct sockaddr_un *ipc_user_sockaddr(void);
int ipc_handle_connection(int fd, uint32_t mask, void *data);
int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data);
void ipc_client_disconnect(struct ipc_client *client);
void ipc_client_handle_command(struct ipc_client *client);
bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length);
void ipc_get_workspaces_callback(swayc_t *workspace, void *data);
void ipc_get_outputs_callback(swayc_t *container, void *data);
json_object *ipc_json_describe_bar_config(struct bar_config *bar);

void ipc_init(void) {
	ipc_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_socket == -1) {
		sway_abort("Unable to create IPC socket");
	}

	ipc_sockaddr = ipc_user_sockaddr();

	// We want to use socket name set by user, not existing socket from another sway instance.
	if (getenv("SWAYSOCK") != NULL && access(getenv("SWAYSOCK"), F_OK) == -1) {
		strncpy(ipc_sockaddr->sun_path, getenv("SWAYSOCK"), sizeof(ipc_sockaddr->sun_path));
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
	ipc_get_pixel_requests = create_list();

	ipc_event_source = wlc_event_loop_add_fd(ipc_socket, WLC_EVENT_READABLE, ipc_handle_connection, NULL);
}

void ipc_terminate(void) {
	if (ipc_event_source) {
		wlc_event_source_remove(ipc_event_source);
	}
	close(ipc_socket);
	unlink(ipc_sockaddr->sun_path);

	list_free(ipc_client_list);

	if (ipc_sockaddr) {
		free(ipc_sockaddr);
	}
}

struct sockaddr_un *ipc_user_sockaddr(void) {
	struct sockaddr_un *ipc_sockaddr = malloc(sizeof(struct sockaddr_un));
	if (ipc_sockaddr == NULL) {
		sway_abort("can't malloc ipc_sockaddr");
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
		sway_abort("socket path won't fit into ipc_sockaddr->sun_path");
	}

	return ipc_sockaddr;
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

	list_add(ipc_client_list, client);

	return 0;
}

static const int ipc_header_size = sizeof(ipc_magic)+8;

int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct ipc_client *client = data;

	if (mask & WLC_EVENT_ERROR) {
		sway_log(L_INFO, "IPC Client socket error, removing client");
		client->fd = -1;
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WLC_EVENT_HANGUP) {
		client->fd = -1;
		ipc_client_disconnect(client);
		return 0;
	}

	int read_available;
	if (ioctl(client_fd, FIONREAD, &read_available) == -1) {
		sway_log_errno(L_INFO, "Unable to read IPC socket buffer size");
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

	client->payload_length = buf32[0];
	client->current_command = (enum ipc_command_type)buf32[1];

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

	if (client->fd != -1) {
		shutdown(client->fd, SHUT_RDWR);
	}

	sway_log(L_INFO, "IPC Client %d disconnected", client->fd);
	wlc_event_source_remove(client->event_source);
	int i = 0;
	while (i < ipc_client_list->length && ipc_client_list->items[i] != client) i++;
	list_del(ipc_client_list, i);
	close(client->fd);
	free(client);
}

bool output_by_name_test(swayc_t *view, void *data) {
	char *name = (char *)data;
	if (view->type != C_OUTPUT) {
		return false;
	}
	return !strcmp(name, view->name);
}

void ipc_get_pixels(wlc_handle output) {
	if (ipc_get_pixel_requests->length == 0) {
		return;
	}

	list_t *unhandled = create_list();

	struct get_pixels_request *req;
	int i;
	for (i = 0; i < ipc_get_pixel_requests->length; ++i) {
		req = ipc_get_pixel_requests->items[i];
		if (req->output != output) {
			list_add(unhandled, req);
			continue;
		}

		const struct wlc_size *size = wlc_output_get_resolution(req->output);
		struct wlc_geometry g = {
			.size = *size,
			.origin = { 0, 0 },
		};
		struct wlc_geometry g_out;
		char response_header[9];
		memset(response_header, 0, sizeof(response_header));
		char *data = malloc(sizeof(response_header) + size->w * size->h * 4);
		wlc_pixels_read(WLC_RGBA8888, &g, &g_out, data + sizeof(response_header));

		response_header[0] = 1;
		uint32_t *_size = (uint32_t *)(response_header + 1);
		_size[0] = g_out.size.w;
		_size[1] = g_out.size.h;
		size_t len = sizeof(response_header) + (g_out.size.w * g_out.size.h * 4);
		memcpy(data, response_header, sizeof(response_header));
		ipc_send_reply(req->client, data, len);
		free(data);
		// free the request since it has been handled
		free(req);
	}

	// free old list of pixel requests and set new list to all unhandled
	// requests (request for another output).
	list_free(ipc_get_pixel_requests);
	ipc_get_pixel_requests = unhandled;
}

void ipc_client_handle_command(struct ipc_client *client) {
	if (!sway_assert(client != NULL, "client != NULL")) {
		return;
	}

	char *buf = malloc(client->payload_length + 1);
	if (client->payload_length > 0)
	{
		ssize_t received = recv(client->fd, buf, client->payload_length, 0);
		if (received == -1)
		{
			sway_log_errno(L_INFO, "Unable to receive payload from IPC client");
			ipc_client_disconnect(client);
			free(buf);
			return;
		}
	}

	switch (client->current_command) {
	case IPC_COMMAND:
	{
		buf[client->payload_length] = '\0';
		struct cmd_results *results = handle_command(buf);
		const char *json = cmd_results_to_json(results);
		char reply[256];
		int length = snprintf(reply, sizeof(reply), "%s", json);
		ipc_send_reply(client, reply, (uint32_t) length);
		free_cmd_results(results);
		break;
	}
	case IPC_SUBSCRIBE:
	{
		buf[client->payload_length] = '\0';
		struct json_object *request = json_tokener_parse(buf);
		if (request == NULL) {
			ipc_send_reply(client, "{\"success\": false}", 18);
			ipc_client_disconnect(client);
			free(buf);
			return;
		}

		// parse requested event types
		for (int i = 0; i < json_object_array_length(request); i++) {
			const char *event_type = json_object_get_string(json_object_array_get_idx(request, i));
			if (strcmp(event_type, "workspace") == 0) {
				client->subscribed_events |= IPC_EVENT_WORKSPACE;
			} else if (strcmp(event_type, "barconfig_update") == 0) {
				client->subscribed_events |= IPC_EVENT_BARCONFIG_UPDATE;
			} else if (strcmp(event_type, "mode") == 0) {
				client->subscribed_events |= IPC_EVENT_MODE;
			} else if (strcmp(event_type, "modifier") == 0) {
				client->subscribed_events |= IPC_EVENT_MODIFIER;
#if SWAY_BINDING_EVENT
			} else if (strcmp(event_type, "binding") == 0) {
				client->subscribed_events |= IPC_EVENT_BINDING;
#endif
			} else {
				ipc_send_reply(client, "{\"success\": false}", 18);
				ipc_client_disconnect(client);
				json_object_put(request);
				free(buf);
				return;
			}
		}

		json_object_put(request);

		ipc_send_reply(client, "{\"success\": true}", 17);
		break;
	}
	case IPC_GET_WORKSPACES:
	{
		json_object *workspaces = json_object_new_array();
		container_map(&root_container, ipc_get_workspaces_callback, workspaces);
		const char *json_string = json_object_to_json_string(workspaces);
		ipc_send_reply(client, json_string, (uint32_t) strlen(json_string));
		json_object_put(workspaces); // free
		break;
	}
	case IPC_GET_INPUTS:
	{
		json_object *inputs = json_object_new_array();
		if (input_devices) {
			for(int i=0; i<input_devices->length; i++) {
				struct libinput_device *device = input_devices->items[i];
				char* identifier = libinput_dev_unique_id(device);
				json_object *device_object = json_object_new_object();
				json_object_object_add(device_object, "identifier", json_object_new_string(identifier));
				json_object_array_add(inputs, device_object);
				free(identifier);
			}
		}
		const char *json_string = json_object_to_json_string(inputs);
		ipc_send_reply(client, json_string, (uint32_t) strlen(json_string));
		json_object_put(inputs);
		break;
	}
	case IPC_GET_OUTPUTS:
	{
		json_object *outputs = json_object_new_array();
		container_map(&root_container, ipc_get_outputs_callback, outputs);
		const char *json_string = json_object_to_json_string(outputs);
		ipc_send_reply(client, json_string, (uint32_t) strlen(json_string));
		json_object_put(outputs); // free
		break;
	}
	case IPC_GET_VERSION:
	{
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
		char *full_version = calloc(strlen(SWAY_GIT_VERSION) + strlen(SWAY_GIT_BRANCH) + strlen(SWAY_VERSION_DATE) + 20, 1);
		strcat(full_version, SWAY_GIT_VERSION);
		strcat(full_version, " (");
		strcat(full_version, SWAY_VERSION_DATE);
		strcat(full_version, ", branch \"");
		strcat(full_version, SWAY_GIT_BRANCH);
		strcat(full_version, "\")");
		json_object *json = json_object_new_object();
		json_object_object_add(json, "human_readable", json_object_new_string(full_version));
		json_object_object_add(json, "variant", json_object_new_string("sway"));
		// Todo once we actually release a version
		json_object_object_add(json, "major", json_object_new_int(0));
		json_object_object_add(json, "minor", json_object_new_int(0));
		json_object_object_add(json, "patch", json_object_new_int(1));
#else
		json_object_object_add(json, "human_readable", json_object_new_string("version not found"));
		json_object_object_add(json, "major", json_object_new_int(0));
		json_object_object_add(json, "minor", json_object_new_int(0));
		json_object_object_add(json, "patch", json_object_new_int(0));
#endif
		const char *json_string = json_object_to_json_string(json);
		ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
		json_object_put(json); // free
		free(full_version);
		break;
	}
	case IPC_SWAY_GET_PIXELS:
	{
		char response_header[9];
		memset(response_header, 0, sizeof(response_header));
		buf[client->payload_length] = '\0';
		swayc_t *output = swayc_by_test(&root_container, output_by_name_test, buf);
		if (!output) {
			sway_log(L_ERROR, "IPC GET_PIXELS request with unknown output name");
			ipc_send_reply(client, response_header, sizeof(response_header));
			break;
		}
		struct get_pixels_request *req = malloc(sizeof(struct get_pixels_request));
		req->client = client;
		req->output = output->handle;
		list_add(ipc_get_pixel_requests, req);
		wlc_output_schedule_render(output->handle);
		break;
	}
	case IPC_GET_BAR_CONFIG:
	{
		buf[client->payload_length] = '\0';
		if (!buf[0]) {
			// Send list of configured bar IDs
			json_object *bars = json_object_new_array();
			int i;
			for (i = 0; i < config->bars->length; ++i) {
				struct bar_config *bar = config->bars->items[i];
				json_object_array_add(bars, json_object_new_string(bar->id));
			}
			const char *json_string = json_object_to_json_string(bars);
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
			json_object_put(bars); // free
		} else {
			// Send particular bar's details
			buf[client->payload_length] = '\0';
			struct bar_config *bar = NULL;
			int i;
			for (i = 0; i < config->bars->length; ++i) {
				bar = config->bars->items[i];
				if (strcmp(buf, bar->id) == 0) {
					break;
				}
				bar = NULL;
			}
			if (!bar) {
				const char *error = "{ \"success\": false, \"error\": \"No bar with that ID\" }";
				ipc_send_reply(client, error, (uint32_t)strlen(error));
				break;
			}
			json_object *json = ipc_json_describe_bar_config(bar);
			const char *json_string = json_object_to_json_string(json);
			ipc_send_reply(client, json_string, (uint32_t)strlen(json_string));
			json_object_put(json); // free
			break;
		}
	}
	default:
		sway_log(L_INFO, "Unknown IPC command type %i", client->current_command);
		ipc_client_disconnect(client);
		return;
	}

	client->payload_length = 0;
	free(buf);
}

bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t payload_length) {
	assert(payload);

	char data[ipc_header_size];
	uint32_t *data32 = (uint32_t*)(data + sizeof(ipc_magic));

	memcpy(data, ipc_magic, sizeof(ipc_magic));
	data32[0] = payload_length;
	data32[1] = client->current_command;

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

json_object *ipc_json_describe_workspace(swayc_t *workspace) {
	if (!sway_assert(workspace, "Workspace must not be NULL")) {
		return NULL;
	}

	int num = isdigit(workspace->name[0]) ? atoi(workspace->name) : -1;
	json_object *object = json_object_new_object();
	json_object *rect = json_object_new_object();
	json_object_object_add(rect, "x", json_object_new_int((int32_t) workspace->x));
	json_object_object_add(rect, "y", json_object_new_int((int32_t) workspace->y));
	json_object_object_add(rect, "width", json_object_new_int((int32_t) workspace->width));
	json_object_object_add(rect, "height", json_object_new_int((int32_t) workspace->height));

	json_object_object_add(object, "num", json_object_new_int(num));
	json_object_object_add(object, "name", json_object_new_string(workspace->name));
	json_object_object_add(object, "visible", json_object_new_boolean(workspace->visible));
	bool focused = root_container.focused == workspace->parent && workspace->parent->focused == workspace;
	json_object_object_add(object, "focused", json_object_new_boolean(focused));
	json_object_object_add(object, "rect", rect);
	json_object_object_add(object, "output", json_object_new_string(workspace->parent ? workspace->parent->name : "null"));
	json_object_object_add(object, "urgent", json_object_new_boolean(false));

	return object;
}

void ipc_get_workspaces_callback(swayc_t *workspace, void *data) {
	if (workspace->type == C_WORKSPACE) {
		json_object_array_add((json_object *)data, ipc_json_describe_workspace(workspace));
	}
}

json_object *ipc_json_describe_output(swayc_t *output) {
	json_object *object = json_object_new_object();
	json_object *rect = json_object_new_object();
	json_object_object_add(rect, "x", json_object_new_int((int32_t) output->x));
	json_object_object_add(rect, "y", json_object_new_int((int32_t) output->y));
	json_object_object_add(rect, "width", json_object_new_int((int32_t) output->width));
	json_object_object_add(rect, "height", json_object_new_int((int32_t) output->height));

	json_object_object_add(object, "name", json_object_new_string(output->name));
	json_object_object_add(object, "active", json_object_new_boolean(true));
	json_object_object_add(object, "primary", json_object_new_boolean(false));
	json_object_object_add(object, "rect", rect);
	json_object_object_add(object, "current_workspace",
		output->focused ? json_object_new_string(output->focused->name) : NULL);

	return object;
}

void ipc_get_outputs_callback(swayc_t *container, void *data) {
	if (container->type == C_OUTPUT) {
		json_object_array_add((json_object *)data, ipc_json_describe_output(container));
	}
}

json_object *ipc_json_describe_bar_config(struct bar_config *bar) {
	if (!sway_assert(bar, "Bar must not be NULL")) {
		return NULL;
	}

	json_object *json = json_object_new_object();
	json_object_object_add(json, "id", json_object_new_string(bar->id));
	json_object_object_add(json, "tray_output", NULL);
	json_object_object_add(json, "mode", json_object_new_string(bar->mode));
	json_object_object_add(json, "hidden_state", json_object_new_string(bar->hidden_state));
	json_object_object_add(json, "modifier", json_object_new_string(get_modifier_name_by_mask(bar->modifier)));
	switch (bar->position) {
	case DESKTOP_SHELL_PANEL_POSITION_TOP:
		json_object_object_add(json, "position", json_object_new_string("top"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
		json_object_object_add(json, "position", json_object_new_string("bottom"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_LEFT:
		json_object_object_add(json, "position", json_object_new_string("left"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_RIGHT:
		json_object_object_add(json, "position", json_object_new_string("right"));
		break;
	}
	json_object_object_add(json, "status_command", json_object_new_string(bar->status_command));
	json_object_object_add(json, "font", json_object_new_string(bar->font ? bar->font : config->font));
	if (bar->separator_symbol) {
		json_object_object_add(json, "separator_symbol", json_object_new_string(bar->separator_symbol));
	}
	json_object_object_add(json, "bar_height", json_object_new_int(bar->height));
	json_object_object_add(json, "workspace_buttons", json_object_new_boolean(bar->workspace_buttons));
	json_object_object_add(json, "strip_workspace_numbers", json_object_new_boolean(bar->strip_workspace_numbers));
	json_object_object_add(json, "binding_mode_indicator", json_object_new_boolean(bar->binding_mode_indicator));
	json_object_object_add(json, "verbose", json_object_new_boolean(bar->verbose));

	json_object *colors = json_object_new_object();
	json_object_object_add(colors, "background", json_object_new_string(bar->colors.background));
	json_object_object_add(colors, "statusline", json_object_new_string(bar->colors.statusline));
	json_object_object_add(colors, "separator", json_object_new_string(bar->colors.separator));

	json_object_object_add(colors, "focused_workspace_border", json_object_new_string(bar->colors.focused_workspace_border));
	json_object_object_add(colors, "focused_workspace_bg", json_object_new_string(bar->colors.focused_workspace_bg));
	json_object_object_add(colors, "focused_workspace_text", json_object_new_string(bar->colors.focused_workspace_text));

	json_object_object_add(colors, "inactive_workspace_border", json_object_new_string(bar->colors.inactive_workspace_border));
	json_object_object_add(colors, "inactive_workspace_bg", json_object_new_string(bar->colors.inactive_workspace_bg));
	json_object_object_add(colors, "inactive_workspace_text", json_object_new_string(bar->colors.inactive_workspace_text));

	json_object_object_add(colors, "active_workspace_border", json_object_new_string(bar->colors.active_workspace_border));
	json_object_object_add(colors, "active_workspace_bg", json_object_new_string(bar->colors.active_workspace_bg));
	json_object_object_add(colors, "active_workspace_text", json_object_new_string(bar->colors.active_workspace_text));

	json_object_object_add(colors, "urgent_workspace_border", json_object_new_string(bar->colors.urgent_workspace_border));
	json_object_object_add(colors, "urgent_workspace_bg", json_object_new_string(bar->colors.urgent_workspace_bg));
	json_object_object_add(colors, "urgent_workspace_text", json_object_new_string(bar->colors.urgent_workspace_text));

	json_object_object_add(colors, "binding_mode_border", json_object_new_string(bar->colors.binding_mode_border));
	json_object_object_add(colors, "binding_mode_bg", json_object_new_string(bar->colors.binding_mode_bg));
	json_object_object_add(colors, "binding_mode_text", json_object_new_string(bar->colors.binding_mode_text));

	json_object_object_add(json, "colors", colors);

	// Add outputs if defined
	if (bar->outputs && bar->outputs->length > 0) {
		json_object *outputs = json_object_new_array();
		int i;
		for (i = 0; i < bar->outputs->length; ++i) {
			const char *name = bar->outputs->items[i];
			json_object_array_add(outputs, json_object_new_string(name));
		}
		json_object_object_add(json, "outputs", outputs);
	}

	return json;
}

void ipc_send_event(const char *json_string, enum ipc_command_type event) {
	int i;
	struct ipc_client *client;
	for (i = 0; i < ipc_client_list->length; i++) {
		client = ipc_client_list->items[i];
		if ((client->subscribed_events & event) == 0) {
			continue;
		}
		client->current_command = event;
		ipc_send_reply(client, json_string, (uint32_t) strlen(json_string));
	}
}

void ipc_event_workspace(swayc_t *old, swayc_t *new, const char *change) {
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(change));
	if (strcmp("focus", change) == 0) {
		if (old) {
			json_object_object_add(obj, "old", ipc_json_describe_workspace(old));
		} else {
			json_object_object_add(obj, "old", NULL);
		}
	}

	if (new) {
		json_object_object_add(obj, "current", ipc_json_describe_workspace(new));
	} else {
		json_object_object_add(obj, "current", NULL);
	}

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_WORKSPACE);

	json_object_put(obj); // free
}

void ipc_event_barconfig_update(struct bar_config *bar) {
	json_object *json = ipc_json_describe_bar_config(bar);
	const char *json_string = json_object_to_json_string(json);
	ipc_send_event(json_string, IPC_EVENT_BARCONFIG_UPDATE);

	json_object_put(json); // free
}

void ipc_event_mode(const char *mode) {
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(mode));

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_MODE);

	json_object_put(obj); // free
}

void ipc_event_modifier(uint32_t modifier, const char *state) {
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string(state));

	const char *modifier_name = get_modifier_name_by_mask(modifier);
	json_object_object_add(obj, "modifier", json_object_new_string(modifier_name));

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_MODIFIER);

	json_object_put(obj); // free
}

#if SWAY_BINDING_EVENT
static void ipc_event_binding(json_object *sb_obj) {
	json_object *obj = json_object_new_object();
	json_object_object_add(obj, "change", json_object_new_string("run"));
	json_object_object_add(obj, "binding", sb_obj);

	const char *json_string = json_object_to_json_string(obj);
	ipc_send_event(json_string, IPC_EVENT_BINDING);

	json_object_put(obj); // free
}
#endif

void ipc_event_binding_keyboard(struct sway_binding *sb) {
#if SWAY_BINDING_EVENT
	json_object *sb_obj = json_object_new_object();
	json_object_object_add(sb_obj, "command", json_object_new_string(sb->command));

	const char *names[10];

	int len = get_modifier_names(names, sb->modifiers);
	int i;
	json_object *modifiers = json_object_new_array();
	for (i = 0; i < len; ++i) {
		json_object_array_add(modifiers, json_object_new_string(names[i]));
	}

	json_object_object_add(sb_obj, "event_state_mask", modifiers);

	json_object *input_codes = json_object_new_array();
	int input_code = 0;
	json_object *symbols = json_object_new_array();
	json_object *symbol = NULL;

	if (sb->bindcode) { // bindcode: populate input_codes
		uint32_t keycode;
		for (i = 0; i < sb->keys->length; ++i) {
			keycode = *(uint32_t *)sb->keys->items[i];
			json_object_array_add(input_codes, json_object_new_int(keycode));
			if (i == 0) {
				input_code = keycode;
			}
		}
	} else { // bindsym: populate symbols
		uint32_t keysym;
		char buffer[64];
		for (i = 0; i < sb->keys->length; ++i) {
			keysym = *(uint32_t *)sb->keys->items[i];
			if (xkb_keysym_get_name(keysym, buffer, 64) > 0) {
				json_object *str = json_object_new_string(buffer);
				json_object_array_add(symbols, str);
				if (i == 0) {
					symbol = str;
				}
			}
		}
	}

	json_object_object_add(sb_obj, "input_codes", input_codes);
	json_object_object_add(sb_obj, "input_code", json_object_new_int(input_code));
	json_object_object_add(sb_obj, "symbols", symbols);
	json_object_object_add(sb_obj, "symbol", symbol);
	json_object_object_add(sb_obj, "input_type", json_object_new_string("keyboard"));

	ipc_event_binding(sb_obj);
#endif
}
