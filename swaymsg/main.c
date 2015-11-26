#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include "stringop.h"
#include "ipc.h"
#include "readline.h"
#include "log.h"

static const char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};
static const size_t ipc_header_size = sizeof(ipc_magic)+8;

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

char *do_ipc(const char *socket_path, uint32_t type, const char *payload, uint32_t len) {
	struct sockaddr_un addr;
	int socketfd;
	if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		sway_abort("Unable to open Unix socket");
	}
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socket_path);
	int l = sizeof(addr.sun_family) + strlen(addr.sun_path);
	if (connect(socketfd, (struct sockaddr *)&addr, l) == -1) {
		sway_abort("Unable to connect to %s", socket_path);
	}

	char data[ipc_header_size];
	uint32_t *data32 = (uint32_t *)(data + sizeof(ipc_magic));
	memcpy(data, ipc_magic, sizeof(ipc_magic));
	data32[0] = len;
	data32[1] = type;

	if (write(socketfd, data, ipc_header_size) == -1) {
		sway_abort("Unable to send IPC header");
	}

	if (write(socketfd, payload, len) == -1) {
		sway_abort("Unable to send IPC payload");
	}

	size_t total = 0;
	while (total < ipc_header_size) {
		ssize_t received = recv(socketfd, data + total, ipc_header_size - total, 0);
		if (received < 0) {
			sway_abort("Unable to receive IPC response");
		}
		total += received;
	}

	total = 0;
	len = data32[0];
	char *response = malloc(len + 1);
	while (total < len) {
		ssize_t received = recv(socketfd, response + total, len - total, 0);
		if (received < 0) {
			sway_abort("Unable to receive IPC response");
		}
		total += received;
	}
	response[len] = '\0';

	close(socketfd);

	return response;
}

int main(int argc, char **argv) {
	static int quiet = 0;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	init_log(L_INFO);

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
		cmdtype = strdup("command");
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
	free(cmdtype);

	char *command = strdup("");
	if (optind < argc) {
		command = join_args(argv + optind, argc - optind);
	}

	char *resp = do_ipc(socket_path, type, command, strlen(command));
	if (!quiet) {
		printf("%s", resp);
	}

	free(command);
	free(resp);
	free(socket_path);
	return 0;
}
