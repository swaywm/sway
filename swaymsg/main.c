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

static bool success(json_object *r, bool fallback) {
	json_object *success;
	if (!json_object_object_get_ex(r, "success", &success)) {
		return fallback;
	} else {
		return json_object_get_boolean(success);
	}
}

static void pretty_print_cmd(json_object *r) {
	if (!success(r, true)) {
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

static const char *pretty_type_name(const char *name) {
	// TODO these constants probably belong in the common lib
	struct {
		const char *a;
		const char *b;
	} type_names[] = {
		{ "keyboard", "Keyboard" },
		{ "pointer", "Mouse" },
		{ "tablet_pad", "Tablet pad" },
		{ "tablet_tool", "Tablet tool" },
		{ "touch", "Touch" },
	};

	for (size_t i = 0; i < sizeof(type_names) / sizeof(type_names[0]); ++i) {
		if (strcmp(type_names[i].a, name) == 0) {
			return type_names[i].b;
		}
	}

	return name;
}

static void pretty_print_input(json_object *i) {
	json_object *id, *name, *type, *product, *vendor;
	json_object_object_get_ex(i, "identifier", &id);
	json_object_object_get_ex(i, "name", &name);
	json_object_object_get_ex(i, "type", &type);
	json_object_object_get_ex(i, "product", &product);
	json_object_object_get_ex(i, "vendor", &vendor);

	const char *fmt =
		"Input device: %s\n"
		"  Type: %s\n"
		"  Identifier: %s\n"
		"  Product ID: %d\n"
		"  Vendor ID: %d\n\n";


	printf(fmt, json_object_get_string(name),
		pretty_type_name(json_object_get_string(type)),
		json_object_get_string(id),
		json_object_get_int(product),
		json_object_get_int(vendor));
}

static void pretty_print_output(json_object *o) {
	json_object *name, *rect, *focused, *active, *ws;
	json_object_object_get_ex(o, "name", &name);
	json_object_object_get_ex(o, "rect", &rect);
	json_object_object_get_ex(o, "focused", &focused);
	json_object_object_get_ex(o, "active", &active);
	json_object_object_get_ex(o, "current_workspace", &ws);
	json_object *make, *model, *serial, *scale, *refresh, *transform;
	json_object_object_get_ex(o, "make", &make);
	json_object_object_get_ex(o, "model", &model);
	json_object_object_get_ex(o, "serial", &serial);
	json_object_object_get_ex(o, "scale", &scale);
	json_object_object_get_ex(o, "refresh", &refresh);
	json_object_object_get_ex(o, "transform", &transform);
	json_object *x, *y, *width, *height;
	json_object_object_get_ex(rect, "x", &x);
	json_object_object_get_ex(rect, "y", &y);
	json_object_object_get_ex(rect, "width", &width);
	json_object_object_get_ex(rect, "height", &height);
	printf(
		"Output %s '%s %s %s'%s%s\n"
		"  Mode: %dx%d @ %f Hz\n"
		"  Position: %d,%d\n"
		"  Scale factor: %dx\n"
		"  Transform: %s\n"
		"  Workspace: %s\n\n",
		json_object_get_string(name),
		json_object_get_string(make),
		json_object_get_string(model),
		json_object_get_string(serial),
		json_object_get_boolean(focused) ? " (focused)" : "",
		!json_object_get_boolean(active) ? " (inactive)" : "",
		json_object_get_int(width), json_object_get_int(height),
		(float)json_object_get_int(refresh) / 1000,
		json_object_get_int(x), json_object_get_int(y),
		json_object_get_int(scale),
		json_object_get_string(transform),
		json_object_get_string(ws)
	);
}

static void pretty_print_version(json_object *v) {
	json_object *ver;
	json_object_object_get_ex(v, "human_readable", &ver);
	printf("sway version %s\n", json_object_get_string(ver));
}

static void pretty_print_clipboard(json_object *v) {
	if (success(v, true)) {
		if (json_object_is_type(v, json_type_array)) {
			for (size_t i = 0; i < json_object_array_length(v); ++i) {
				json_object *o = json_object_array_get_idx(v, i);
				printf("%s\n", json_object_get_string(o));
			}
		} else {
			// NOTE: could be extended to print all received types
			// instead just the first one when sways ipc server
			// supports it
			struct json_object_iterator iter = json_object_iter_begin(v);
			struct json_object_iterator end = json_object_iter_end(v);
			if (!json_object_iter_equal(&iter, &end)) {
				json_object *obj = json_object_iter_peek_value(&iter);
				if (success(obj, false)) {
					json_object *content;
					json_object_object_get_ex(obj, "content", &content);
					printf("%s\n", json_object_get_string(content));
				} else {
					json_object *error;
					json_object_object_get_ex(obj, "error", &error);
					printf("Error: %s\n", json_object_get_string(error));
				}
			}
		}
	} else {
		json_object *error;
		json_object_object_get_ex(v, "error", &error);
		printf("Error: %s\n", json_object_get_string(error));
	}
}

static void pretty_print(int type, json_object *resp) {
	if (type != IPC_COMMAND && type != IPC_GET_WORKSPACES &&
			type != IPC_GET_INPUTS && type != IPC_GET_OUTPUTS &&
			type != IPC_GET_VERSION && type != IPC_GET_CLIPBOARD) {
		printf("%s\n", json_object_to_json_string_ext(resp,
			JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
		return;
	}

	if (type == IPC_GET_VERSION) {
		pretty_print_version(resp);
		return;
	}

	if (type == IPC_GET_CLIPBOARD) {
		pretty_print_clipboard(resp);
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
			fprintf(stdout, "sway version " SWAY_VERSION "\n");
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
	} else if (strcasecmp(cmdtype, "get_clipboard") == 0) {
		type = IPC_GET_CLIPBOARD;
	} else {
		sway_abort("Unknown message type %s", cmdtype);
	}
	free(cmdtype);

	char *command = NULL;
	if (optind < argc) {
		command = join_args(argv + optind, argc - optind);
	} else {
		command = strdup("");
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
			if (!success(obj, true)) {
				ret = 1;
			}
			if (raw) {
				printf("%s\n", json_object_to_json_string_ext(obj,
					JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
			} else {
				pretty_print(type, obj);
			}
			json_object_put(obj);
		}
	}
	close(socketfd);

	free(command);
	free(resp);
	free(socket_path);
	return ret;
}
