// sway-save-tree: dump a workspace's tiling tree as JSON for `swaymsg
// append_layout`. Counterpart to i3-save-tree(1).

#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <json.h>
#include "ipc-client.h"
#include "ipc.h"
#include "log.h"

static void usage(void) {
	fprintf(stderr,
		"Usage: sway-save-tree --workspace <name|number>\n"
		"\n"
		"Dump the tiling tree of the named workspace as JSON suitable for\n"
		"`swaymsg append_layout`. Output is sent to stdout.\n"
		"\n"
		"Floating windows are skipped in this release.\n");
}

// Anchored, metachar-escaped copy so swallows match the value literally.
static char *anchored_regex(const char *s) {
	if (!s) {
		return NULL;
	}
	size_t n = strlen(s);
	char *out = malloc(n * 2 + 3);
	if (!out) {
		return NULL;
	}
	size_t pos = 0;
	out[pos++] = '^';
	for (size_t i = 0; i < n; i++) {
		char c = s[i];
		if (strchr(".\\+*?()[]{}|^$", c)) {
			out[pos++] = '\\';
		}
		out[pos++] = c;
	}
	out[pos++] = '$';
	out[pos] = '\0';
	return out;
}

// Returns NULL if the view has nothing identifiable; caller drops the leaf.
static struct json_object *synth_swallows(struct json_object *view_obj) {
	struct json_object *app_id = NULL, *wp = NULL;
	const char *class = NULL, *instance = NULL;

	json_object_object_get_ex(view_obj, "app_id", &app_id);

	if (json_object_object_get_ex(view_obj, "window_properties", &wp)) {
		struct json_object *cls, *inst;
		if (json_object_object_get_ex(wp, "class", &cls)) {
			class = json_object_get_string(cls);
		}
		if (json_object_object_get_ex(wp, "instance", &inst)) {
			instance = json_object_get_string(inst);
		}
	}

	struct json_object *entry = json_object_new_object();
	bool added = false;

	if (app_id && json_object_get_type(app_id) == json_type_string) {
		const char *s = json_object_get_string(app_id);
		if (s && *s) {
			char *re = anchored_regex(s);
			if (re) {
				json_object_object_add(entry, "app_id",
						json_object_new_string(re));
				free(re);
				added = true;
			}
		}
	}
	if (class) {
		char *re = anchored_regex(class);
		if (re) {
			json_object_object_add(entry, "class",
					json_object_new_string(re));
			free(re);
			added = true;
		}
	}
	if (instance) {
		char *re = anchored_regex(instance);
		if (re) {
			json_object_object_add(entry, "instance",
					json_object_new_string(re));
			free(re);
			added = true;
		}
	}

	if (!added) {
		json_object_put(entry);
		return NULL;
	}

	struct json_object *arr = json_object_new_array();
	json_object_array_add(arr, entry);
	return arr;
}

static void copy_key(struct json_object *src, struct json_object *dst,
		const char *key) {
	struct json_object *v;
	if (json_object_object_get_ex(src, key, &v)) {
		json_object_object_add(dst, key, json_object_get(v));
	}
}

static struct json_object *build_layout_node(struct json_object *src) {
	struct json_object *out = json_object_new_object();

	copy_key(src, out, "layout");
	copy_key(src, out, "name");
	copy_key(src, out, "border");
	copy_key(src, out, "current_border_width");
	copy_key(src, out, "percent");
	copy_key(src, out, "marks");

	// Unfilled placeholders carry their original swallows; pass it through.
	struct json_object *swallows;
	if (json_object_object_get_ex(src, "swallows", &swallows) &&
			json_object_is_type(swallows, json_type_array) &&
			json_object_array_length(swallows) > 0) {
		json_object_object_add(out, "swallows", json_object_get(swallows));
		return out;
	}

	struct json_object *nodes;
	bool has_nodes = json_object_object_get_ex(src, "nodes", &nodes) &&
			json_object_is_type(nodes, json_type_array) &&
			json_object_array_length(nodes) > 0;

	if (has_nodes) {
		struct json_object *out_nodes = json_object_new_array();
		size_t n = json_object_array_length(nodes);
		for (size_t i = 0; i < n; i++) {
			struct json_object *child = json_object_array_get_idx(nodes, i);
			struct json_object *built = build_layout_node(child);
			if (built) {
				json_object_array_add(out_nodes, built);
			}
		}
		if (json_object_array_length(out_nodes) == 0) {
			json_object_put(out_nodes);
			json_object_put(out);
			return NULL;
		}
		json_object_object_add(out, "nodes", out_nodes);
		return out;
	}

