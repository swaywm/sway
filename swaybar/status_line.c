#define _POSIX_C_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "swaybar/config.h"
#include "swaybar/status_line.h"
#include "readline.h"

bool handle_status_readable(struct status_line *status) {
	char *line = read_line_buffer(status->read,
			status->buffer, status->buffer_size);
	switch (status->protocol) {
	case PROTOCOL_I3BAR:
		// TODO
		break;
	case PROTOCOL_TEXT:
		status->text = line;
		return true;
	case PROTOCOL_UNDEF:
		if (!line) {
			return false;
		}
		if (line[0] == '{') {
			// TODO: JSON
		} else {
			status->text = line;
			status->protocol = PROTOCOL_TEXT;
		}
		return false;
	}
	return false;
}

struct status_line *status_line_init(char *cmd) {
	struct status_line *status = calloc(1, sizeof(struct status_line));
	status->buffer_size = 4096;
	status->buffer = malloc(status->buffer_size);

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
	free(status);
}
