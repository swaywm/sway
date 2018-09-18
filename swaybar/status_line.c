#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "swaybar/config.h"
#include "swaybar/event_loop.h"
#include "swaybar/status_line.h"
#include "readline.h"

static void status_line_close_fds(struct status_line *status) {
	if (status->read_fd != -1) {
		remove_event(status->read_fd);
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
		read_bytes = getline(&status->buffer,
				&status->buffer_size, status->read);
		if (errno == EAGAIN) {
			clearerr(status->read);
		} else if (errno) {
			status_error(status, "[error reading from status command]");
			return true;
		}

		// the header must be sent completely the first time round
		json_object *header, *version;
		if (status->buffer[read_bytes - 1] == '\n'
				&& (header = json_tokener_parse(status->buffer))
				&& json_object_object_get_ex(header, "version", &version)
				&& json_object_get_int(version) == 1) {
			wlr_log(WLR_DEBUG, "Using i3bar protocol.");
			status->protocol = PROTOCOL_I3BAR;

			json_object *click_events;
			if (json_object_object_get_ex(header, "click_events", &click_events)
					&& json_object_get_boolean(click_events)) {
				wlr_log(WLR_DEBUG, "Enabling click events.");
				status->click_events = true;
				if (write(status->write_fd, "[\n", 2) != 2) {
					status_error(status, "[failed to write to status command]");
					json_object_put(header);
					return true;
				}
			}
			json_object_put(header);

			wl_list_init(&status->blocks);
			status->tokener = json_tokener_new();
			status->buffer_index = getdelim(&status->buffer,
					&status->buffer_size, EOF, status->read);
			return i3bar_handle_readable(status);
		}

		wlr_log(WLR_DEBUG, "Using text protocol.");
		status->protocol = PROTOCOL_TEXT;
		status->text = status->buffer;
		// intentional fall-through
	case PROTOCOL_TEXT:
		errno = 0;
		while (true) {
			if (status->buffer[read_bytes - 1] == '\n') {
				status->buffer[read_bytes - 1] = '\0';
			}
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
	status->buffer_size = 8192;
	status->buffer = malloc(status->buffer_size);

	int pipe_read_fd[2];
	int pipe_write_fd[2];
	if (pipe(pipe_read_fd) != 0 || pipe(pipe_write_fd) != 0) {
		wlr_log(WLR_ERROR, "Unable to create pipes for status_command fork");
		exit(1);
	}

	status->pid = fork();
	if (status->pid == 0) {
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
	kill(status->pid, SIGTERM);
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