	struct json_object *synth = synth_swallows(src);
	if (!synth) {
		json_object_put(out);
		return NULL;
	}
	json_object_object_add(out, "swallows", synth);
	return out;
}

// Matches by name; if numeric >= 0, also matches num so "1" finds "1: web".
static struct json_object *find_workspace(struct json_object *node,
		const char *name, int numeric) {
	struct json_object *type;
	if (json_object_object_get_ex(node, "type", &type) &&
			json_object_get_type(type) == json_type_string &&
			strcmp(json_object_get_string(type), "workspace") == 0) {
		struct json_object *ws_name;
		if (json_object_object_get_ex(node, "name", &ws_name) &&
				strcmp(json_object_get_string(ws_name), name) == 0) {
			return node;
		}
		if (numeric >= 0) {
			struct json_object *num;
			if (json_object_object_get_ex(node, "num", &num) &&
					json_object_get_int(num) == numeric) {
				return node;
			}
		}
	}
	struct json_object *nodes;
	if (json_object_object_get_ex(node, "nodes", &nodes) &&
			json_object_is_type(nodes, json_type_array)) {
		size_t n = json_object_array_length(nodes);
		for (size_t i = 0; i < n; i++) {
			struct json_object *child = json_object_array_get_idx(nodes, i);
			struct json_object *match = find_workspace(child, name, numeric);
			if (match) {
				return match;
			}
		}
	}
	return NULL;
}

int main(int argc, char **argv) {
	const char *workspace = NULL;
	static const struct option long_opts[] = {
		{"workspace", required_argument, NULL, 'w'},
		{"help",      no_argument,       NULL, 'h'},
		{0,           0,                 0,    0  },
	};
	int opt;
	while ((opt = getopt_long(argc, argv, "w:h", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'w':
			workspace = optarg;
			break;
		case 'h':
		default:
			usage();
			return opt == 'h' ? 0 : 1;
		}
	}
	if (!workspace) {
		usage();
		return 1;
	}

	char *socket_path = get_socketpath();
	if (!socket_path) {
		fprintf(stderr, "sway-save-tree: cannot find sway IPC socket\n");
		return 1;
	}
	int fd = ipc_open_socket(socket_path);
	free(socket_path);
	struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
	ipc_set_recv_timeout(fd, timeout);
	uint32_t len = 0;
	char *resp = ipc_single_command(fd, IPC_GET_TREE, "", &len);
	if (!resp) {
		fprintf(stderr, "sway-save-tree: GET_TREE IPC failed\n");
		return 1;
	}

	struct json_tokener *tok = json_tokener_new();
	struct json_object *root = json_tokener_parse_ex(tok, resp, len);
	json_tokener_free(tok);
	free(resp);
	if (!root) {
		fprintf(stderr, "sway-save-tree: failed to parse GET_TREE response\n");
		return 1;
	}

	int numeric = -1;
	if (workspace[0] != '\0') {
		char *end = NULL;
		long n = strtol(workspace, &end, 10);
		if (end != workspace && *end == '\0' && n >= 0 && n <= INT_MAX) {
			numeric = (int)n;
		}
	}
	struct json_object *ws = find_workspace(root, workspace, numeric);
	if (!ws) {
		fprintf(stderr, "sway-save-tree: workspace '%s' not found\n",
				workspace);
		json_object_put(root);
		return 1;
	}

	struct json_object *floating;
	if (json_object_object_get_ex(ws, "floating_nodes", &floating) &&
			json_object_is_type(floating, json_type_array) &&
			json_object_array_length(floating) > 0) {
		fprintf(stderr, "sway-save-tree: ignoring %zu floating window(s) on "
				"workspace '%s' (tiling-only in this release)\n",
				json_object_array_length(floating), workspace);
	}

	struct json_object *nodes;
	if (!json_object_object_get_ex(ws, "nodes", &nodes) ||
			!json_object_is_type(nodes, json_type_array) ||
			json_object_array_length(nodes) == 0) {
		fprintf(stderr, "sway-save-tree: workspace '%s' has no tiling "
				"children\n", workspace);
		json_object_put(root);
		return 1;
	}

	struct json_object *out = json_object_new_array();
	size_t n = json_object_array_length(nodes);
	for (size_t i = 0; i < n; i++) {
		struct json_object *child = json_object_array_get_idx(nodes, i);
		struct json_object *built = build_layout_node(child);
		if (built) {
			json_object_array_add(out, built);
		}
	}

	const char *str = json_object_to_json_string_ext(out,
			JSON_C_TO_STRING_PRETTY);
	puts(str);

	json_object_put(out);
	json_object_put(root);
	return 0;
}
