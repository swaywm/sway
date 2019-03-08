#define _POSIX_C_SOURCE 200809L
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
#include <json.h>
#include "stringop.h"
#include "ipc-client.h"
#include "log.h"

void sway_terminate(int exit_code) {
	exit(exit_code);
}

static bool success_object(json_object *result) {
	json_object *success;

	if (!json_object_object_get_ex(result, "success", &success)) {
		return true;
	}

	return json_object_get_boolean(success);
}

// Iterate results array and return false if any of them failed
static bool success(json_object *r, bool fallback) {
	if (!json_object_is_type(r, json_type_array)) {
		if (json_object_is_type(r, json_type_object)) {
			return success_object(r);
		}
		return fallback;
	}

	size_t results_len = json_object_array_length(r);
	if (!results_len) {
		return fallback;
	}

	for (size_t i = 0; i < results_len; ++i) {
		json_object *result = json_object_array_get_idx(r, i);

		if (!success_object(result)) {
			return false;
		}
	}

	return true;
}

static void pretty_print_cmd(json_object *r) {
	if (!success_object(r)) {
		json_object *error;
		if (!json_object_object_get_ex(r, "error", &error)) {
			printf("An unknkown error occurred");
		} else {
			printf("Error: %s\n", json_object_get_string(error));
		}
	}
}

