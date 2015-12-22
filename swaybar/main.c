#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>
#include <json-c/json.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include "ipc-client.h"
#include "client/registry.h"
#include "client/window.h"
#include "client/pango.h"
#include "stringop.h"
#include "log.h"

struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

struct colors {
	uint32_t background;
	uint32_t statusline;
	uint32_t separator;

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

struct status_block {
	char *full_text, *short_text, *align;
	bool urgent;
	uint32_t color;
	int min_width;
	char *name, *instance;
	bool separator;
	int separator_block_width;
};

list_t *status_line = NULL;

list_t *workspaces = NULL;
int socketfd;
pid_t pid;
int pipefd[2];
FILE *command;
char line[1024];
char *output, *status_command;
struct registry *registry;
struct window *window;
bool dirty = true;

struct colors colors = {
	.background = 0x000000FF,
	.statusline = 0xFFFFFFFF,
	.separator = 0x666666FF,

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

void swaybar_teardown() {
	window_teardown(window);
	if (registry) {
		registry_teardown(registry);
	}

	if (command) {
		fclose(command);
	}

	if (pid) {
		// terminate status_command process
		int ret = kill(pid, SIGTERM);
		if (ret != 0) {
			sway_log(L_ERROR, "Unable to terminate status_command [pid: %d]", pid);
		} else {
			int status;
			waitpid(pid, &status, 0);
		}
	}

	if (pipefd[0]) {
		close(pipefd[0]);
	}
}


void sway_terminate(void) {
	swaybar_teardown();
	exit(EXIT_FAILURE);
}

void sig_handler(int signal) {
	swaybar_teardown();
	exit(0);
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
	if (!results) {
		free(res);
		return;
	}

	int i;
	for (i = 0; i < json_object_array_length(results); ++i) {
		json_object *ws_json = json_object_array_get_idx(results, i);
		json_object *num, *name, *visible, *focused, *out, *urgent;
		if (!ws_json) {
			return;
		}
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
		}
	}

