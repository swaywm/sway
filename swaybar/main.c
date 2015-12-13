#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stropts.h>
#include <json-c/json.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "ipc-client.h"
#include "readline.h"
#include "client/registry.h"
#include "client/window.h"
#include "client/pango.h"
#include "stringop.h"
#include "log.h"

#define MARGIN 5

struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

struct colors {
	uint32_t background;
	uint32_t statusline;
	uint32_t seperator;

	struct box_colors focused_workspace;
	struct box_colors active_workspace;
	struct box_colors inactive_workspace;
	struct box_colors urgent_workspace;
	struct box_colors binding_mode;
};

struct workspace {
	int num;
	char *name;
	bool focused;
	bool visible;
	bool urgent;
};

list_t *workspaces = NULL;
int socketfd;
FILE *command;
char *line, *output;
struct registry *registry;
struct window *window;

struct colors colors = {
	.background = 0x000000FF,
	.statusline = 0xFFFFFFFF,
	.seperator = 0x666666FF,

	.focused_workspace = {
		.border = 0x4C7899FF,
		.background = 0x285577FF,
		.text = 0xFFFFFFFF
	},
	.active_workspace = {
		.border = 0x333333FF,
		.background = 0x5F676AFF,
		.text = 0xFFFFFFFF
	},
	.inactive_workspace = {
		.border = 0x333333FF,
		.background = 0x222222FF,
		.text = 0x888888FF
	},
	.urgent_workspace = {
		.border = 0x2F343AFF,
		.background = 0x900000FF,
		.text = 0xFFFFFFFF
	},
	.binding_mode = {
		.border = 0x2F343AFF,
		.background = 0x900000FF,
		.text = 0xFFFFFFFF
	},
};

void sway_terminate(void) {
	window_teardown(window);
	registry_teardown(registry);
	exit(EXIT_FAILURE);
}

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo, 
			((color & 0xFF000000) >> 24) / 256.0,
			((color & 0xFF0000) >> 16) / 256.0,
			((color & 0xFF00) >> 8) / 256.0,
			(color & 0xFF) / 256.0);
}

void ipc_update_workspaces() {
	if (workspaces) {
		free_flat_list(workspaces);
	}
	workspaces = create_list();

	uint32_t len = 0;
	char *res = ipc_single_command(socketfd, IPC_GET_WORKSPACES, NULL, &len);
	json_object *results = json_tokener_parse(res);

	int i;
	for (i = 0; i < json_object_array_length(results); ++i) {
		json_object *ws_json = json_object_array_get_idx(results, i);
		json_object *num, *name, *visible, *focused, *out, *urgent;
		json_object_object_get_ex(ws_json, "num", &num);
		json_object_object_get_ex(ws_json, "name", &name);
		json_object_object_get_ex(ws_json, "visible", &visible);
		json_object_object_get_ex(ws_json, "focused", &focused);
		json_object_object_get_ex(ws_json, "output", &out);
		json_object_object_get_ex(ws_json, "urgent", &urgent);

		if (strcmp(json_object_get_string(out), output) == 0) {
			struct workspace *ws = malloc(sizeof(struct workspace));
			ws->num = json_object_get_int(num);
			ws->name = strdup(json_object_get_string(name));
			ws->visible = json_object_get_boolean(visible);
			ws->focused = json_object_get_boolean(focused);
			ws->urgent = json_object_get_boolean(urgent);
			list_add(workspaces, ws);
			sway_log(L_INFO, "Handling workspace %s", ws->name);
		}

		json_object_put(num);
		json_object_put(name);
		json_object_put(visible);
		json_object_put(focused);
		json_object_put(out);
		json_object_put(urgent);
		json_object_put(ws_json);
	}

	json_object_put(results);
	free(res);
}

