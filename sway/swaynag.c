#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "log.h"
#include "sway/swaynag.h"

void swaynag_clone(struct swaynag_instance *dest,
		struct swaynag_instance *src) {
	dest->args = src->args;
	dest->pid = src->pid;
	dest->fd[0] = src->fd[0];
	dest->fd[1] = src->fd[1];
	dest->detailed = src->detailed;
}

bool swaynag_spawn(const char *swaynag_command,
		struct swaynag_instance *swaynag) {
	if (swaynag->detailed) {
		if (pipe(swaynag->fd) != 0) {
			wlr_log(WLR_ERROR, "Failed to create pipe for swaynag");
			return false;
		}
		fcntl(swaynag->fd[1], F_SETFD, FD_CLOEXEC);
	}

	pid_t pid;
	if ((pid = fork()) == 0) {
		if (swaynag->detailed) {
			close(swaynag->fd[1]);
			dup2(swaynag->fd[0], STDIN_FILENO);
			close(swaynag->fd[0]);
		}

		size_t length = strlen(swaynag_command) + strlen(swaynag->args) + 2;
		char *cmd = malloc(length);
		snprintf(cmd, length, "%s %s", swaynag_command, swaynag->args);
		execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		_exit(0);
	} else if (pid < 0) {
		wlr_log(WLR_ERROR, "Failed to create fork for swaynag");
		if (swaynag->detailed) {
			close(swaynag->fd[0]);
			close(swaynag->fd[1]);
		}
		return false;
	}

	if (swaynag->detailed) {
		close(swaynag->fd[0]);
	}
	swaynag->pid = pid;
	return true;
}


void swaynag_kill(struct swaynag_instance *swaynag) {
	if (swaynag->pid > 0) {
		kill(swaynag->pid, SIGTERM);
		swaynag->pid = -1;
	}
}

void swaynag_log(const char *swaynag_command, struct swaynag_instance *swaynag,
		const char *fmt, ...) {
	if (!swaynag->detailed) {
		wlr_log(WLR_ERROR, "Attempting to write to non-detailed swaynag inst");
		return;
	}

	if (swaynag->pid <= 0 && !swaynag_spawn(swaynag_command, swaynag)) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	size_t length = vsnprintf(NULL, 0, fmt, args) + 1;
	va_end(args);

	char *temp = malloc(length + 1);
	if (!temp) {
		wlr_log(WLR_ERROR, "Failed to allocate buffer for swaynag log entry.");
		return;
	}

	va_start(args, fmt);
	vsnprintf(temp, length, fmt, args);
	va_end(args);

	write(swaynag->fd[1], temp, length);

	free(temp);
}

void swaynag_show(struct swaynag_instance *swaynag) {
	if (swaynag->detailed && swaynag->pid > 0) {
		close(swaynag->fd[1]);
	}
}

