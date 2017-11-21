#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#ifdef __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#else
#include <linux/input-event-codes.h>
#endif
#ifdef ENABLE_TRAY
#include <dbus/dbus.h>
#include "swaybar/tray/sni_watcher.h"
#include "swaybar/tray/tray.h"
#include "swaybar/tray/sni.h"
#endif
#include "swaybar/ipc.h"
#include "swaybar/render.h"
#include "swaybar/config.h"
#include "swaybar/status_line.h"
#include "swaybar/event_loop.h"
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
		int pipe_read_fd[2];
		int pipe_write_fd[2];

		if (pipe(pipe_read_fd) != 0) {
			sway_log(L_ERROR, "Unable to create pipes for status_command fork");
			return;
		}
		if (pipe(pipe_write_fd) != 0) {
			sway_log(L_ERROR, "Unable to create pipe for status_command fork (write)");
			close(pipe_read_fd[0]);
			close(pipe_read_fd[1]);
			return;
		}

		bar->status_command_pid = fork();
		if (bar->status_command_pid == 0) {
			close(pipe_read_fd[0]);
			dup2(pipe_read_fd[1], STDOUT_FILENO);
			close(pipe_read_fd[1]);
			
			dup2(pipe_write_fd[0], STDIN_FILENO);
			close(pipe_write_fd[0]);
			close(pipe_write_fd[1]);
			
			char *const cmd[] = {
				"sh",
				"-c",
				bar->config->status_command,
				NULL,
			};
			execvp(cmd[0], cmd);
			return;
		}

		close(pipe_read_fd[1]);
		bar->status_read_fd = pipe_read_fd[0];
		fcntl(bar->status_read_fd, F_SETFL, O_NONBLOCK);
		
		close(pipe_write_fd[0]);
		bar->status_write_fd = pipe_write_fd[1];
		fcntl(bar->status_write_fd, F_SETFL, O_NONBLOCK);
	}
}

struct output *new_output(const char *name) {
	struct output *output = malloc(sizeof(struct output));
	output->name = strdup(name);
	output->window = NULL;
	output->registry = NULL;
	output->workspaces = create_list();
#ifdef ENABLE_TRAY
	output->items = create_list();
#endif
	return output;
}

static void mouse_button_notify(struct window *window, int x, int y,
		uint32_t button, uint32_t state_w) {
	sway_log(L_DEBUG, "Mouse button %d clicked at %d %d %d", button, x, y, state_w);
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

	switch (button) {
	case BTN_LEFT:
		status_line_mouse_event(&swaybar, x, y, 1);
		break;
	case BTN_MIDDLE:
		status_line_mouse_event(&swaybar, x, y, 2);
		break;
	case BTN_RIGHT:
		status_line_mouse_event(&swaybar, x, y, 3);
		break;
	}

#ifdef ENABLE_TRAY
	tray_mouse_event(clicked_output, x, y, button, state_w);
#endif

}

static void mouse_scroll_notify(struct window *window, enum scroll_direction direction) {
	sway_log(L_DEBUG, "Mouse wheel scrolled %s", direction == SCROLL_UP ? "up" : "down");

	// If there are status blocks and click_events are enabled
	// check if the position is within the status area and if so
	// tell the status line to output the event and skip workspace
	// switching below.
	int num_blocks = swaybar.status->block_line->length;
	if (swaybar.status->click_events && num_blocks > 0) {
		struct status_block *first_block = swaybar.status->block_line->items[0];
		int x = window->pointer_input.last_x;
		int y = window->pointer_input.last_y;
		if (x > first_block->x) {
			if (direction == SCROLL_UP) {
				status_line_mouse_event(&swaybar, x, y, 4);
			} else {
				status_line_mouse_event(&swaybar, x, y, 5);
			}
			return;
		}
	}

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

	/* Initialize event loop lists */
	init_event_loop();

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

		bar_output->window = window_setup(bar_output->registry,
				output->width / output->scale, 30, output->scale, false);
		if (!bar_output->window) {
			sway_abort("Failed to create window.");
		}
		desktop_shell_set_panel(bar_output->registry->desktop_shell,
				output->output, bar_output->window->surface);
		desktop_shell_set_panel_position(bar_output->registry->desktop_shell,
				bar->config->position);

		window_make_shell(bar_output->window);

		/* set font */
		bar_output->window->font = bar->config->font;

		/* set mouse event callbacks */
		bar_output->window->pointer_input.notify_button = mouse_button_notify;
		bar_output->window->pointer_input.notify_scroll = mouse_scroll_notify;

		/* set window height */
		set_window_height(bar_output->window, bar->config->height);

		bar_output->state = output;
	}
	/* spawn status command */
	spawn_status_cmd_proc(bar);

#ifdef ENABLE_TRAY
	init_tray(bar);
#endif
}

bool dirty = true;

static void respond_ipc(int fd, short mask, void *_bar) {
	struct bar *bar = (struct bar *)_bar;
	sway_log(L_DEBUG, "Got IPC event.");
	dirty = handle_ipc_event(bar);
}

static void respond_command(int fd, short mask, void *_bar) {
	struct bar *bar = (struct bar *)_bar;
	dirty = handle_status_line(bar);
}

static void respond_output(int fd, short mask, void *_output) {
	struct output *output = (struct output *)_output;
	if (wl_display_dispatch(output->registry->display) == -1) {
		sway_log(L_ERROR, "failed to dispatch wl: %d", errno);
	}
}

void bar_run(struct bar *bar) {
	add_event(bar->ipc_event_socketfd, POLLIN, respond_ipc, bar);
	add_event(bar->status_read_fd, POLLIN, respond_command, bar);

	int i;
	for (i = 0; i < bar->outputs->length; ++i) {
		struct output *output = bar->outputs->items[i];
		add_event(wl_display_get_fd(output->registry->display),
				POLLIN, respond_output, output);
	}

	while (1) {
		if (dirty) {
			int i;
			for (i = 0; i < bar->outputs->length; ++i) {
				struct output *output = bar->outputs->items[i];
				if (window_prerender(output->window) && output->window->cairo) {
					output->active = true;
					render(output, bar->config, bar->status);
					window_render(output->window);
					wl_display_flush(output->registry->display);
				} else {
					output->active = false;
				}
			}
		}

		dirty = false;

		event_loop_poll();
#ifdef ENABLE_TRAY
		dispatch_dbus();
#endif
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

	if (bar->status_write_fd) {
		close(bar->status_write_fd);
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
