#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"
#include "stringop.h"

static enum sway_container_layout parse_layout(const char *layout) {
	if (!layout) {
		return L_NONE;
	}
	if (strcasecmp(layout, "splith") == 0) {
		return L_HORIZ;
	} else if (strcasecmp(layout, "splitv") == 0) {
		return L_VERT;
	} else if (strcasecmp(layout, "tabbed") == 0) {
		return L_TABBED;
	} else if (strcasecmp(layout, "stacked") == 0 ||
			strcasecmp(layout, "stacking") == 0) {
		return L_STACKED;
	}
	return L_NONE;
}

/**
 * Parse a JSON object representing a container and return a container node.
 * Child containers are also parsed and attached to the parent container.
 */
static struct sway_node *parse_container(json_object *object, char **error) {
	struct sway_container *con = container_create(NULL);
	// TODO:
	// border
	// current_border_width
	// layout
	// floating
	// geometry (box)
	// name
	// percent
	// swallows (app_id, class, instance, title, transient_for)
	// marks
	// fullscreen_mode
	return &con->node;
}

/**
 * Parse a JSON object representing a workspace and return a workspace node.
 * Child containers are also parsed and attached to the workspace.
 */
static struct sway_node *parse_workspace(json_object *object, char **error) {
	json_object *tmp = NULL;

	// name
	json_object_object_get_ex(object, "name", &tmp);
	const char *name = json_object_get_string(tmp);

	if (!name) {
		*error = strdup("Workspace 'name' element is missing");
		return NULL;
	}

	struct sway_workspace *ws = workspace_create(NULL, name);

	// layout
	json_object_object_get_ex(object, "layout", &tmp);
	const char *layout = json_object_get_string(tmp);

	ws->layout = parse_layout(layout);
	if (ws->layout == L_NONE) {
		workspace_begin_destroy(ws);
		int len = strlen("Unrecognized workspace layout ''") + strlen(layout);
		*error = malloc(len + 1);
		sprintf(*error, "Unrecognized workspace layout '%s'", layout);
		return NULL;
	}

	// TODO: Tiling and floating children

	return &ws->node;
}

/**
 * Parse a single JSON object which represents either a workspace or container,
 * as well as all of its children.
 */
static struct sway_node *parse_object(json_object *object, char **error) {
	json_object *tmp = NULL;
	json_object_object_get_ex(object, "type", &tmp);
	const char *type = json_object_get_string(tmp);

	if (type && strcasecmp(type, "con") == 0) {
		return parse_container(object, error);
	} else if (type && strcasecmp(type, "workspace") == 0) {
		return parse_workspace(object, error);
	} else {
		int len = strlen("Unexpected type: ") + strlen(type);
		*error = malloc(len + 1);
		sprintf(*error, "Unexpected type: %s", type);
	}

	return NULL;
}

/**
 * Parse the given null-terminated buffer and return a list of the root children
 * nodes.
 *
 * If an error occurs, the error pointer will point to an error message and the
 * parsed nodes will be returned.
 */
static list_t *parse_buffer(char *buffer, char **error) {
	json_tokener *tokener = json_tokener_new();
	json_object *object = NULL;
	list_t *root_children = create_list(); // Either workspaces or containers
	bool done = false;

	while (!done) {
		json_object_put(object);
		object = json_tokener_parse_ex(tokener, buffer, strlen(buffer));
		enum json_tokener_error err = json_tokener_get_error(tokener);

		switch (err) {
		case json_tokener_success: {
				struct sway_node *node = parse_object(object, error);
				if (*error) {
					done = true;
				}
				list_add(root_children, node);
				buffer += tokener->char_offset;
				json_tokener_reset(tokener);
			}
			break;
		case json_tokener_continue: // No more objects left to read
			done = true;
			break;
		default: { // Error
				const char *tokener_error = json_tokener_error_desc(err);
				int len = strlen("Error parsing JSON: ") + strlen(tokener_error);
				*error = malloc(len + 1);
				sprintf(*error, "Error parsing JSON: %s", tokener_error);
				done = true;
			}
		}
	}

	json_object_put(object);
	json_tokener_free(tokener);

	return root_children;
}

/**
 * Parse the given file and return a list of the root children nodes.
 * The root children will usually be containers, but may be workspaces
 * (if they were dumped with --output).
 *
 * If an error occurs, the error pointer will point to an error message and the
 * parsed nodes will be returned.
 */
