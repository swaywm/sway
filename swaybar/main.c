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
#include "state.h"
#include "config.h"
#include "render.h"
#include "status_line.h"

char *output;
struct swaybar_state *state;

void swaybar_teardown() {
	window_teardown(state->output->window);
	if (state->output->registry) {
		registry_teardown(state->output->registry);
	}

	if (state->status_read_fd) {
		close(state->status_read_fd);
	}

	if (state->status_command_pid) {
		// terminate status_command process
		int ret = kill(state->status_command_pid, SIGTERM);
		if (ret != 0) {
			sway_log(L_ERROR, "Unable to terminate status_command [pid: %d]", state->status_command_pid);
		} else {
			int status;
			waitpid(state->status_command_pid, &status, 0);
		}
	}

	if (state->status_read_fd) {
		close(state->status_read_fd);
	}

	if (state->ipc_socketfd) {
		close(state->ipc_socketfd);
	}

	if (state->ipc_event_socketfd) {
		close(state->ipc_event_socketfd);
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

void ipc_update_workspaces() {
	if (state->output->workspaces) {
		list_foreach(state->output->workspaces, free_workspace);
		list_free(state->output->workspaces);
	}
	state->output->workspaces = create_list();

	uint32_t len = 0;
	char *res = ipc_single_command(state->ipc_socketfd, IPC_GET_WORKSPACES, NULL, &len);
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
			list_add(state->output->workspaces, ws);
		}
	}

	json_object_put(results);
	free(res);
}