void bar_ipc_init(int outputi) {
	uint32_t len = 0;
	char *res = ipc_single_command(socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	json_object *info = json_object_array_get_idx(outputs, outputi);
	json_object *name;
	json_object_object_get_ex(info, "name", &name);
	output = strdup(json_object_get_string(name));
	free(res);
	json_object_put(outputs);
	sway_log(L_INFO, "Running on output %s", output);

	const char *subscribe_json = "[ \"workspace\" ]";
	len = strlen(subscribe_json);
	res = ipc_single_command(socketfd, IPC_SUBSCRIBE, subscribe_json, &len);
	sway_log(L_INFO, "%s", res);

	ipc_update_workspaces();
}

void update() {
	int pending;
	if (ioctl(fileno(command), FIONREAD, &pending) != -1 && pending > 0) {
		free(line);
		line = read_line(command);
		int l = strlen(line) - 1;
		if (line[l] == '\n') {
			line[l] = '\0';
		}
	}
	if (ioctl(socketfd, FIONREAD, &pending) != -1 && pending > 0) {
		sway_log(L_INFO, "data available");
		uint32_t len;
		char *buf = ipc_recv_response(socketfd, &len);
		sway_log(L_INFO, "%s", buf);
		free(buf);
		ipc_update_workspaces();
	}
}

void render() {
	// Clear
	cairo_save(window->cairo);
	cairo_set_operator(window->cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(window->cairo);
	cairo_restore(window->cairo);

	// Background
	cairo_set_source_u32(window->cairo, colors.background);
	cairo_paint(window->cairo);

	// Command output
	cairo_set_source_u32(window->cairo, colors.statusline);
	int width, height;
	get_text_size(window, &width, &height, "%s", line);

	cairo_move_to(window->cairo, window->width - MARGIN - width, MARGIN);
	pango_printf(window, "%s", line);

	// Workspaces
	int x = 0;
	int i;
	for (i = 0; i < workspaces->length; ++i) {
		struct workspace *ws = workspaces->items[i];
		get_text_size(window, &width, &height, "%s", ws->name);
		struct box_colors box_colors;
		if (ws->urgent) {
			box_colors = colors.urgent_workspace;
		} else if (ws->focused) {
			box_colors = colors.focused_workspace;
		} else if (ws->visible) {
			box_colors = colors.active_workspace;
		} else {
			box_colors = colors.inactive_workspace;
		}
		cairo_set_source_u32(window->cairo, box_colors.background);
		cairo_rectangle(window->cairo, x, 0, width + MARGIN * 2, window->height);
		cairo_fill(window->cairo);

		cairo_set_source_u32(window->cairo, box_colors.border);
		cairo_rectangle(window->cairo, x, 0, width + MARGIN * 2, window->height);
		cairo_stroke(window->cairo);

		cairo_set_source_u32(window->cairo, box_colors.text);
		cairo_move_to(window->cairo, x + MARGIN, MARGIN);
		pango_printf(window, "%s", ws->name);

		x += width + MARGIN * 2 + MARGIN;
	}
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	registry = registry_poll();

	if (!registry->desktop_shell) {
		sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
	}

	int desired_output = atoi(argv[1]);
	sway_log(L_INFO, "Using output %d of %d", desired_output, registry->outputs->length);

	struct output_state *output = registry->outputs->items[desired_output];
	window = window_setup(registry, output->width, 30, false);
	if (!window) {
		sway_abort("Failed to create window.");
	}
	desktop_shell_set_panel(registry->desktop_shell, output->output, window->surface);
	desktop_shell_set_panel_position(registry->desktop_shell, DESKTOP_SHELL_PANEL_POSITION_BOTTOM);

	int width, height;
	get_text_size(window, &width, &height, "Test string for measuring purposes");
	window->height = height + MARGIN * 2;

	command = popen(argv[2], "r");
	line = malloc(1024);
	line[0] = '\0';

	char *socket_path = get_socketpath();
	if (!socket_path) {
		sway_abort("Unable to retrieve socket path");
	}
	socketfd = ipc_open_socket(socket_path);
	bar_ipc_init(desired_output);

	do {
		if (window_prerender(window) && window->cairo) {
			update();
			render();
			window_render(window);
		}
	} while (wl_display_dispatch(registry->display) != -1);

	window_teardown(window);
	registry_teardown(registry);

	return 0;
}
