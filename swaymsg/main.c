#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <ctype.h>
#include <unistd.h>
#include <json-c/json.h>
#include "stringop.h"
#include "ipc-client.h"
#include "readline.h"
#include "log.h"

void sway_terminate(int exit_code) {
	exit(exit_code);
}

static void pretty_print_cmd(json_object *r) {
	bool _success;
	json_object *success;
	if (!json_object_object_get_ex(r, "success", &success)) {
		_success = true;
	} else {
		_success = json_object_get_boolean(success);
	}
	if (!_success) {
		json_object *error;
		if (!json_object_object_get_ex(r, "error", &error)) {
			printf("An unknkown error occured");
		} else {
			printf("Error: %s\n", json_object_get_string(error));
		}
	}
}

static void pretty_print_workspace(json_object *w) {
	json_object *name, *rect, *visible, *output, *urgent, *layout, *focused;
	json_object_object_get_ex(w, "name", &name);
	json_object_object_get_ex(w, "rect", &rect);
	json_object_object_get_ex(w, "visible", &visible);
	json_object_object_get_ex(w, "output", &output);
	json_object_object_get_ex(w, "urgent", &urgent);
	json_object_object_get_ex(w, "layout", &layout);
	json_object_object_get_ex(w, "focused", &focused);
	printf(
		"Workspace %s%s%s%s\n"
		"  Output: %s\n"
		"  Layout: %s\n\n",
		json_object_get_string(name),
		json_object_get_boolean(focused) ? " (focused)" : "",
		!json_object_get_boolean(visible) ? " (off-screen)" : "",
		json_object_get_boolean(urgent) ? " (urgent)" : "",
		json_object_get_string(output),
		json_object_get_string(layout)
	);
}

static void pretty_print_input(json_object *i) {
	json_object *id, *name, *size, *caps;
	json_object_object_get_ex(i, "identifier", &id);
	json_object_object_get_ex(i, "name", &name);
	json_object_object_get_ex(i, "size", &size);
	json_object_object_get_ex(i, "capabilities", &caps);

	printf( "Input device %s\n  Type: ", json_object_get_string(name));

	struct {
		const char *a;
		const char *b;
	} cap_names[] = {
		{ "keyboard", "Keyboard" },
		{ "pointer", "Mouse" },
		{ "touch", "Touch" },
		{ "tablet_tool", "Tablet tool" },
		{ "tablet_pad", "Tablet pad" },
		{ "gesture", "Gesture" },
		{ "switch", "Switch" },
	};

	size_t len = json_object_array_length(caps);
	if (len == 0) {
		printf("Unknown");
	}

	json_object *cap;
	for (size_t i = 0; i < len; ++i) {
		cap = json_object_array_get_idx(caps, i);
		const char *cap_s = json_object_get_string(cap);
		const char *_name = NULL;
		for (size_t j = 0; j < sizeof(cap_names) / sizeof(cap_names[0]); ++i) {
			if (strcmp(cap_names[i].a, cap_s) == 0) {
				_name = cap_names[i].b;
				break;
			}
		}
		printf("%s%s", _name ? _name : cap_s, len > 1 && i != len - 1 ? ", " : "");
	}
	printf("\n  Sway ID: %s\n", json_object_get_string(id));
	if (size) {
		json_object *width, *height;
		json_object_object_get_ex(size, "width", &width);
		json_object_object_get_ex(size, "height", &height);
		printf("  Size: %lfmm x %lfmm\n",
				json_object_get_double(width), json_object_get_double(height));
	}
	printf("\n");
}

static void pretty_print_output(json_object *o) {
	json_object *name, *rect, *focused, *active, *ws, *scale;
	json_object_object_get_ex(o, "name", &name);
	json_object_object_get_ex(o, "rect", &rect);
	json_object_object_get_ex(o, "focused", &focused);
	json_object_object_get_ex(o, "active", &active);
	json_object_object_get_ex(o, "current_workspace", &ws);
	json_object_object_get_ex(o, "scale", &scale);
	json_object *x, *y, *width, *height;
	json_object_object_get_ex(rect, "x", &x);
	json_object_object_get_ex(rect, "y", &y);
	json_object_object_get_ex(rect, "width", &width);
	json_object_object_get_ex(rect, "height", &height);
	printf(
		"Output %s%s%s\n"
		"  Geometry: %dx%d @ %d,%d\n"
		"  Scale factor: %dx\n"
		"  Workspace: %s\n\n",
		json_object_get_string(name),
		json_object_get_boolean(focused) ? " (focused)" : "",
		!json_object_get_boolean(active) ? " (inactive)" : "",
		json_object_get_int(width), json_object_get_int(height),
		json_object_get_int(x), json_object_get_int(y),
		json_object_get_int(scale),
		json_object_get_string(ws)
	);
}

static void pretty_print_version(json_object *v) {
	json_object *ver;
	json_object_object_get_ex(v, "human_readable", &ver);
	printf("sway version %s\n", json_object_get_string(ver));
}

static void pretty_print(int type, json_object *resp) {
	if (type != IPC_COMMAND && type != IPC_GET_WORKSPACES &&
			type != IPC_GET_INPUTS && type != IPC_GET_OUTPUTS &&
			type != IPC_GET_VERSION) {
		printf("%s\n", json_object_to_json_string_ext(resp,
			JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
		return;
	}

	if (type == IPC_GET_VERSION) {
		pretty_print_version(resp);
		return;
	}

	json_object *obj;
	size_t len = json_object_array_length(resp);
	for (size_t i = 0; i < len; ++i) {
		obj = json_object_array_get_idx(resp, i);
		switch (type) {
		case IPC_COMMAND:
			pretty_print_cmd(obj);
			break;
		case IPC_GET_WORKSPACES:
			pretty_print_workspace(obj);
			break;
		case IPC_GET_INPUTS:
			pretty_print_input(obj);
			break;
		case IPC_GET_OUTPUTS:
			pretty_print_output(obj);
			break;
		}
	}
}

int main(int argc, char **argv) {
	static int quiet = 0;
	static int raw = 0;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	init_log(L_INFO);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"raw", no_argument, NULL, 'r'},
		{"socket", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaymsg [options] [message]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -q, --quiet            Be quiet.\n"
		"  -r, --raw              Use raw output even if using a tty\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -t, --type <type>      Specify the message type.\n"
		"  -v, --version          Show the version number and quit.\n";

	raw = !isatty(STDOUT_FILENO);

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hqrs:t:v", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'q': // Quiet
			quiet = 1;
			break;
		case 'r': // Raw
			raw = 1;
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

	int ret = 0;
	int socketfd = ipc_open_socket(socket_path);
	uint32_t len = strlen(command);
	char *resp = ipc_single_command(socketfd, type, command, &len);
	if (!quiet) {
		// pretty print the json
		json_object *obj = json_tokener_parse(resp);

		if (obj == NULL) {
			fprintf(stderr, "ERROR: Could not parse json response from ipc. This is a bug in sway.");
			printf("%s\n", resp);
			ret = 1;
		} else {
			if (raw) {
				printf("%s\n", json_object_to_json_string_ext(obj,
					JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
			} else {
				pretty_print(type, obj);
			}
			free(obj);
		}
	}
	close(socketfd);

	free(command);
	free(resp);
	free(socket_path);
	return ret;
}