void bar_ipc_init(int outputi, const char *bar_id) {
	uint32_t len = 0;
	char *res = ipc_single_command(state->ipc_socketfd, IPC_GET_OUTPUTS, NULL, &len);
	json_object *outputs = json_tokener_parse(res);
	json_object *info = json_object_array_get_idx(outputs, outputi);
	json_object *name;
	json_object_object_get_ex(info, "name", &name);
	output = strdup(json_object_get_string(name));
	free(res);
	json_object_put(outputs);

	len = strlen(bar_id);
	res = ipc_single_command(state->ipc_socketfd, IPC_GET_BAR_CONFIG, bar_id, &len);

	json_object *bar_config = json_tokener_parse(res);
	json_object *tray_output, *mode, *hidden_state, *position, *_status_command;
	json_object *font, *bar_height, *_workspace_buttons, *_strip_workspace_numbers;
	json_object *_binding_mode_indicator, *verbose, *_colors, *sep_symbol;
	json_object_object_get_ex(bar_config, "tray_output", &tray_output);
	json_object_object_get_ex(bar_config, "mode", &mode);
	json_object_object_get_ex(bar_config, "hidden_state", &hidden_state);
	json_object_object_get_ex(bar_config, "position", &position);
	json_object_object_get_ex(bar_config, "status_command", &_status_command);
	json_object_object_get_ex(bar_config, "font", &font);
	json_object_object_get_ex(bar_config, "bar_height", &bar_height);
	json_object_object_get_ex(bar_config, "workspace_buttons", &_workspace_buttons);
	json_object_object_get_ex(bar_config, "strip_workspace_numbers", &_strip_workspace_numbers);
	json_object_object_get_ex(bar_config, "binding_mode_indicator", &_binding_mode_indicator);
	json_object_object_get_ex(bar_config, "verbose", &verbose);
	json_object_object_get_ex(bar_config, "separator_symbol", &sep_symbol);
	json_object_object_get_ex(bar_config, "colors", &_colors);

	// TODO: More of these options
	// TODO: Refactor swaybar into several files, create a bar config struct (shared with compositor?)
	if (_status_command) {
		free(state->config->status_command);
		state->config->status_command = strdup(json_object_get_string(_status_command));
	}

	if (position) {
		state->config->position = parse_position(json_object_get_string(position));
		desktop_shell_set_panel_position(state->output->registry->desktop_shell, state->config->position);
	}

	if (font) {
		state->output->window->font = parse_font(json_object_get_string(font));
	}

	if (sep_symbol) {
		free(state->config->sep_symbol);
		state->config->sep_symbol = strdup(json_object_get_string(sep_symbol));
	}

	if (_strip_workspace_numbers) {
		state->config->strip_workspace_numbers = json_object_get_boolean(_strip_workspace_numbers);
	}

	if (_binding_mode_indicator) {
		state->config->binding_mode_indicator = json_object_get_boolean(_binding_mode_indicator);
	}

	if (_workspace_buttons) {
		state->config->workspace_buttons = json_object_get_boolean(_workspace_buttons);
	}

	if (bar_height) {
		int width, height;
		get_text_size(state->output->window, &width, &height, "Test string for measuring purposes");
		int bar_height_value = json_object_get_int(bar_height);
		if (bar_height_value > 0) {
			state->config->margin = (bar_height_value - height) / 2;
			state->config->ws_vertical_padding = state->config->margin - 1.5;
		}
		state->output->window->height = height + state->config->margin * 2;
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
		if (background) {
			state->config->colors.background = parse_color(json_object_get_string(background));
		}
		if (statusline) {
			state->config->colors.statusline = parse_color(json_object_get_string(statusline));
		}
		if (separator) {
			state->config->colors.separator = parse_color(json_object_get_string(separator));
		}
		if (focused_workspace_border) {
			state->config->colors.focused_workspace.border = parse_color(json_object_get_string(focused_workspace_border));
		}
		if (focused_workspace_bg) {
			state->config->colors.focused_workspace.background = parse_color(json_object_get_string(focused_workspace_bg));
		}
		if (focused_workspace_text) {
			state->config->colors.focused_workspace.text = parse_color(json_object_get_string(focused_workspace_text));
		}
		if (active_workspace_border) {
			state->config->colors.active_workspace.border = parse_color(json_object_get_string(active_workspace_border));
		}
		if (active_workspace_bg) {
			state->config->colors.active_workspace.background = parse_color(json_object_get_string(active_workspace_bg));
		}
		if (active_workspace_text) {
			state->config->colors.active_workspace.text = parse_color(json_object_get_string(active_workspace_text));
		}
		if (inactive_workspace_border) {
			state->config->colors.inactive_workspace.border = parse_color(json_object_get_string(inactive_workspace_border));
		}
		if (inactive_workspace_bg) {
			state->config->colors.inactive_workspace.background = parse_color(json_object_get_string(inactive_workspace_bg));
		}
		if (inactive_workspace_text) {
			state->config->colors.inactive_workspace.text = parse_color(json_object_get_string(inactive_workspace_text));
		}
		if (binding_mode_border) {
			state->config->colors.binding_mode.border = parse_color(json_object_get_string(binding_mode_border));
		}
		if (binding_mode_bg) {
			state->config->colors.binding_mode.background = parse_color(json_object_get_string(binding_mode_bg));
		}
		if (binding_mode_text) {
			state->config->colors.binding_mode.text = parse_color(json_object_get_string(binding_mode_text));
		}
	}

	json_object_put(bar_config);
	free(res);

	const char *subscribe_json = "[ \"workspace\", \"mode\" ]";
	len = strlen(subscribe_json);
	res = ipc_single_command(state->ipc_event_socketfd, IPC_SUBSCRIBE, subscribe_json, &len);
	free(res);

	ipc_update_workspaces();
}
bool handle_ipc_event() {
	struct ipc_response *resp = ipc_recv_response(state->ipc_event_socketfd);
	switch (resp->type) {
	case IPC_EVENT_WORKSPACE:
		ipc_update_workspaces();
		break;
	case IPC_EVENT_MODE: {
		json_object *result = json_tokener_parse(resp->payload);
		if (!result) {
			free_ipc_response(resp);
			sway_log(L_ERROR, "failed to parse payload as json");
			return false;
		}
		json_object *json_change;
		if (json_object_object_get_ex(result, "change", &json_change)) {
			const char *change = json_object_get_string(json_change);

			free(state->config->mode);
			if (strcmp(change, "default") == 0) {
				state->config->mode = NULL;
			} else {
				state->config->mode = strdup(change);
			}
		} else {
			sway_log(L_ERROR, "failed to parse response");
		}

		json_object_put(result);
		break;
	}
	default:
		free_ipc_response(resp);
		return false;
	}

	free_ipc_response(resp);
	return true;
}