	json_object_put(results);
	free(res);
}

uint32_t parse_color(const char *color) {
	if (color[0] != '#') {
		sway_abort("Invalid color %s", color);
	}
	char *end;
	uint32_t res = (uint32_t)strtol(color + 1, &end, 16);
	if (strlen(color) == 7) {
		res = (res << 8) | 0xFF;
	}
	return res;
}

uint32_t parse_position(const char *position) {
	if (strcmp("top", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_TOP;
	} else if (strcmp("bottom", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	} else if (strcmp("left", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_LEFT;
	} else if (strcmp("right", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_RIGHT;
	} else {
		return DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	}
}

char *parse_font(const char *font) {
	char *new_font = NULL;
	if (strncmp("pango:", font, 6) == 0) {
		new_font = strdup(font + 6);
	}

	return new_font;
}

static int margin = 3;
static const int ws_hor_padding = 5;
static double ws_ver_padding = 1.5;
static const int ws_spacing = 1;

void bar_ipc_init(int outputi, const char *bar_id) {
	uint32_t len = 0;
	char *res = ipc_single_command(socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	json_object *info = json_object_array_get_idx(outputs, outputi);
	json_object *name;
	json_object_object_get_ex(info, "name", &name);
	output = strdup(json_object_get_string(name));
	free(res);
	json_object_put(outputs);

	len = strlen(bar_id);
	res = ipc_single_command(socketfd, IPC_GET_BAR_CONFIG, bar_id, &len);

	json_object *bar_config = json_tokener_parse(res);
	json_object *tray_output, *mode, *hidden_state, *position, *_status_command;
	json_object *font, *bar_height, *workspace_buttons, *strip_workspace_numbers;
	json_object *binding_mode_indicator, *verbose, *_colors;
	json_object_object_get_ex(bar_config, "tray_output", &tray_output);
	json_object_object_get_ex(bar_config, "mode", &mode);
	json_object_object_get_ex(bar_config, "hidden_state", &hidden_state);
	json_object_object_get_ex(bar_config, "position", &position);
	json_object_object_get_ex(bar_config, "status_command", &_status_command);
	json_object_object_get_ex(bar_config, "font", &font);
	json_object_object_get_ex(bar_config, "bar_height", &bar_height);
	json_object_object_get_ex(bar_config, "workspace_buttons", &workspace_buttons);
	json_object_object_get_ex(bar_config, "strip_workspace_numbers", &strip_workspace_numbers);
	json_object_object_get_ex(bar_config, "binding_mode_indicator", &binding_mode_indicator);
	json_object_object_get_ex(bar_config, "verbose", &verbose);
	json_object_object_get_ex(bar_config, "colors", &_colors);

	// TODO: More of these options
	// TODO: Refactor swaybar into several files, create a bar config struct (shared with compositor?)
	if (_status_command) {
		status_command = strdup(json_object_get_string(_status_command));
	}

	if (position) {
		desktop_shell_set_panel_position(registry->desktop_shell, parse_position(json_object_get_string(position)));
	}

	if (font) {
		window->font = parse_font(json_object_get_string(font));
	}

	if (bar_height) {
		int width, height;
		get_text_size(window, &width, &height, "Test string for measuring purposes");
		int bar_height_value = json_object_get_int(bar_height);
		if (bar_height_value > 0) {
			margin = (bar_height_value - height) / 2;
			ws_ver_padding = margin - 1.5;
		}
		window->height = height + margin * 2;
	}

	if (_colors) {
		json_object *background, *statusline, *separator;
		json_object *focused_workspace_border, *focused_workspace_bg, *focused_workspace_text;
		json_object *inactive_workspace_border, *inactive_workspace_bg, *inactive_workspace_text;
		json_object *active_workspace_border, *active_workspace_bg, *active_workspace_text;
		json_object *urgent_workspace_border, *urgent_workspace_bg, *urgent_workspace_text;
		json_object *binding_mode_border, *binding_mode_bg, *binding_mode_text;
		json_object_object_get_ex(_colors, "background", &background);
		json_object_object_get_ex(_colors, "statusline", &statusline);
		json_object_object_get_ex(_colors, "separator", &separator);
		json_object_object_get_ex(_colors, "focused_workspace_border", &focused_workspace_border);
		json_object_object_get_ex(_colors, "focused_workspace_bg", &focused_workspace_bg);
		json_object_object_get_ex(_colors, "focused_workspace_text", &focused_workspace_text);
		json_object_object_get_ex(_colors, "active_workspace_border", &active_workspace_border);
		json_object_object_get_ex(_colors, "active_workspace_bg", &active_workspace_bg);
		json_object_object_get_ex(_colors, "active_workspace_text", &active_workspace_text);
		json_object_object_get_ex(_colors, "inactive_workspace_border", &inactive_workspace_border);
		json_object_object_get_ex(_colors, "inactive_workspace_bg", &inactive_workspace_bg);
		json_object_object_get_ex(_colors, "inactive_workspace_text", &inactive_workspace_text);
		json_object_object_get_ex(_colors, "urgent_workspace_border", &urgent_workspace_border);
		json_object_object_get_ex(_colors, "urgent_workspace_bg", &urgent_workspace_bg);
		json_object_object_get_ex(_colors, "urgent_workspace_text", &urgent_workspace_text);
		json_object_object_get_ex(_colors, "binding_mode_border", &binding_mode_border);
		json_object_object_get_ex(_colors, "binding_mode_bg", &binding_mode_bg);
		json_object_object_get_ex(_colors, "binding_mode_text", &binding_mode_text);
		if (background) colors.background = parse_color(json_object_get_string(background));
		if (statusline) colors.statusline = parse_color(json_object_get_string(statusline));
		if (separator) colors.separator = parse_color(json_object_get_string(separator));
		if (focused_workspace_border) {
			colors.focused_workspace.border = parse_color(json_object_get_string(focused_workspace_border));
		}
		if (focused_workspace_bg) {
			colors.focused_workspace.background = parse_color(json_object_get_string(focused_workspace_bg));
		}
		if (focused_workspace_text) {
			colors.focused_workspace.text = parse_color(json_object_get_string(focused_workspace_text));
		}
		if (active_workspace_border) {
			colors.active_workspace.border = parse_color(json_object_get_string(active_workspace_border));
		}
		if (active_workspace_bg) {
			colors.active_workspace.background = parse_color(json_object_get_string(active_workspace_bg));
		}
		if (active_workspace_text) {
			colors.active_workspace.text = parse_color(json_object_get_string(active_workspace_text));
		}
		if (inactive_workspace_border) {
			colors.inactive_workspace.border = parse_color(json_object_get_string(inactive_workspace_border));
		}
		if (inactive_workspace_bg) {
			colors.inactive_workspace.background = parse_color(json_object_get_string(inactive_workspace_bg));
		}
		if (inactive_workspace_text) {
			colors.inactive_workspace.text = parse_color(json_object_get_string(inactive_workspace_text));
		}
		if (binding_mode_border) {
			colors.binding_mode.border = parse_color(json_object_get_string(binding_mode_border));
		}
		if (binding_mode_bg) {
			colors.binding_mode.background = parse_color(json_object_get_string(binding_mode_bg));
		}
		if (binding_mode_text) {
			colors.binding_mode.text = parse_color(json_object_get_string(binding_mode_text));
		}
	}

	json_object_put(bar_config);
	free(res);

	const char *subscribe_json = "[ \"workspace\" ]";
	len = strlen(subscribe_json);
	res = ipc_single_command(socketfd, IPC_SUBSCRIBE, subscribe_json, &len);

	ipc_update_workspaces();
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

	if (status_line) {
		int i;
		int moved = 0;
		for ( i = status_line->length - 1; i >= 0; --i ) {
			struct status_block *block = status_line->items[i];
			if (block->full_text) {
				get_text_size(window, &width, &height, "%s", block->full_text);
				moved += width + block->separator_block_width;
				cairo_move_to(window->cairo, window->width - margin - moved, margin);
				cairo_set_source_u32(window->cairo, block->color);
				pango_printf(window, "%s", block->full_text);
			}
		}
	}

	// Workspaces
	cairo_set_line_width(window->cairo, 1.0);
	double x = 0.5;
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
		cairo_rectangle(window->cairo, x, 1.5, width + ws_hor_padding * 2 - 1, height + ws_ver_padding * 2);
		cairo_fill(window->cairo);

		cairo_set_source_u32(window->cairo, box_colors.border);
		cairo_rectangle(window->cairo, x, 1.5, width + ws_hor_padding * 2 - 1, height + ws_ver_padding * 2);
		cairo_stroke(window->cairo);

		cairo_set_source_u32(window->cairo, box_colors.text);
		cairo_move_to(window->cairo, (int)x + ws_hor_padding, margin);
		pango_printf(window, "%s", ws->name);

		x += width + ws_hor_padding * 2 + ws_spacing;
	}
}

void parse_json(const char *text) {
/* the array of objects looks like this:
 * [ {
 *     "full_text": "E: 10.0.0.1 (1000 Mbit/s)",
 *     "short_text": "10.0.0.1",
 *     "color": "#00ff00",
 *     "min_width": 300,
 *     "align": "right",
 *     "urgent": false,
 *     "name": "ethernet",
 *     "instance": "eth0",
 *     "separator": true,
 *     "separator_block_width": 9
 *   },
 *   { ... }, ...
 * ]
 */
	json_object *results = json_tokener_parse(text);
	if (!results) {
		sway_log(L_DEBUG, "xxx Failed to parse json");
		return;
	}

	if (json_object_array_length(results) < 1) {
		return;
	}

	if (status_line) {
		free_flat_list(status_line);
	}

	status_line = create_list();

	int i;
	for (i = 0; i < json_object_array_length(results); ++i) {
		json_object *full_text, *short_text, *color, *min_width, *align, *urgent;
		json_object *name, *instance, *separator, *separator_block_width;

		json_object *json = json_object_array_get_idx(results, i);
		if (!json) {
			continue;
		}

		json_object_object_get_ex(json, "full_text", &full_text);
		json_object_object_get_ex(json, "short_text", &short_text);
		json_object_object_get_ex(json, "color", &color);
		json_object_object_get_ex(json, "min_width", &min_width);
		json_object_object_get_ex(json, "align", &align);
		json_object_object_get_ex(json, "urgent", &urgent);
		json_object_object_get_ex(json, "name", &name);
		json_object_object_get_ex(json, "instance", &instance);
		json_object_object_get_ex(json, "separator", &separator);
		json_object_object_get_ex(json, "separator_block_width", &separator_block_width);

		struct status_block *new = malloc(sizeof(struct status_block));
		memset(new, 0, sizeof(struct status_block));

		if (full_text) {
			new->full_text = strdup(json_object_get_string(full_text));
		}

		if (short_text) {
			new->short_text = strdup(json_object_get_string(short_text));
		}

		if (color) {
			new->color = parse_color(json_object_get_string(color));
		}
		else {
			new->color = 0xFFFFFFFF;
		}

		if (min_width) {
			new->min_width = json_object_get_int(min_width);
		}

		if (align) {
			new->align = strdup(json_object_get_string(align));
		}
		else {
			new->align = strdup("left");
		}

		if (urgent) {
			new->urgent = json_object_get_int(urgent);
		}

		if (name) {
			new->name = strdup(json_object_get_string(name));
		}

		if (instance) {
			new->instance = strdup(json_object_get_string(instance));
		}

		if (separator) {
			new->separator = json_object_get_int(separator);
		}
		else {
			new->separator = true; // i3bar spec
		}

		if (separator_block_width) {
			new->separator_block_width = json_object_get_int(separator_block_width);
		}
		else {
			new->separator_block_width = 9; // i3bar spec
		}

		list_add(status_line, new);
	}

	json_object_put(results);
}

void poll_for_update() {
	fd_set readfds;
	int activity;

	while (1) {
		if (dirty && window_prerender(window) && window->cairo) {
			render();
			window_render(window);
		}

		if (wl_display_dispatch(registry->display) == -1) {
			break;
		}

		dirty = false;
		FD_ZERO(&readfds);
		FD_SET(socketfd, &readfds);
		FD_SET(pipefd[0], &readfds);

		activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			sway_log(L_ERROR, "polling failed: %d", errno);
		}

		if (FD_ISSET(socketfd, &readfds)) {
			sway_log(L_DEBUG, "Got workspace update.");
			uint32_t len;
			char *buf = ipc_recv_response(socketfd, &len);
			free(buf);
			ipc_update_workspaces();
			dirty = true;
		}

		if (status_command && FD_ISSET(pipefd[0], &readfds)) {
			sway_log(L_DEBUG, "Got update from status command.");
			fgets(line, sizeof(line), command);
			sway_log(L_DEBUG, "zzz %s", line);
			int l = strlen(line) - 1;
			if (line[l] == '\n') {
				line[l] = '\0';
			}
			if (line[0] == ',') {
				line[0] = ' ';
			}
			dirty = true;
			parse_json(line);
		}
	}
}

int main(int argc, char **argv) {
	init_log(L_DEBUG);

	char *socket_path = NULL;
	char *bar_id = NULL;

	static struct option long_options[] = {
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"bar_id", required_argument, NULL, 'b'},
		{0, 0, 0, 0}
	};

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "vs:b:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 'b': // Type
			bar_id = strdup(optarg);
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version not detected\n");
#endif
			exit(EXIT_SUCCESS);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	if (!bar_id) {
		sway_abort("No bar_id passed. Provide --bar_id or let sway start swaybar");
	}

	registry = registry_poll();

	if (!registry->desktop_shell) {
		sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}
	socketfd = ipc_open_socket(socket_path);

	if (argc == optind) {
		sway_abort("No output index provided");
	}

	int desired_output = atoi(argv[optind]);
	struct output_state *output = registry->outputs->items[desired_output];

	window = window_setup(registry, output->width, 30, false);
	if (!window) {
		sway_abort("Failed to create window.");
	}
	desktop_shell_set_panel(registry->desktop_shell, output->output, window->surface);

	bar_ipc_init(desired_output, bar_id);

	if (status_command) {
		pipe(pipefd);
		pid = fork();
		if (pid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[1]);
			char *const cmd[] = {
				"sh",
				"-c",
				status_command,
				NULL,
			};
			execvp(cmd[0], cmd);
			return 0;
		}

		close(pipefd[1]);
		command = fdopen(pipefd[0], "r");
		line[0] = '\0';
	}

	signal(SIGTERM, sig_handler);

	poll_for_update();

	// gracefully shutdown swaybar and status_command
	swaybar_teardown();

	return 0;
}
