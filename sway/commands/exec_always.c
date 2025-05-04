#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/server.h"
#include "sway/desktop/launcher.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_exec_validate(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, argv[-1], EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!config->active || config->validating) {
		return cmd_results_new(CMD_DEFER, NULL);
	}
	return error;
}

struct cmd_results *cmd_exec_process(int argc, char **argv) {
	struct cmd_results *error = NULL;
	char *cmd = NULL;
	bool no_startup_id = false;
	if (strcmp(argv[0], "--no-startup-id") == 0) {
		no_startup_id = true;
		--argc; ++argv;
		if ((error = checkarg(argc, argv[-1], EXPECTED_AT_LEAST, 1))) {
			return error;
		}
	}

	if (argc == 1 && (argv[0][0] == '\'' || argv[0][0] == '"')) {
		cmd = strdup(argv[0]);
		strip_quotes(cmd);
	} else {
		cmd = join_args(argv, argc);
	}

	sway_log(SWAY_DEBUG, "Executing %s", cmd);

	struct launcher_ctx *ctx = launcher_ctx_create_internal();

	// Fork process
	pid_t child = fork();
	if (child == 0) {
		setsid();

		if (ctx) {
			const char *token = launcher_ctx_get_token_name(ctx);
			setenv("XDG_ACTIVATION_TOKEN", token, 1);
			if (!no_startup_id) {
				setenv("DESKTOP_STARTUP_ID", token, 1);
			}
		}

		execlp("sh", "sh", "-c", cmd, (void*)NULL);
		sway_log_errno(SWAY_ERROR, "execve failed");
		_exit(0); // Close child process
	} else if (child < 0) {
		launcher_ctx_destroy(ctx);
		free(cmd);
		return cmd_results_new(CMD_FAILURE, "fork() failed");
	}

	sway_log(SWAY_DEBUG, "Child process created with pid %d", child);
	if (ctx != NULL) {
		sway_log(SWAY_DEBUG, "Recording workspace for process %d", child);
		ctx->pid = child;
	}

	free(cmd);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_exec_always(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = cmd_exec_validate(argc, argv))) {
		return error;
	}
	return cmd_exec_process(argc, argv);
}
