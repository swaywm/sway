#define _POSIX_C_SOURCE
#include <fcntl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "config.h"
#include "swaybar/config.h"
#include "swaybar/status_line.h"
#include "readline.h"

void status_error(struct status_line *status, const char *text) {
	close(status->read_fd);
	close(status->write_fd);
	status->protocol = PROTOCOL_ERROR;
	status->text = text;
}

bool status_handle_readable(struct status_line *status) {
	char *line;
	switch (status->protocol) {
	case PROTOCOL_ERROR:
		return false;
	case PROTOCOL_I3BAR:
		if (i3bar_handle_readable(status) > 0) {
			return true;
		}
		break;
	case PROTOCOL_TEXT:
		line = read_line_buffer(status->read,
				status->text_state.buffer, status->text_state.buffer_size);
		if (!line) {
			status_error(status, "[error reading from status command]");
		} else {
			status->text = line;
		}
		return true;
	case PROTOCOL_UNDEF:
		line = read_line_buffer(status->read,
				status->text_state.buffer, status->text_state.buffer_size);
		if (!line) {
			status_error(status, "[error reading from status command]");
			return false;
		}
		if (line[0] == '{') {
			json_object *proto = json_tokener_parse(line);
			if (proto) {
				json_object *version;
				if (json_object_object_get_ex(proto, "version", &version)
							&& json_object_get_int(version) == 1) {
					wlr_log(L_DEBUG, "Switched to i3bar protocol.");
					status->protocol = PROTOCOL_I3BAR;
				}
				json_object *click_events;
				if (json_object_object_get_ex(
							proto, "click_events", &click_events)
						&& json_object_get_boolean(click_events)) {
					wlr_log(L_DEBUG, "Enabled click events.");
					status->i3bar_state.click_events = true;
					const char *events_array = "[\n";
					write(status->write_fd, events_array, strlen(events_array));
				}
				json_object_put(proto);
			}

			status->protocol = PROTOCOL_I3BAR;
			free(status->text_state.buffer);
			wl_list_init(&status->blocks);
			status->i3bar_state.buffer_size = 4096;
			status->i3bar_state.buffer =
				malloc(status->i3bar_state.buffer_size);
		} else {
			status->protocol = PROTOCOL_TEXT;
			status->text = line;
		}
		return true;
	}
	return false;
}

struct status_line *status_line_init(char *cmd) {
	struct status_line *status = calloc(1, sizeof(struct status_line));
	status->text_state.buffer_size = 8192;
	status->text_state.buffer = malloc(status->text_state.buffer_size);

	int pipe_read_fd[2];
	int pipe_write_fd[2];
	if (pipe(pipe_read_fd) != 0 || pipe(pipe_write_fd) != 0) {
		wlr_log(L_ERROR, "Unable to create pipes for status_command fork");
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
	close(status->read_fd);
	close(status->write_fd);
	kill(status->pid, SIGTERM);
	free(status);
}