void poll_for_update() {
	fd_set readfds;
	int activity;
	bool dirty = true;

	while (1) {
		if (dirty) {
			struct output *output = state->output;
			if (window_prerender(output->window) && output->window->cairo) {
				render(output, state->config, state->status);
				window_render(output->window);
				if (wl_display_dispatch(output->registry->display) == -1) {
					break;
				}
			}
		}

		dirty = false;
		FD_ZERO(&readfds);
		FD_SET(state->ipc_event_socketfd, &readfds);
		FD_SET(state->status_read_fd, &readfds);

		activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			sway_log(L_ERROR, "polling failed: %d", errno);
		}

		if (FD_ISSET(state->ipc_event_socketfd, &readfds)) {
			sway_log(L_DEBUG, "Got IPC event.");
			dirty = handle_ipc_event();
		}

		if (state->config->status_command && FD_ISSET(state->status_read_fd, &readfds)) {
			sway_log(L_DEBUG, "Got update from status command.");
			dirty = handle_status_line(state);
		}
	}
}

int main(int argc, char **argv) {
	char *socket_path = NULL;
	char *bar_id = NULL;
	bool debug = false;
	state = init_state();

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"bar_id", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaybar [options...] <output>\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -v, --version          Show the version number and quit.\n"
		"  -s, --socket <socket>  Connect to sway via socket.\n"
		"  -b, --bar_id <id>      Bar ID for which to get the configuration.\n"
		"  -d, --debug            Enable debugging.\n"
		"\n"
		" PLEASE NOTE that swaybar will be automatically started by sway as\n"
		" soon as there is a 'bar' configuration block in your config file.\n"
		" You should never need to start it manually.\n";

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hvs:b:d", long_options, &option_index);
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
		case 'd': // Debug
			debug = true;
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (!bar_id) {
		sway_abort("No bar_id passed. Provide --bar_id or let sway start swaybar");
	}

	if (debug) {
		init_log(L_DEBUG);
	} else {
		init_log(L_ERROR);
	}

	state->output->registry = registry_poll();

	if (!state->output->registry->desktop_shell) {
		sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
	}

	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}
	state->ipc_socketfd = ipc_open_socket(socket_path);
	state->ipc_event_socketfd = ipc_open_socket(socket_path);


	if (argc == optind) {
		sway_abort("No output index provided");
	}

	int desired_output = atoi(argv[optind]);
	struct output_state *output = state->output->registry->outputs->items[desired_output];

	state->output->window = window_setup(state->output->registry, output->width, 30, false);
	if (!state->output->window) {
		sway_abort("Failed to create window.");
	}
	desktop_shell_set_panel(state->output->registry->desktop_shell, output->output, state->output->window->surface);

	bar_ipc_init(desired_output, bar_id);

	if (state->config->status_command) {
		int pipefd[2];
		pipe(pipefd);
		state->status_command_pid = fork();
		if (state->status_command_pid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[1]);
			char *const cmd[] = {
				"sh",
				"-c",
				state->config->status_command,
				NULL,
			};
			execvp(cmd[0], cmd);
			return 0;
		}

		close(pipefd[1]);
		state->status_read_fd = pipefd[0];
		fcntl(state->status_read_fd, F_SETFL, O_NONBLOCK);
	}

	signal(SIGTERM, sig_handler);

	poll_for_update();

	// gracefully shutdown swaybar and status_command
	swaybar_teardown();

	return 0;
}
