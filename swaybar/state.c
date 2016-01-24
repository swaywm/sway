#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc-client.h"
#include "list.h"
#include "log.h"
#include "ipc.h"
#include "render.h"
#include "config.h"
#include "status_line.h"
#include "state.h"

static void state_init(struct swaybar_state *state) {
	state->config = init_config();
	state->status = init_status_line();
	state->output = malloc(sizeof(struct output));
	state->output->window = NULL;
	state->output->registry = NULL;
	state->output->workspaces = create_list();
	state->output->name = NULL;
}

static void spawn_status_cmd_proc(struct swaybar_state *state) {
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
			return;
		}

		close(pipefd[1]);
		state->status_read_fd = pipefd[0];
		fcntl(state->status_read_fd, F_SETFL, O_NONBLOCK);
	}
}


void state_setup(struct swaybar_state *state, const char *socket_path, const char *bar_id, int desired_output) {
	/* initialize state with default values */
	state_init(state);

	state->output->registry = registry_poll();

	if (!state->output->registry->desktop_shell) {
		sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
	}

	/* connect to sway ipc */
	state->ipc_socketfd = ipc_open_socket(socket_path);
	state->ipc_event_socketfd = ipc_open_socket(socket_path);

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

	/* spawn status command */
	spawn_status_cmd_proc(state);
}

void state_run(struct swaybar_state *state) {
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

void free_workspaces(list_t *workspaces) {
	int i;
	for (i = 0; i < workspaces->length; ++i) {
		struct workspace *ws = workspaces->items[i];
		free(ws->name);
		free(ws);
	}
	list_free(workspaces);
}

static void free_output(struct output *output) {
	window_teardown(output->window);
	if (output->registry) {
		registry_teardown(output->registry);
	}

	free(output->name);

	if (output->workspaces) {
		free_workspaces(output->workspaces);
	}

	free(output);
}

static void terminate_status_command(pid_t pid) {
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
}

void state_teardown(struct swaybar_state *state) {
	free_config(state->config);
	free_output(state->output);
	free_status_line(state->status);

	/* close sockets/pipes */
	if (state->status_read_fd) {
		close(state->status_read_fd);
	}

	if (state->ipc_socketfd) {
		close(state->ipc_socketfd);
	}

	if (state->ipc_event_socketfd) {
		close(state->ipc_event_socketfd);
	}

	/* terminate status command process */
	terminate_status_command(state->status_command_pid);
}
