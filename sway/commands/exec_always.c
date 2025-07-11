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
	bool matched_container_id = false;
	bool no_startup_id = false;
	int argv_counter = -1;
	while (argc > 0 && has_prefix(*argv, "--")) {
		if (strcmp(argv[0], "--matched-container-id") == 0) {
			matched_container_id = true;
		} else if (strcmp(argv[0], "--no-startup-id") == 0) {
			no_startup_id = true;
		} else {
			return cmd_results_new(CMD_INVALID, "Unrecognized argument '%s'", *argv);
		}
		--argc; ++argv; --argv_counter;
	}

	if ((error = checkarg(argc, argv[argv_counter], EXPECTED_AT_LEAST, 1))) {
		return error;
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

		if (matched_container_id && config->handler_context.node != NULL) {
			size_t con_id = config->handler_context.node->id;
			int con_id_len = snprintf(NULL, 0, "%zu", con_id);
			if (con_id_len < 0) {
				sway_log(SWAY_ERROR, "Unable to determine buffer length for SWAY_EXEC_CON_ID");
				goto no_con_id_export;
			}
			// accommodate \0
			con_id_len++;
			char* con_id_str = malloc(con_id_len);
			if (!con_id_str) {
				sway_log(SWAY_ERROR, "Unable to allocate buffer for SWAY_EXEC_CON_ID");
				goto no_con_id_export;
			}
			snprintf(con_id_str, con_id_len, "%zu", con_id);
			setenv("SWAY_EXEC_CON_ID", con_id_str, 1);
			free(con_id_str);
		}
no_con_id_export:

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
