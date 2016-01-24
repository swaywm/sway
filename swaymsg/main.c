#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <json-c/json.h>
#include <json-c/printbuf.h>
#include "stringop.h"
#include "ipc-client.h"
#include "readline.h"
#include "log.h"

void sway_terminate(void) {
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	static int quiet = 0;
	static int pretty = 0;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"version", no_argument, NULL, 'v'},
		{"socket", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaymsg [options] [message]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -p, --pretty           Decode the JSON response and format it.\n"
		"  -q, --quiet            Be quiet.\n"
		"  -v, --version          Show the version number and quit.\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -t, --type <type>      Specify the message type.\n";

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hpqvs:t:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'q': // Quiet
			quiet = 1;
			break;
		case 'p':
			pretty = 1;
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
			exit(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
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

	uint32_t type = IPC_COMMAND;

	if (strcasecmp(cmdtype, "command") == 0) {
		type = IPC_COMMAND;
	} else if (strcasecmp(cmdtype, "get_workspaces") == 0) {
		type = IPC_GET_WORKSPACES;
	} else if (strcasecmp(cmdtype, "get_inputs") == 0) {
		type = IPC_GET_INPUTS;
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
	} else {
		sway_abort("Unknown message type %s", cmdtype);
	}
	free(cmdtype);

	char *command = strdup("");
	if (optind < argc) {
		command = join_args(argv + optind, argc - optind);
	}

	int socketfd = ipc_open_socket(socket_path);
	uint32_t len = strlen(command);
	char *resp = ipc_single_command(socketfd, type, command, &len);

	if (pretty) {
		struct printbuf *pb;
		if(!(pb = printbuf_new())) {
			return -1;
		}
    	printbuf_memappend(pb, resp, sizeof(char)*strlen(resp));
		struct json_object *obj = json_tokener_parse(pb->buf);

		const char *resp_json = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY);
		printf("%s\n", resp_json);
		free((char*)resp_json);
		printbuf_free(pb);
	} else if (!quiet) {
		printf("%s\n", resp);
	}
	close(socketfd);

	free(command);
	free(resp);
	free(socket_path);
	return 0;
}