static void pretty_print_workspace(json_object *w) {
	json_object *name, *rect, *visible, *output, *urgent, *layout,
				*representation, *focused;
	json_object_object_get_ex(w, "name", &name);
	json_object_object_get_ex(w, "rect", &rect);
	json_object_object_get_ex(w, "visible", &visible);
	json_object_object_get_ex(w, "output", &output);
	json_object_object_get_ex(w, "urgent", &urgent);
	json_object_object_get_ex(w, "layout", &layout);
	json_object_object_get_ex(w, "representation", &representation);
	json_object_object_get_ex(w, "focused", &focused);
	printf(
		"Workspace %s%s%s%s\n"
		"  Output: %s\n"
		"  Layout: %s\n"
		"  Representation: %s\n\n",
		json_object_get_string(name),
		json_object_get_boolean(focused) ? " (focused)" : "",
		!json_object_get_boolean(visible) ? " (off-screen)" : "",
		json_object_get_boolean(urgent) ? " (urgent)" : "",
		json_object_get_string(output),
		json_object_get_string(layout),
		json_object_get_string(representation)
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
	json_object *id, *name, *type, *product, *vendor, *kbdlayout, *libinput;
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
		"  Vendor ID: %d\n";


	printf(fmt, json_object_get_string(name),
		pretty_type_name(json_object_get_string(type)),
		json_object_get_string(id),
		json_object_get_int(product),
		json_object_get_int(vendor));

	if (json_object_object_get_ex(i, "xkb_active_layout_name", &kbdlayout)) {
		const char *layout = json_object_get_string(kbdlayout);
		printf("  Active Keyboard Layout: %s\n", layout ? layout : "(unnamed)");
	}

	if (json_object_object_get_ex(i, "libinput", &libinput)) {
		json_object *events;
		if (json_object_object_get_ex(libinput, "send_events", &events)) {
			printf("  Libinput Send Events: %s\n",
					json_object_get_string(events));
		}
	}

	printf("\n");
}

static void pretty_print_seat(json_object *i) {
	json_object *name, *capabilities, *devices;
	json_object_object_get_ex(i, "name", &name);
	json_object_object_get_ex(i, "capabilities", &capabilities);
	json_object_object_get_ex(i, "devices", &devices);

	const char *fmt =
		"Seat: %s\n"
		"  Capabilities: %d\n";

	printf(fmt, json_object_get_string(name),
		json_object_get_int(capabilities));

	size_t devices_len = json_object_array_length(devices);
	if (devices_len > 0) {
		printf("  Devices:\n");
		for (size_t i = 0; i < devices_len; ++i) {
			json_object *device = json_object_array_get_idx(devices, i);

			json_object *device_name;
			json_object_object_get_ex(device, "name", &device_name);

			printf("    %s\n", json_object_get_string(device_name));
		}
	}

	printf("\n");
}

static void pretty_print_output(json_object *o) {
	json_object *name, *rect, *focused, *active, *ws, *current_mode;
	json_object_object_get_ex(o, "name", &name);
	json_object_object_get_ex(o, "rect", &rect);
	json_object_object_get_ex(o, "focused", &focused);
	json_object_object_get_ex(o, "active", &active);
	json_object_object_get_ex(o, "current_workspace", &ws);
	json_object *make, *model, *serial, *scale, *transform;
	json_object_object_get_ex(o, "make", &make);
	json_object_object_get_ex(o, "model", &model);
	json_object_object_get_ex(o, "serial", &serial);
	json_object_object_get_ex(o, "scale", &scale);
	json_object_object_get_ex(o, "transform", &transform);
	json_object *x, *y;
	json_object_object_get_ex(rect, "x", &x);
	json_object_object_get_ex(rect, "y", &y);
	json_object *modes;
	json_object_object_get_ex(o, "modes", &modes);
	json_object *width, *height, *refresh;
	json_object_object_get_ex(o, "current_mode", &current_mode);
	json_object_object_get_ex(current_mode, "width", &width);
	json_object_object_get_ex(current_mode, "height", &height);
	json_object_object_get_ex(current_mode, "refresh", &refresh);

	if (json_object_get_boolean(active)) {
		printf(
			"Output %s '%s %s %s'%s\n"
			"  Current mode: %dx%d @ %f Hz\n"
			"  Position: %d,%d\n"
			"  Scale factor: %f\n"
			"  Transform: %s\n"
			"  Workspace: %s\n",
			json_object_get_string(name),
			json_object_get_string(make),
			json_object_get_string(model),
			json_object_get_string(serial),
			json_object_get_boolean(focused) ? " (focused)" : "",
			json_object_get_int(width),
			json_object_get_int(height),
			(float)json_object_get_int(refresh) / 1000,
			json_object_get_int(x), json_object_get_int(y),
			json_object_get_double(scale),
			json_object_get_string(transform),
			json_object_get_string(ws)
		);
	} else {
		printf(
			"Output %s '%s %s %s' (inactive)",
			json_object_get_string(name),
			json_object_get_string(make),
			json_object_get_string(model),
			json_object_get_string(serial)
		);
	}

	size_t modes_len = json_object_array_length(modes);
	if (modes_len > 0) {
		printf("  Available modes:\n");
		for (size_t i = 0; i < modes_len; ++i) {
			json_object *mode = json_object_array_get_idx(modes, i);

			json_object *mode_width, *mode_height, *mode_refresh;
			json_object_object_get_ex(mode, "width", &mode_width);
			json_object_object_get_ex(mode, "height", &mode_height);
			json_object_object_get_ex(mode, "refresh", &mode_refresh);

			printf("    %dx%d @ %f Hz\n", json_object_get_int(mode_width),
				json_object_get_int(mode_height),
				(float)json_object_get_int(mode_refresh) / 1000);
		}
	}

	printf("\n");
}

static int binding_cmp(const void *a, const void *b) {
	int cmp = 0;
	json_object *binding_a = *(json_object **)a;
	json_object *binding_b = *(json_object **)b;

	json_object *cmd_a;
	json_object *cmd_b;
	json_object_object_get_ex(binding_a, "command", &cmd_a);
	json_object_object_get_ex(binding_b, "command", &cmd_b);
	const char *cmd_a_str = json_object_get_string(cmd_a);
	const char *cmd_b_str = json_object_get_string(cmd_b);

	list_t *cmd_list_a = split_string(cmd_a_str, " ");
	list_t *cmd_list_b = split_string(cmd_b_str, " ");

	int min_cmd_list_len = (cmd_list_a->length < cmd_list_b->length) ?
		cmd_list_a->length : cmd_list_b->length;
	for (int i = 0; !cmp && i < min_cmd_list_len - 1; ++i) {
		cmp = strcmp(cmd_list_a->items[i], cmd_list_b->items[i]);
	}

	list_free(cmd_list_a);
	list_free(cmd_list_b);

	if (cmp) {
		return cmp;
	}

	json_object *modifiers_a;
	json_object *modifiers_b;

	json_object_object_get_ex(binding_a, "event_state_mask", &modifiers_a);
	json_object_object_get_ex(binding_b, "event_state_mask", &modifiers_b);

	size_t mod_len_a = json_object_array_length(modifiers_a);
	size_t mod_len_b = json_object_array_length(modifiers_b);

	for (size_t i = 0; i < mod_len_a && i < mod_len_b; ++i) {
		json_object *mod_a = json_object_array_get_idx(modifiers_a, i);
		json_object *mod_b = json_object_array_get_idx(modifiers_b, i);
		const char *mod_a_str = json_object_get_string(mod_a);
		const char *mod_b_str = json_object_get_string(mod_b);

		cmp = strcmp(mod_a_str, mod_b_str);
		if (cmp) {
			return cmp;
		}
	}

	if (mod_len_a < mod_len_b) {
		return -1;
	} else if (mod_len_a > mod_len_b) {
		return 1;
	}

	json_object *symbols_a;
	json_object *symbols_b;

	json_object_object_get_ex(binding_a, "symbols", &symbols_a);
	json_object_object_get_ex(binding_b, "symbols", &symbols_b);

	size_t sym_len_a = json_object_array_length(symbols_a);
	size_t sym_len_b = json_object_array_length(symbols_b);

	for (size_t i = 0; i < sym_len_a && i < sym_len_b; ++i) {
		json_object *sym_a = json_object_array_get_idx(symbols_a, i);
		json_object *sym_b = json_object_array_get_idx(symbols_b, i);
		const char *sym_a_str = json_object_get_string(sym_a);
		const char *sym_b_str = json_object_get_string(sym_b);
		cmp = strcmp(sym_a_str, sym_b_str);

		if (cmp) {
			return cmp;
		}
	}

	if (sym_len_a < sym_len_b) {
		return -1;
	} else if (sym_len_a > sym_len_b) {
		return 1;
	}

	json_object *codes_a;
	json_object *codes_b;

	json_object_object_get_ex(binding_a, "input_codes", &codes_a);
	json_object_object_get_ex(binding_b, "input_codes", &codes_b);

	size_t codes_len_a = json_object_array_length(codes_a);
	size_t codes_len_b = json_object_array_length(codes_b);

	for (size_t i = 0; i < codes_len_a && i < codes_len_b; ++i) {
		uint32_t code_a = (uint32_t)json_object_get_int64(
				json_object_array_get_idx(codes_a, i));
		uint32_t code_b = (uint32_t)json_object_get_int64(
				json_object_array_get_idx(codes_b, i));
		if (code_a < code_b) {
			return -1;
		} else if (code_a > code_b) {
			return 1;
		}
	}

	return 0;
}

static void pretty_print_binding(json_object *binding) {
	char keys_str[64];
	size_t keys_len = 0;
	size_t array_len;

	json_object *modifiers;
	json_object_object_get_ex(binding, "event_state_mask", &modifiers);
	array_len = json_object_array_length(modifiers);
	for (size_t i = 0; i < array_len; ++i) {
		json_object *modifier = json_object_array_get_idx(modifiers, i);
		const char *mod_str = json_object_get_string(modifier);
		int len = snprintf(keys_str + keys_len, sizeof(keys_str) - keys_len,
			"%s%s", keys_len ? "+" : "", mod_str);
		if (len < 0) {
			fprintf(stderr, "error printing modifier symbol\n");
			return;
		}
		keys_len += len;
	}

	json_object *symbols;
	json_object_object_get_ex(binding, "symbols", &symbols);
	array_len = json_object_array_length(symbols);
	for (size_t i = 0; i < array_len; ++i) {
		json_object *symbol = json_object_array_get_idx(symbols, i);
		const char *sym_str = json_object_get_string(symbol);
		int len = snprintf(keys_str + keys_len, sizeof(keys_str) - keys_len,
			"%s%s", keys_len ? "+" : "", sym_str);
		if (len < 0) {
			fprintf(stderr, "error printing key symbol\n");
			return;
		}
		keys_len += len;
	}

	json_object *input_codes;
	json_object_object_get_ex(binding, "input_codes", &input_codes);
	array_len = json_object_array_length(input_codes);
	for (size_t i = 0; i < array_len; ++i) {
		json_object *input_code = json_object_array_get_idx(input_codes, i);
		uint32_t code = (uint32_t)json_object_get_int64(input_code);
		int len = snprintf(keys_str + keys_len, sizeof(keys_str) - keys_len,
			"%s%d", keys_len ? "+" : "", code);
		if (len < 0) {
			fprintf(stderr, "error printing input code\n");
			return;
		}
		keys_len += len;
	}

	json_object *command;
	json_object_object_get_ex(binding, "command", &command);
	const char *cmd_str = json_object_get_string(command);
	printf("%-30s %s\n", keys_str, cmd_str);
}

static void pretty_print_bindings(json_object *binding_modes) {
	json_object_object_foreach(binding_modes, mode_name, bindings) {
		printf("%s:\n", mode_name);
		json_object_array_sort(bindings, binding_cmp);
		size_t len = json_object_array_length(bindings);
		for (size_t i = 0; i < len; ++i) {
			printf("  ");
			pretty_print_binding(json_object_array_get_idx(bindings, i));
		}
	}
}

static void pretty_print_version(json_object *v) {
	json_object *ver;
	json_object_object_get_ex(v, "human_readable", &ver);
	printf("sway version %s\n", json_object_get_string(ver));
}

static void pretty_print_config(json_object *c) {
	json_object *config;
	json_object_object_get_ex(c, "config", &config);
	printf("%s\n", json_object_get_string(config));
}

static void pretty_print(int type, json_object *resp) {
	if (type != IPC_COMMAND && type != IPC_GET_WORKSPACES &&
			type != IPC_GET_INPUTS && type != IPC_GET_OUTPUTS &&
			type != IPC_GET_VERSION && type != IPC_GET_SEATS &&
			type != IPC_GET_CONFIG && type != IPC_SEND_TICK &&
			type != IPC_GET_BINDINGS) {
		printf("%s\n", json_object_to_json_string_ext(resp,
			JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
		return;
	}

	if (type == IPC_SEND_TICK) {
		return;
	}

	if (type == IPC_GET_VERSION) {
		pretty_print_version(resp);
		return;
	}

	if (type == IPC_GET_CONFIG) {
		pretty_print_config(resp);
		return;
	}

	if (type == IPC_GET_BINDINGS) {
		pretty_print_bindings(resp);
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
		case IPC_GET_SEATS:
			pretty_print_seat(obj);
			break;
		}
	}
}

int main(int argc, char **argv) {
	static bool quiet = false;
	static bool raw = false;
	static bool monitor = false;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	sway_log_init(SWAY_INFO, NULL);

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"monitor", no_argument, NULL, 'm'},
		{"pretty", no_argument, NULL, 'p'},
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
		"  -m, --monitor          Monitor until killed (-t SUBSCRIBE only)\n"
		"  -p, --pretty           Use pretty output even when not using a tty\n"
		"  -q, --quiet            Be quiet.\n"
		"  -r, --raw              Use raw output even if using a tty\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -t, --type <type>      Specify the message type.\n"
		"  -v, --version          Show the version number and quit.\n";

	raw = !isatty(STDOUT_FILENO);

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hmpqrs:t:v", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'm': // Monitor
			monitor = true;
			break;
		case 'p': // Pretty
			raw = false;
			break;
		case 'q': // Quiet
			quiet = true;
			break;
		case 'r': // Raw
			raw = true;
			break;
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 't': // Type
			cmdtype = strdup(optarg);
			break;
		case 'v':
			fprintf(stdout, "swaymsg version " SWAY_VERSION "\n");
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
	} else if (strcasecmp(cmdtype, "get_seats") == 0) {
		type = IPC_GET_SEATS;
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
	} else if (strcasecmp(cmdtype, "get_binding_modes") == 0) {
		type = IPC_GET_BINDING_MODES;
	} else if (strcasecmp(cmdtype, "get_bindings") == 0) {
		type = IPC_GET_BINDINGS;
	} else if (strcasecmp(cmdtype, "get_config") == 0) {
		type = IPC_GET_CONFIG;
	} else if (strcasecmp(cmdtype, "send_tick") == 0) {
		type = IPC_SEND_TICK;
	} else if (strcasecmp(cmdtype, "subscribe") == 0) {
		type = IPC_SUBSCRIBE;
	} else {
		sway_abort("Unknown message type %s", cmdtype);
	}

	free(cmdtype);

	if (monitor && type != IPC_SUBSCRIBE) {
		sway_log(SWAY_ERROR, "Monitor can only be used with -t SUBSCRIBE");
		free(socket_path);
		return 1;
	}

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
			fprintf(stderr, "ERROR: Could not parse json response from ipc. "
					"This is a bug in sway.");
			printf("%s\n", resp);
			ret = 1;
		} else {
			if (!success(obj, true)) {
				ret = 1;
			}
			if (type != IPC_SUBSCRIBE  || ret != 0) {
				if (raw) {
					printf("%s\n", json_object_to_json_string_ext(obj,
						JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
				} else {
					pretty_print(type, obj);
				}
			}
			json_object_put(obj);
		}
	}
	free(command);
	free(resp);

	if (type == IPC_SUBSCRIBE && ret == 0) {
		do {
			struct ipc_response *reply = ipc_recv_response(socketfd);
			if (!reply) {
				break;
			}

			json_object *obj = json_tokener_parse(reply->payload);
			if (obj == NULL) {
				fprintf(stderr, "ERROR: Could not parse json response from ipc"
						". This is a bug in sway.");
				ret = 1;
				break;
			} else {
				if (raw) {
					printf("%s\n", json_object_to_json_string(obj));
				} else {
					printf("%s\n", json_object_to_json_string_ext(obj,
						JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
				}
				json_object_put(obj);
			}

			free_ipc_response(reply);
		} while (monitor);
	}

	close(socketfd);
	free(socket_path);
	return ret;
}
