#define _POSIX_C_SOURCE 200809L // for getline
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stringop.h"

#define SWAY_COMMAND "sway"

char allowed_shell(char *shell) {
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	char allowed = false;

	if (strstr(shell, "false") != NULL || strstr(shell, "nologin") != NULL) {
		return false;
	}

	fp = fopen("/etc/shells", "r");
	if (fp == NULL) {
		return true;
	}

	while (getline(&line, &len, fp) != -1) {
		strip_whitespace(line);
		if (strcmp(shell, line) == 0) {
			allowed = true;
			break;
		}
	}

	fclose(fp);
	if (line) {
		free(line);
	}
	return allowed;
}

int main(int argc, char **argv) {
	char *shell = getenv("SHELL");

	if (shell && allowed_shell(shell)) {
		// 3 exec arguments + argc + argv[argc] NULL pointer
		int exec_argc = 4 + argc;
		char **exec_argv = malloc(exec_argc * sizeof(char*));

		// Prefix - to shell path to indicate login shell
		char *login_shell = malloc(strlen(shell) + 2);
		strcpy(login_shell, "-");
		strcat(login_shell, shell);

		// Build the argumrnts to exec
		memcpy(exec_argv + 3, argv, (argc + 1) * sizeof(argv));
		exec_argv[0] = login_shell;
		exec_argv[1] = "-c";
		exec_argv[2] = "exec " SWAY_COMMAND " \"$@\"";
		exec_argv[3] = shell;

		execvp(shell, exec_argv);
		fprintf(stderr, "Could not run %s using login shell: %s\n", SWAY_COMMAND, shell);
	} else {
		argv[0] = SWAY_COMMAND;
		execvp(SWAY_COMMAND, argv);
		fprintf(stderr, "Could not run %s\n", SWAY_COMMAND);
	}

	return errno;
}
