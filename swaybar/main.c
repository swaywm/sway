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
#include "ipc.h"

struct swaybar_state *state;

void sway_terminate(void) {
	free_state(state);
	exit(EXIT_FAILURE);
}

void sig_handler(int signal) {
	free_state(state);
	exit(0);
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
			dirty = handle_ipc_event(state);
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
	ipc_bar_init(state, desired_output, bar_id);

	struct output_state *output = state->output->registry->outputs->items[desired_output];

	state->output->window = window_setup(state->output->registry, output->width, 30, false);
	if (!state->output->window) {
		sway_abort("Failed to create window.");
	}
	desktop_shell_set_panel(state->output->registry->desktop_shell, output->output, state->output->window->surface);
	desktop_shell_set_panel_position(state->output->registry->desktop_shell, state->config->position);

	/* set font */
	state->output->window->font = state->config->font;

	/* set window height */
	set_window_height(state->output->window, state->config->height);

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
	free_state(state);

	return 0;
}