static list_t *parse_file(char *filename, char **error) {
	wlr_log(WLR_DEBUG, "Reading '%s'", filename);

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		int len = strlen("Unable to open '' for reading") + strlen(filename);
		*error = malloc(len + 1);
		sprintf(*error, "Unable to open '%s' for reading", filename);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *buffer = malloc(len + 1);
	fread(buffer, len, 1, fp);
	buffer[len] = '\0';
	fclose(fp);

	list_t *root_children = parse_buffer(buffer, error);

	free(buffer);

	return root_children;
}

/**
 * Free everything in the nodes list recursively.
 */
static void destroy_children(list_t *nodes) {
	for (int i = 0; i < nodes->length; ++i) {
		struct sway_node *node = nodes->items[i];
		switch (node->type) {
		case N_CONTAINER:
			if (node->sway_container->children->length) {
				destroy_children(node->sway_container->children);
			}
			container_begin_destroy(node->sway_container);
			break;
		case N_WORKSPACE:
			if (node->sway_workspace->tiling->length) {
				destroy_children(node->sway_workspace->tiling);
			}
			if (node->sway_workspace->floating->length) {
				destroy_children(node->sway_workspace->floating);
			}
			workspace_begin_destroy(node->sway_workspace);
			break;
		case N_OUTPUT:
		case N_ROOT:
			sway_assert(false, "Never reached");
			break;
		}
	}
}

/**
 * Free anything in the root_children list recursively.
 * Used when the command is invalid and we need to discard what we've collected.
 */
static void destroy_root_children(list_t *root_children) {
	if (root_children) {
		destroy_children(root_children);
		list_free(root_children);
	}
}

/**
 * Append all the containers in the nodes list to the given workspace.
 * At this point we know that the items in the nodes list are containers.
 * The containers may contain child containers too, but we don't have to do
 * anything with them here.
 */
static void workspace_append_containers(
		struct sway_workspace *ws, list_t *nodes) {
	for (int i = 0; i < nodes->length; ++i) {
		struct sway_node *node = nodes->items[i];
		struct sway_container *con = node->sway_container;
		workspace_add_tiling(ws, con);
	}
}

/**
 * Append all the workspaces in the nodes list to the given output.
 * At this point we know that the items in the nodes list are workspaces.
 * The workspaces probably contain child containers too, but we don't have to do
 * anything with them here.
 */
static void output_append_workspaces(
		struct sway_output *output, list_t *nodes) {
	for (int i = 0; i < nodes->length; ++i) {
		struct sway_node *node = nodes->items[i];
		struct sway_workspace *ws =
			workspace_by_name(node->sway_workspace->name);
		if (ws) {
			// A workspace already exists with this name, so copy some
			// properties over before destroying our temporary workspace.
			ws->layout = node->sway_workspace->layout;
			list_cat(ws->tiling, node->sway_workspace->tiling);
			list_cat(ws->floating, node->sway_workspace->floating);
			node->sway_workspace->tiling->length = 0;
			node->sway_workspace->floating->length = 0;
			workspace_begin_destroy(node->sway_workspace);
		} else {
			ws = node->sway_workspace;
			output_add_workspace(output, ws);
		}
	}
	output_sort_workspaces(output);
}

struct cmd_results *cmd_append_layout(int argc, char **argv) {
	struct cmd_results *err = NULL;
	if ((err = checkarg(argc, "append_layout", EXPECTED_AT_LEAST, 1))) {
		return err;
	}
	char *error = NULL;
	char *filename = join_args(argv, argc);
	list_t *root_children = parse_file(filename, &error);
	free(filename);

	if (error) {
		struct cmd_results *result = cmd_results_new(
				CMD_INVALID, "append_layout", error);
		free(error);
		destroy_root_children(root_children);
		return result;
	}

	if (root_children->length == 0) {
		list_free(root_children);
		return cmd_results_new(CMD_INVALID, "append_layout",
				"Couldn't find any containers in your layout file");
	}

	// Make sure all the root children are the same type
	bool have_workspace = false;
	bool have_container = false;
	for (int i = 0; i < root_children->length; ++i) {
		struct sway_node *node = root_children->items[i];
		if (node->type == N_CONTAINER) {
			have_container = true;
		} else {
			have_workspace = true;
		}
	}
	if (have_workspace && have_container) {
		destroy_root_children(root_children);
		return cmd_results_new(CMD_INVALID, "append_layout",
				"All root children must be the same type");
	}

	if (have_workspace) {
		struct sway_seat *seat = config->handler_context.seat;
		struct sway_node *focus = seat_get_focus_inactive(seat, &root->node);
		struct sway_output *output = node_get_output(focus);
		output_append_workspaces(output, root_children);
		arrange_output(output);
	} else {
		struct sway_workspace *ws = config->handler_context.workspace;
		workspace_append_containers(ws, root_children);
		arrange_workspace(ws);
	}

	list_free(root_children);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
