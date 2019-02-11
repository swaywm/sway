#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SWAY_COMMAND "sway"

int main(int argc, char **argv) {
	char *shell = getenv("SHELL");

	if (shell) {
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
