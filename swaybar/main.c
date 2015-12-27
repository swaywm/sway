#include <fcntl.h>
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
	// Airblader features
	uint32_t background;
	uint32_t border;
	int border_top;
	int border_bottom;
	int border_left;
	int border_right;
};

list_t *status_line = NULL;

list_t *workspaces = NULL;
int ipc_socketfd, ipc_listen_socketfd;
pid_t pid;
int status_read_fd;
char line[1024];
char line_rest[1024];
char *output, *status_command;
struct registry *registry;
struct window *window;
bool dirty = true;
typedef enum {UNDEF, TEXT, I3BAR} command_protocol;
command_protocol protocol = UNDEF;

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

#define I3JSON_MAXDEPTH 4
#define I3JSON_UNKNOWN 0
#define I3JSON_ARRAY 1
#define I3JSON_STRING 2
struct {
	int bufsize;
	char *buffer;
	char *line_start;
	char *parserpos;
	bool escape;
	int depth;
	int state[I3JSON_MAXDEPTH+1];
} i3json_state = { 0, NULL, NULL, NULL, false, 0, { I3JSON_UNKNOWN } };


void swaybar_teardown() {
	window_teardown(window);
	if (registry) {
		registry_teardown(registry);
	}

	if (status_read_fd) {
		close(status_read_fd);
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

	if (status_read_fd) {
		close(status_read_fd);
	}

	if (ipc_socketfd) {
		close(ipc_socketfd);
	}

	if (ipc_listen_socketfd) {
		close(ipc_listen_socketfd);
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

void free_workspace(void *item) {
	if (!item) {
		return;
	}
	struct workspace *ws = (struct workspace *)item;
	if (ws->name) {
		free(ws->name);
	}
	free(ws);
}

void ipc_update_workspaces() {
	if (workspaces) {
		list_foreach(workspaces, free_workspace);
		list_free(workspaces);
	}
	workspaces = create_list();

	uint32_t len = 0;
	char *res = ipc_single_command(ipc_socketfd, IPC_GET_WORKSPACES, NULL, &len);
	json_object *results = json_tokener_parse(res);
	if (!results) {
		free(res);
		return;
	}

	int i;
	int length = json_object_array_length(results);
	json_object *ws_json;
	json_object *num, *name, *visible, *focused, *out, *urgent;
	for (i = 0; i < length; ++i) {
		ws_json = json_object_array_get_idx(results, i);

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
	char *res = ipc_single_command(ipc_socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	json_object *info = json_object_array_get_idx(outputs, outputi);
	json_object *name;
	json_object_object_get_ex(info, "name", &name);
	output = strdup(json_object_get_string(name));
	free(res);
	json_object_put(outputs);

	len = strlen(bar_id);
	res = ipc_single_command(ipc_socketfd, IPC_GET_BAR_CONFIG, bar_id, &len);

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
	res = ipc_single_command(ipc_listen_socketfd, IPC_SUBSCRIBE, subscribe_json, &len);

	ipc_update_workspaces();
}

/**
 * Renders a sharp line of any width and height.
 *
 * The line is drawn from (x,y) to (x+width,y+height) where width/height is 0
 * if the line has a width/height of one pixel, respectively.
 */
void render_sharp_line(cairo_t *cairo, uint32_t color, double x, double y, double width, double height) {
	cairo_set_source_u32(cairo, color);

	if (width > 1 && height > 1) {
		cairo_rectangle(cairo, x, y, width, height);
		cairo_fill(cairo);
	} else {
		if (width == 1) {
			x += 0.5;
			height += y;
			width = x;
		}

		if (height == 1) {
			y += 0.5;
			width += x;
			height = y;
		}

		cairo_move_to(cairo, x, y);
		cairo_set_line_width(cairo, 1.0);
		cairo_line_to(cairo, width, height);
		cairo_stroke(cairo);
	}
}

void render_block(struct status_block *block, double *x, bool edge) {
	int width, height;
	get_text_size(window, &width, &height, "%s", block->full_text);

	int textwidth = width;
	double block_width = width;

	if (width < block->min_width) {
		width = block->min_width;
	}

	*x -= width;

	if (block->border != 0 && block->border_left > 0) {
		*x -= (block->border_left + margin);
		block_width += block->border_left + margin;
	}

	if (block->border != 0 && block->border_right > 0) {
		*x -= (block->border_right + margin);
		block_width += block->border_right + margin;
	}

	// Add separator
	if (!edge) {
		*x -= block->separator_block_width;
	} else {
		*x -= margin;
	}

	double pos = *x;

	// render background
	if (block->background != 0x0) {
		cairo_set_source_u32(window->cairo, block->background);
		cairo_rectangle(window->cairo, pos - 0.5, 1, block_width, window->height - 2);
		cairo_fill(window->cairo);
	}

	// render top border
	if (block->border != 0 && block->border_top > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block_width,
				block->border_top);
	}

	// render bottom border
	if (block->border != 0 && block->border_bottom > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				window->height - 1 - block->border_bottom,
				block_width,
				block->border_bottom);
	}

	// render left border
	if (block->border != 0 && block->border_left > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block->border_left,
				window->height - 2);

		pos += block->border_left + margin;
	}

	// render text
	double offset = 0;

	if (strncmp(block->align, "left", 5) == 0) {
		offset = pos;
	} else if (strncmp(block->align, "right", 5) == 0) {
		offset = pos + width - textwidth;
	} else if (strncmp(block->align, "center", 6) == 0) {
		offset = pos + (width - textwidth) / 2;
	}

	cairo_move_to(window->cairo, offset, margin);
	cairo_set_source_u32(window->cairo, block->color);
	pango_printf(window, "%s", block->full_text);

	pos += width;

	// render right border
	if (block->border != 0 && block->border_right > 0) {
		pos += margin;

		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block->border_right,
				window->height - 2);

		pos += block->border_right;
	}

	// render separator
	// TODO: Handle custom separator
	if (!edge && block->separator) {
		cairo_set_source_u32(window->cairo, colors.separator);
		cairo_set_line_width(window->cairo, 1);
		cairo_move_to(window->cairo, pos + block->separator_block_width/2, margin);
		cairo_line_to(window->cairo, pos + block->separator_block_width/2, window->height - margin);
		cairo_stroke(window->cairo);
	}

}

void render_workspace_button(struct workspace *ws, double *x) {
	int width, height;
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

	// background
	cairo_set_source_u32(window->cairo, box_colors.background);
	cairo_rectangle(window->cairo, *x, 1.5, width + ws_hor_padding * 2 - 1,
			height + ws_ver_padding * 2);
	cairo_fill(window->cairo);

	// border
	cairo_set_source_u32(window->cairo, box_colors.border);
	cairo_rectangle(window->cairo, *x, 1.5, width + ws_hor_padding * 2 - 1,
			height + ws_ver_padding * 2);
	cairo_stroke(window->cairo);

	// text
	cairo_set_source_u32(window->cairo, box_colors.text);
	cairo_move_to(window->cairo, (int)*x + ws_hor_padding, margin);
	pango_printf(window, "%s", ws->name);

	*x += width + ws_hor_padding * 2 + ws_spacing;
}

void render() {
	int i;

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

	if (protocol == TEXT) {
		get_text_size(window, &width, &height, "%s", line);
		cairo_move_to(window->cairo, window->width - margin - width, margin);
		pango_printf(window, "%s", line);
	} else if (protocol == I3BAR && status_line) {
		double pos = window->width - 0.5;
		bool edge = true;
		for (i = status_line->length - 1; i >= 0; --i) {
			struct status_block *block = status_line->items[i];
			if (block->full_text && block->full_text[0]) {
				render_block(block, &pos, edge);
				edge = false;
			}
		}
	}

	// Workspaces
	cairo_set_line_width(window->cairo, 1.0);
	double x = 0.5;
	for (i = 0; i < workspaces->length; ++i) {
		struct workspace *ws = workspaces->items[i];
		render_workspace_button(ws, &x);
	}
}

void free_status_block(void *item) {
	if (!item) {
		return;
	}
	struct status_block *sb = (struct status_block*)item;
	if (sb->full_text) {
		free(sb->full_text);
	}
	if (sb->short_text) {
		free(sb->short_text);
	}
	if (sb->align) {
		free(sb->align);
	}
	if (sb->name) {
		free(sb->name);
	}
	if (sb->instance) {
		free(sb->instance);
	}
	free(sb);
}

void parse_json(const char *text) {
	json_object *results = json_tokener_parse(text);
	if (!results) {
		sway_log(L_DEBUG, "xxx Failed to parse json");
		return;
	}

	if (json_object_array_length(results) < 1) {
		return;
	}

	if (status_line) {
		list_foreach(status_line, free_status_block);
		list_free(status_line);
	}

	status_line = create_list();

	int i;
	for (i = 0; i < json_object_array_length(results); ++i) {
		json_object *full_text, *short_text, *color, *min_width, *align, *urgent;
		json_object *name, *instance, *separator, *separator_block_width;
		json_object *background, *border, *border_top, *border_bottom;
		json_object *border_left, *border_right;

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
		json_object_object_get_ex(json, "background", &background);
		json_object_object_get_ex(json, "border", &border);
		json_object_object_get_ex(json, "border_top", &border_top);
		json_object_object_get_ex(json, "border_bottom", &border_bottom);
		json_object_object_get_ex(json, "border_left", &border_left);
		json_object_object_get_ex(json, "border_right", &border_right);

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
			new->color = colors.statusline;
		}

		if (min_width) {
			json_type type = json_object_get_type(min_width);
			if (type == json_type_int) {
				new->min_width = json_object_get_int(min_width);
			}
			else if (type == json_type_string) {
				int width, height;
				get_text_size(window, &width, &height, "%s", json_object_get_string(min_width));
				new->min_width = width;
			}
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

		// Airblader features
		if (background) {
			new->background = parse_color(json_object_get_string(background));
		} else {
			new->background = 0x0; // transparent
		}

		if (border) {
			new->border = parse_color(json_object_get_string(border));
		} else {
			new->border = 0x0; // transparent
		}

		if (border_top) {
			new->border_top = json_object_get_int(border_top);
		} else {
			new->border_top = 1;
		}

		if (border_bottom) {
			new->border_bottom = json_object_get_int(border_bottom);
		} else {
			new->border_bottom = 1;
		}

		if (border_left) {
			new->border_left = json_object_get_int(border_left);
		} else {
			new->border_left = 1;
		}

		if (border_right) {
			new->border_right = json_object_get_int(border_right);
		} else {
			new->border_right = 1;
		}

		list_add(status_line, new);
	}

	json_object_put(results);
}

// Read line from file descriptor, only show the line tail if it is too long.
// In non-blocking mode treat "no more data" as a linebreak.
// If data after a line break has been read, return it in rest.
// If rest is non-empty, then use that as the start of the next line.
int read_line_tail(int fd, char *buf, int nbyte, char *rest) {
	if (fd < 0 || !buf || !nbyte) {
		return -1;
	}
	int l;
	char *buffer = malloc(nbyte*2+1);
	char *readpos = buffer;
	char *lf;
	// prepend old data to new line if necessary
	if (rest) {
		l = strlen(rest);
		if (l > nbyte) {
			strcpy(buffer, rest + l - nbyte);
			readpos += nbyte;
		} else if (l) {
			strcpy(buffer, rest);
			readpos += l;
		}
	}
	// read until a linefeed is found or no more data is available
	while ((l = read(fd, readpos, nbyte)) > 0) {
		readpos[l] = '\0';
		lf = strchr(readpos, '\n');
		if (lf) {
			// linefeed found, replace with \0
			*lf = '\0';
			// give data from the end of the line, try to fill the buffer
			if (lf-buffer > nbyte) {
				strcpy(buf, lf - nbyte + 1);
			} else {
				strcpy(buf, buffer);
			}
			// we may have read data from the next line, save it to rest
			if (rest) {
				rest[0] = '\0';
				strcpy(rest, lf + 1);
			}
			free(buffer);
			return strlen(buf);
		} else {
			// no linefeed found, slide data back.
			int overflow = readpos - buffer + l - nbyte;
			if (overflow > 0) {
				memmove(buffer, buffer + overflow , nbyte + 1);
			}
		}
	}
	if (l < 0) {
		free(buffer);
		return l;
	}
	readpos[l]='\0';
	if (rest) {
		rest[0] = '\0';
	}
	if (nbyte < readpos - buffer + l - 1) {
		memcpy(buf, readpos - nbyte + l + 1, nbyte);
	} else {
		strncpy(buf, buffer, nbyte);
	}
	buf[nbyte-1] = '\0';
	free(buffer);
	return strlen(buf);
}

// make sure that enough buffer space is available starting from parserpos
void i3json_ensure_free(int min_free) {
	int _step = 10240;
	int r = min_free % _step;
	if (r) {
		min_free += _step - r;
	}
	if (!i3json_state.buffer) {
		i3json_state.buffer = malloc(min_free);
		i3json_state.bufsize = min_free;
		i3json_state.parserpos = i3json_state.buffer;
	} else {
		int len = 0;
		int pos = 0;
		if (i3json_state.line_start) {
			len = strlen(i3json_state.line_start);
			pos = i3json_state.parserpos - i3json_state.line_start;
			if (i3json_state.line_start != i3json_state.buffer) {
				memmove(i3json_state.buffer, i3json_state.line_start, len+1);
			}
		} else {
			len = strlen(i3json_state.buffer);
		}
		if (i3json_state.bufsize < len+min_free) {
			i3json_state.bufsize += min_free;
			if (i3json_state.bufsize > 1024000) {
				sway_abort("Status line json too long or malformed.");
			}
			i3json_state.buffer = realloc(i3json_state.buffer, i3json_state.bufsize);
			if (!i3json_state.buffer) {
				sway_abort("Could not allocate json buffer");
			}
		}
		if (i3json_state.line_start) {
			i3json_state.line_start = i3json_state.buffer;
			i3json_state.parserpos = i3json_state.buffer + pos;
		} else {
			i3json_state.parserpos = i3json_state.buffer;
		}
	}
	if (!i3json_state.buffer) {
		sway_abort("Could not allocate buffer.");
	}
}

// continue parsing from last parserpos
int i3json_parse() {
	char *c = i3json_state.parserpos;
	int handled = 0;
	while (*c) {
		if (i3json_state.state[i3json_state.depth] == I3JSON_STRING) {
			if (!i3json_state.escape && *c == '"') {
				--i3json_state.depth;
			}
			i3json_state.escape = !i3json_state.escape && *c == '\\';
		} else {
			switch (*c) {
			case '[':
				++i3json_state.depth;
				if (i3json_state.depth > I3JSON_MAXDEPTH) {
					sway_abort("JSON too deep");
				}
				i3json_state.state[i3json_state.depth] = I3JSON_ARRAY;
				if (i3json_state.depth == 2) {
					i3json_state.line_start = c;
				}
				break;
			case ']':
				if (i3json_state.state[i3json_state.depth] != I3JSON_ARRAY) {
					sway_abort("JSON malformed");
				}
				--i3json_state.depth;
				if (i3json_state.depth == 1) {
					// c[1] is valid since c[0] != '\0'
					char p = c[1];
					c[1] = '\0';
					parse_json(i3json_state.line_start);
					c[1] = p;
					++handled;
					i3json_state.line_start = c+1;
				}
				break;
			case '"':
				++i3json_state.depth;
				if (i3json_state.depth > I3JSON_MAXDEPTH) {
					sway_abort("JSON too deep");
				}
				i3json_state.state[i3json_state.depth] = I3JSON_STRING;
				break;
			}
		}
		++c;
	}
	i3json_state.parserpos = c;
	return handled;
}

// append data and parse it.
int i3json_handle_data(char *data) {
	int len = strlen(data);
	i3json_ensure_free(len);
	strcpy(i3json_state.parserpos, data);
	return i3json_parse();
}

// read data from fd and parse it.
int i3json_handle_fd(int fd) {
	i3json_ensure_free(10240);
	// get fresh data at the end of the buffer
	int readlen = read(fd, i3json_state.parserpos, 10239);
	if (readlen < 0) {
		return readlen;
	}
	i3json_state.parserpos[readlen] = '\0';
	return i3json_parse();
}

void poll_for_update() {
	fd_set readfds;
	int activity;

	while (1) {
		if (dirty && window_prerender(window) && window->cairo) {
			render();
			window_render(window);
			if (wl_display_dispatch(registry->display) == -1) {
				break;
			}
		}

		dirty = false;
		FD_ZERO(&readfds);
		FD_SET(ipc_listen_socketfd, &readfds);
		FD_SET(status_read_fd, &readfds);

		activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			sway_log(L_ERROR, "polling failed: %d", errno);
		}

		if (FD_ISSET(ipc_listen_socketfd, &readfds)) {
			sway_log(L_DEBUG, "Got workspace update.");
			uint32_t len;
			char *buf = ipc_recv_response(ipc_listen_socketfd, &len);
			free(buf);
			ipc_update_workspaces();
			dirty = true;
		}

		if (status_command && FD_ISSET(status_read_fd, &readfds)) {
			sway_log(L_DEBUG, "Got update from status command.");
			switch (protocol) {
			case I3BAR:
				sway_log(L_DEBUG, "Got i3bar protocol.");
				if (i3json_handle_fd(status_read_fd) > 0) {
					dirty = true;
				}
				break;
			case TEXT:
				sway_log(L_DEBUG, "Got text protocol.");
				read_line_tail(status_read_fd, line, sizeof(line), line_rest);
				dirty = true;
				break;
			case UNDEF:
				sway_log(L_DEBUG, "Detecting protocol...");
				if (read_line_tail(status_read_fd, line, sizeof(line), line_rest) < 0) {
					break;
				}
				dirty = true;
				protocol = TEXT;
				if (line[0] == '{') {
					// detect i3bar json protocol
					json_object *proto = json_tokener_parse(line);
					json_object *version;
					if (proto) {
						if (json_object_object_get_ex(proto, "version", &version)
									&& json_object_get_int(version) == 1
						) {
							sway_log(L_DEBUG, "Switched to i3bar protocol.");
							protocol = I3BAR;
							i3json_handle_data(line_rest);
						}
						json_object_put(proto);
					}
				}
				break;
			}
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
	ipc_socketfd = ipc_open_socket(socket_path);
	ipc_listen_socketfd = ipc_open_socket(socket_path);


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
		int pipefd[2];
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
		status_read_fd = pipefd[0];
		fcntl(status_read_fd, F_SETFL, O_NONBLOCK);
		line[0] = '\0';
	}

	signal(SIGTERM, sig_handler);

	poll_for_update();

	// gracefully shutdown swaybar and status_command
	swaybar_teardown();

	return 0;
}
