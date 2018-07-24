#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_exec_always(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->active) return cmd_results_new(CMD_DEFER, NULL, NULL);
	if ((error = checkarg(argc, "exec_always", EXPECTED_MORE_THAN, 0))) {
		return error;
	}

	char *tmp = NULL;
	if (strcmp((char*)*argv, "--no-startup-id") == 0) {
		wlr_log(WLR_INFO, "exec switch '--no-startup-id' not supported, ignored.");
		if ((error = checkarg(argc - 1, "exec_always", EXPECTED_MORE_THAN, 0))) {
			return error;
		}

		tmp = join_args(argv + 1, argc - 1);
	} else {
		tmp = join_args(argv, argc);
	}

	// Put argument into cmd array
	char cmd[4096];
	strncpy(cmd, tmp, sizeof(cmd) - 1);
	cmd[sizeof(cmd) - 1] = 0;
	free(tmp);
	wlr_log(WLR_DEBUG, "Executing %s", cmd);

	int fd[2];
	if (pipe(fd) != 0) {
		wlr_log(WLR_ERROR, "Unable to create pipe for fork");
	}

	pid_t pid, child;
	// Fork process
	if ((pid = fork()) == 0) {
		// Fork child process again
		setsid();
		close(fd[0]);
		if ((child = fork()) == 0) {
			close(fd[1]);
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
			_exit(0);
		}
		ssize_t s = 0;
		while ((size_t)s < sizeof(pid_t)) {
			s += write(fd[1], ((uint8_t *)&child) + s, sizeof(pid_t) - s);
		}
		close(fd[1]);
		_exit(0); // Close child process
	} else if (pid < 0) {
		close(fd[0]);
		close(fd[1]);
		return cmd_results_new(CMD_FAILURE, "exec_always", "fork() failed");
	}
	close(fd[1]); // close write
	ssize_t s = 0;
	while ((size_t)s < sizeof(pid_t)) {
		s += read(fd[0], ((uint8_t *)&child) + s, sizeof(pid_t) - s);
	}
	close(fd[0]);
	// cleanup child process
	waitpid(pid, NULL, 0);
	if (child > 0) {
		wlr_log(WLR_DEBUG, "Child process created with pid %d", child);
		workspace_record_pid(child);
	} else {
		return cmd_results_new(CMD_FAILURE, "exec_always",
			"Second fork() failed");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
