#define _XOPEN_SOURCE 700
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "ipc-client.h"
#include "swaygrab/json.h"

static json_object *tree;

void init_json_tree(int socketfd) {
	uint32_t len = 0;
	char *res = ipc_single_command(socketfd, IPC_GET_TREE, NULL, &len);
	tree = json_tokener_parse(res);
}

void free_json_tree() {
	json_object_put(tree);
}

static bool is_focused(json_object *c) {
	json_object *focused;
	json_object_object_get_ex(c, "focused", &focused);
	return json_object_get_boolean(focused);
}

static json_object *get_focused_container_r(json_object *c) {
	json_object *name;
	json_object_object_get_ex(c, "name", &name);
	if (is_focused(c)) {
		return c;
	} else {
		json_object *nodes, *node, *child;
		json_object_object_get_ex(c, "nodes", &nodes);
		int i;
		for (i = 0; i < json_object_array_length(nodes); i++) {
			node = json_object_array_get_idx(nodes, i);

			if ((child = get_focused_container_r(node))) {
				return child;
			}
		}

		json_object_object_get_ex(c, "floating_nodes", &nodes);
		for (i = 0; i < json_object_array_length(nodes); i++) {
			node = json_object_array_get_idx(nodes, i);

			if ((child = get_focused_container_r(node))) {
				return child;
			}
		}

	}

	return NULL;
}

json_object *get_focused_container() {
	return get_focused_container_r(tree);
}

char *get_focused_output() {
	json_object *outputs, *output, *name;
	json_object_object_get_ex(tree, "nodes", &outputs);

	for (int i = 0; i < json_object_array_length(outputs); i++) {
		output = json_object_array_get_idx(outputs, i);

		if (get_focused_container_r(output)) {
			json_object_object_get_ex(output, "name", &name);
			return strdup(json_object_get_string(name));
		}
	}

	return NULL;
}

char *create_payload(const char *output, struct wlc_geometry *g) {
	char *payload_str = malloc(256);
	json_object *payload = json_object_new_object();

	json_object_object_add(payload, "output", json_object_new_string(output));
	json_object_object_add(payload, "x", json_object_new_int(g->origin.x));
	json_object_object_add(payload, "y", json_object_new_int(g->origin.y));
	json_object_object_add(payload, "w", json_object_new_int(g->size.w));
	json_object_object_add(payload, "h", json_object_new_int(g->size.h));

	snprintf(payload_str, 256, "%s", json_object_to_json_string(payload));
	return strdup(payload_str);
}

struct wlc_geometry *get_container_geometry(json_object *container) {
	struct wlc_geometry *geo = malloc(sizeof(struct wlc_geometry));
	json_object *rect, *x, *y, *w, *h;

	json_object_object_get_ex(container, "rect", &rect);
	json_object_object_get_ex(rect, "x", &x);
	json_object_object_get_ex(rect, "y", &y);
	json_object_object_get_ex(rect, "width", &w);
	json_object_object_get_ex(rect, "height", &h);

	geo->origin.x = json_object_get_int(x);
	geo->origin.y = json_object_get_int(y);
	geo->size.w = json_object_get_int(w);
	geo->size.h = json_object_get_int(h);

	return geo;
}

json_object *get_output_container(const char *output) {
	json_object *outputs, *json_output, *name;
	json_object_object_get_ex(tree, "nodes", &outputs);

	for (int i = 0; i < json_object_array_length(outputs); i++) {
		json_output = json_object_array_get_idx(outputs, i);
		json_object_object_get_ex(json_output, "name", &name);

		if (strcmp(json_object_get_string(name), output) == 0) {
			return json_output;
		}
	}

	return NULL;
}
