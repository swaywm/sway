#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include "stringop.h"
#include "ipc.h"
#include "readline.h"
#include "log.h"

void sway_terminate(void) {
	exit(1);
}

char *get_socketpath(void) {
	FILE *fp = popen("sway --get-socketpath", "r");
	if (!fp) {
		return NULL;
	}
	char *line = read_line(fp);
	pclose(fp);
	return line;
}

int main(int argc, char **argv) {
	static int quiet = 0;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	static struct option long_options[] = {
		{"quiet", no_argument, &quiet, 'q'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{0, 0, 0, 0}
	};

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "qvs:t:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 0: // Flag
			break;
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 't': // Type
			cmdtype = strdup(optarg);
			break;
		case 'v':
#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
			fprintf(stdout, "sway version %s (%s, branch \"%s\")\n", SWAY_GIT_VERSION, SWAY_VERSION_DATE, SWAY_GIT_BRANCH);
#else
			fprintf(stdout, "version not detected\n");
#endif
			exit(0);
			break;
		}
	}

	if (!cmdtype) {
		cmdtype = "command";
	}
	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			sway_abort("Unable to retrieve socket path");
		}
	}

	uint32_t type;

	if (strcasecmp(cmdtype, "command") == 0) {
		type = IPC_COMMAND;
	} else if (strcasecmp(cmdtype, "get_workspaces") == 0) {
		type = IPC_GET_WORKSPACES;
	} else if (strcasecmp(cmdtype, "get_outputs") == 0) {
		type = IPC_GET_OUTPUTS;
	} else if (strcasecmp(cmdtype, "get_tree") == 0) {
		type = IPC_GET_TREE;
	} else if (strcasecmp(cmdtype, "get_marks") == 0) {
		type = IPC_GET_MARKS;
	} else if (strcasecmp(cmdtype, "get_bar_config") == 0) {
		type = IPC_GET_BAR_CONFIG;
	} else if (strcasecmp(cmdtype, "get_version") == 0) {
		type = IPC_GET_VERSION;
	}

	char *command = strdup("");
	if (optind < argc) {
		command = join_args(argv + optind, argc - optind);
	}

	printf("%s", command);
	free(socket_path);
	return 0;
}
