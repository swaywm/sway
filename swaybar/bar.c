#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <poll.h>
#include "swaybar/ipc.h"
#include "swaybar/render.h"
#include "swaybar/config.h"
#include "swaybar/status_line.h"
#include "swaybar/bar.h"
#include "ipc-client.h"
#include "list.h"
#include "log.h"

static void bar_init(struct bar *bar) {
	bar->config = init_config();
	bar->status = init_status_line();
	bar->outputs = create_list();
}

static void spawn_status_cmd_proc(struct bar *bar) {
	if (bar->config->status_command) {
		int pipefd[2];
		if (pipe(pipefd) != 0) {
			sway_log(L_ERROR, "Unable to create pipe for status_command fork");
			return;
		}
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

static void mouse_button_notify(struct window *window, int x, int y,
		uint32_t button, uint32_t state_w) {
	sway_log(L_DEBUG, "Mouse button %d clicked at %d %d %d\n", button, x, y, state_w);
	if (!state_w) {
		return;
	}

	struct output *clicked_output = NULL;
	for (int i = 0; i < swaybar.outputs->length; i++) {
		struct output *output = swaybar.outputs->items[i];
		if (window == output->window) {
			clicked_output = output;
			break;
		}
	}

	if (!sway_assert(clicked_output != NULL, "Got pointer event for non-existing output")) {
		return;
	}

	double button_x = 0.5;
	for (int i = 0; i < clicked_output->workspaces->length; i++) {
		struct workspace *workspace = clicked_output->workspaces->items[i];
		int button_width, button_height;

		workspace_button_size(window, workspace->name, &button_width, &button_height);

		button_x += button_width;
		if (x <= button_x) {
			ipc_send_workspace_command(workspace->name);
			break;
		}
	}
}

static void mouse_scroll_notify(struct window *window, enum scroll_direction direction) {
	sway_log(L_DEBUG, "Mouse wheel scrolled %s", direction == SCROLL_UP ? "up" : "down");

	if (!swaybar.config->wrap_scroll) {
		// Find output this window lives on
		int i;
		struct output *output = NULL;
		for (i = 0; i < swaybar.outputs->length; ++i) {
			output = swaybar.outputs->items[i];
			if (output->window == window) {
				break;
			}
		}
		if (!sway_assert(i != swaybar.outputs->length, "Unknown window in scroll event")) {
			return;
		}
		int focused = -1;
		for (i = 0; i < output->workspaces->length; ++i) {
			struct workspace *ws = output->workspaces->items[i];
			if (ws->focused) {
				focused = i;
				break;
			}
		}
		if (!sway_assert(focused != -1, "Scroll wheel event received on inactive output")) {
			return;
		}
		if ((focused == 0 && direction == SCROLL_UP) ||
				(focused == output->workspaces->length - 1 && direction == SCROLL_DOWN)) {
			// Do not wrap
			return;
		}
	}

	const char *workspace_name = direction == SCROLL_UP ? "prev_on_output" : "next_on_output";
	ipc_send_workspace_command(workspace_name);
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

		window_make_shell(bar_output->window);

		/* set font */
		bar_output->window->font = bar->config->font;

		/* set mouse event callbacks */
		bar_output->window->pointer_input.notify_button = mouse_button_notify;
		bar_output->window->pointer_input.notify_scroll = mouse_scroll_notify;

		/* set window height */
		set_window_height(bar_output->window, bar->config->height);
	}
	/* spawn status command */
	spawn_status_cmd_proc(bar);
}

void bar_run(struct bar *bar) {
	int pfds = bar->outputs->length + 2;
	struct pollfd *pfd = malloc(pfds * sizeof(struct pollfd));
	bool dirty = true;

	pfd[0].fd = bar->ipc_event_socketfd;
	pfd[0].events = POLLIN;
	pfd[1].fd = bar->status_read_fd;
	pfd[1].events = POLLIN;

	int i;
	for (i = 0; i < bar->outputs->length; ++i) {
		struct output *output = bar->outputs->items[i];
		pfd[i+2].fd = wl_display_get_fd(output->registry->display);
		pfd[i+2].events = POLLIN;
	}

	while (1) {
		if (dirty) {
			int i;
			for (i = 0; i < bar->outputs->length; ++i) {
				struct output *output = bar->outputs->items[i];
				if (window_prerender(output->window) && output->window->cairo) {
					render(output, bar->config, bar->status);
					window_render(output->window);
					wl_display_flush(output->registry->display);
				}
			}
		}

		dirty = false;

		poll(pfd, pfds, -1);

		if (pfd[0].revents & POLLIN) {
			sway_log(L_DEBUG, "Got IPC event.");
			dirty = handle_ipc_event(bar);
		}

		if (bar->config->status_command && pfd[1].revents & POLLIN) {
			sway_log(L_DEBUG, "Got update from status command.");
			dirty = handle_status_line(bar);
		}

		// dispatch wl_display events
		for (i = 0; i < bar->outputs->length; ++i) {
			struct output *output = bar->outputs->items[i];
			if (pfd[i+2].revents & POLLIN) {
				if (wl_display_dispatch(output->registry->display) == -1) {
					sway_log(L_ERROR, "failed to dispatch wl: %d", errno);
				}
			} else {
				wl_display_dispatch_pending(output->registry->display);
			}
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
	if (bar->config) {
		free_config(bar->config);
	}

	if (bar->outputs) {
		free_outputs(bar->outputs);
	}

	if (bar->status) {
		free_status_line(bar->status);
	}

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
