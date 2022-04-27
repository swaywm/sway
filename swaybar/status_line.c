#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "log.h"
#include "loop.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/i3bar.h"
#include "swaybar/status_line.h"

static void status_line_close_fds(struct status_line *status) {
	if (status->read_fd != -1) {
		loop_remove_fd(status->bar->eventloop, status->read_fd);
		close(status->read_fd);
		status->read_fd = -1;
	}
	if (status->write_fd != -1) {
		close(status->write_fd);
		status->write_fd = -1;
	}
}

void status_error(struct status_line *status, const char *text) {
	status_line_close_fds(status);
	status->protocol = PROTOCOL_ERROR;
	status->text = text;
}

bool status_handle_readable(struct status_line *status) {
	ssize_t read_bytes = 1;
	switch (status->protocol) {
	case PROTOCOL_UNDEF:
		errno = 0;
		int available_bytes;
		if (ioctl(status->read_fd, FIONREAD, &available_bytes) == -1) {
			sway_log(SWAY_ERROR, "Unable to read status command output size");
			status_error(status, "[error reading from status command]");
			return true;
		}

		if ((size_t)available_bytes + 1 > status->buffer_size) {
			// need room for leading '\0' too
			status->buffer_size = available_bytes + 1;
			status->buffer = realloc(status->buffer, status->buffer_size);
		}
		if (status->buffer == NULL) {
			sway_log_errno(SWAY_ERROR, "Unable to read status line");
			status_error(status, "[error reading from status command]");
			return true;
		}

		read_bytes = read(status->read_fd, status->buffer, available_bytes);
		if (read_bytes != available_bytes) {
			status_error(status, "[error reading from status command]");
			return true;
		}
		status->buffer[available_bytes] = 0;

		// the header must be sent completely the first time round
		char *newline = strchr(status->buffer, '\n');
		json_object *header, *version;
		if (newline != NULL
				&& (header = json_tokener_parse(status->buffer))
				&& json_object_object_get_ex(header, "version", &version)
				&& json_object_get_int(version) == 1) {
			sway_log(SWAY_DEBUG, "Using i3bar protocol.");
			status->protocol = PROTOCOL_I3BAR;

			json_object *click_events;
			if (json_object_object_get_ex(header, "click_events", &click_events)
					&& json_object_get_boolean(click_events)) {
				sway_log(SWAY_DEBUG, "Enabling click events.");
				status->click_events = true;
				if (write(status->write_fd, "[\n", 2) != 2) {
					status_error(status, "[failed to write to status command]");
					json_object_put(header);
					return true;
				}
			}

			json_object *float_event_coords;
			if (json_object_object_get_ex(header, "float_event_coords", &float_event_coords)
					&& json_object_get_boolean(float_event_coords)) {
				sway_log(SWAY_DEBUG, "Enabling floating-point coordinates.");
				status->float_event_coords = true;
			}

			json_object *signal;
			if (json_object_object_get_ex(header, "stop_signal", &signal)) {
				status->stop_signal = json_object_get_int(signal);
				sway_log(SWAY_DEBUG, "Setting stop signal to %d", status->stop_signal);
			}
			if (json_object_object_get_ex(header, "cont_signal", &signal)) {
				status->cont_signal = json_object_get_int(signal);
				sway_log(SWAY_DEBUG, "Setting cont signal to %d", status->cont_signal);
			}

			json_object_put(header);

			wl_list_init(&status->blocks);
			status->tokener = json_tokener_new();
			status->buffer_index = strlen(newline + 1);
			memmove(status->buffer, newline + 1, status->buffer_index + 1);
			return i3bar_handle_readable(status);
		}

		sway_log(SWAY_DEBUG, "Using text protocol.");
		status->protocol = PROTOCOL_TEXT;
		status->text = status->buffer;
		// intentional fall-through
	case PROTOCOL_TEXT:
		while (true) {
			if (status->buffer[read_bytes - 1] == '\n') {
				status->buffer[read_bytes - 1] = '\0';
			}
			errno = 0;
			read_bytes = getline(&status->buffer,
					&status->buffer_size, status->read);
			if (errno == EAGAIN) {
				clearerr(status->read);
				return true;
			} else if (errno) {
				status_error(status, "[error reading from status command]");
				return true;
			}
		}
	case PROTOCOL_I3BAR:
		return i3bar_handle_readable(status);
	default:
		return false;
	}
}

struct status_line *status_line_init(char *cmd) {
	struct status_line *status = calloc(1, sizeof(struct status_line));
	status->stop_signal = SIGSTOP;
	status->cont_signal = SIGCONT;

	status->buffer_size = 8192;
	status->buffer = malloc(status->buffer_size);

	int pipe_read_fd[2];
	int pipe_write_fd[2];
	if (pipe(pipe_read_fd) != 0 || pipe(pipe_write_fd) != 0) {
		sway_log(SWAY_ERROR, "Unable to create pipes for status_command fork");
		exit(1);
	}

	assert(!getenv("WAYLAND_SOCKET") && "display must be initialized before "
		" starting `status-command`; WAYLAND_SOCKET should not be set");
	status->pid = fork();
	if (status->pid < 0) {
		sway_log_errno(SWAY_ERROR, "fork failed");
		exit(1);
	} else if (status->pid == 0) {
		setpgid(0, 0);

		dup2(pipe_read_fd[1], STDOUT_FILENO);
		close(pipe_read_fd[0]);
		close(pipe_read_fd[1]);

		dup2(pipe_write_fd[0], STDIN_FILENO);
		close(pipe_write_fd[0]);
		close(pipe_write_fd[1]);

		char *const _cmd[] = { "sh", "-c", cmd, NULL, };
		execvp(_cmd[0], _cmd);
		exit(1);
	}

	close(pipe_read_fd[1]);
	status->read_fd = pipe_read_fd[0];
	fcntl(status->read_fd, F_SETFL, O_NONBLOCK);
	close(pipe_write_fd[0]);
	status->write_fd = pipe_write_fd[1];
	fcntl(status->write_fd, F_SETFL, O_NONBLOCK);

	status->read = fdopen(status->read_fd, "r");
	status->write = fdopen(status->write_fd, "w");
	return status;
}

void status_line_free(struct status_line *status) {
	status_line_close_fds(status);
	kill(-status->pid, status->cont_signal);
	kill(-status->pid, SIGTERM);
	waitpid(status->pid, NULL, 0);
	if (status->protocol == PROTOCOL_I3BAR) {
		struct i3bar_block *block, *tmp;
		wl_list_for_each_safe(block, tmp, &status->blocks, link) {
			wl_list_remove(&block->link);
			i3bar_block_unref(block);
		}
		json_tokener_free(status->tokener);
	}
	free(status->buffer);
	free(status);
}
