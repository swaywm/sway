#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc-client.h"
#include "list.h"
#include "log.h"
#include "bar/ipc.h"
#include "bar/render.h"
#include "bar/config.h"
#include "bar/status_line.h"
#include "bar/bar.h"

static void bar_init(struct bar *bar) {
	bar->config = init_config();
	bar->status = init_status_line();
	bar->outputs = create_list();
}

static void spawn_status_cmd_proc(struct bar *bar) {
	if (bar->config->status_command) {
		int pipefd[2];
		pipe(pipefd);
		bar->status_command_pid = fork();
		if (bar->status_command_pid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[1]);
			char *const cmd[] = {
				"sh",
				"-c",
				bar->config->status_command,
				NULL,
			};
			execvp(cmd[0], cmd);
			return;
		}

		close(pipefd[1]);
		bar->status_read_fd = pipefd[0];
		fcntl(bar->status_read_fd, F_SETFL, O_NONBLOCK);
	}
}

struct output *new_output(const char *name) {
	struct output *output = malloc(sizeof(struct output));
	output->name = strdup(name);
	output->window = NULL;
	output->registry = NULL;
	output->workspaces = create_list();
	return output;
}

void bar_setup(struct bar *bar, const char *socket_path, const char *bar_id) {
	/* initialize bar with default values */
	bar_init(bar);

	/* connect to sway ipc */
	bar->ipc_socketfd = ipc_open_socket(socket_path);
	bar->ipc_event_socketfd = ipc_open_socket(socket_path);

	ipc_bar_init(bar, bar_id);

	int i;
	for (i = 0; i < bar->outputs->length; ++i) {
		struct output *bar_output = bar->outputs->items[i];

		bar_output->registry = registry_poll();

		if (!bar_output->registry->desktop_shell) {
			sway_abort("swaybar requires the compositor to support the desktop-shell extension.");
		}

		struct output_state *output = bar_output->registry->outputs->items[bar_output->idx];

		bar_output->window = window_setup(bar_output->registry, output->width, 30, false);
		if (!bar_output->window) {
			sway_abort("Failed to create window.");
		}
		desktop_shell_set_panel(bar_output->registry->desktop_shell, output->output, bar_output->window->surface);
		desktop_shell_set_panel_position(bar_output->registry->desktop_shell, bar->config->position);

		/* set font */
		bar_output->window->font = bar->config->font;

		/* set window height */
		set_window_height(bar_output->window, bar->config->height);
	}
	/* spawn status command */
	spawn_status_cmd_proc(bar);
}

void bar_run(struct bar *bar) {
	fd_set readfds;
	int activity;
	bool dirty = true;

	while (1) {
		if (dirty) {
			int i;
			for (i = 0; i < bar->outputs->length; ++i) {
				struct output *output = bar->outputs->items[i];
				if (window_prerender(output->window) && output->window->cairo) {
					render(output, bar->config, bar->status);
					window_render(output->window);
					if (wl_display_dispatch(output->registry->display) == -1) {
						break;
					}
				}
			}
		}

		dirty = false;
		FD_ZERO(&readfds);
		FD_SET(bar->ipc_event_socketfd, &readfds);
		FD_SET(bar->status_read_fd, &readfds);

		activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			sway_log(L_ERROR, "polling failed: %d", errno);
		}

		if (FD_ISSET(bar->ipc_event_socketfd, &readfds)) {
			sway_log(L_DEBUG, "Got IPC event.");
			dirty = handle_ipc_event(bar);
		}

		if (bar->config->status_command && FD_ISSET(bar->status_read_fd, &readfds)) {
			sway_log(L_DEBUG, "Got update from status command.");
			dirty = handle_status_line(bar);
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

static void free_outputs(list_t *outputs) {
	int i;
	for (i = 0; i < outputs->length; ++i) {
		free_output(outputs->items[i]);
	}
	list_free(outputs);
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

void bar_teardown(struct bar *bar) {
	free_config(bar->config);
	free_outputs(bar->outputs);
	free_status_line(bar->status);

	/* close sockets/pipes */
	if (bar->status_read_fd) {
		close(bar->status_read_fd);
	}

	if (bar->ipc_socketfd) {
		close(bar->ipc_socketfd);
	}

	if (bar->ipc_event_socketfd) {
		close(bar->ipc_event_socketfd);
	}

	/* terminate status command process */
	terminate_status_command(bar->status_command_pid);
}
