#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/output.h"
#include "sway/tree/workspace.h"
#include "log.h"

static void restore_workspaces(struct sway_container *output) {
	for (int i = 0; i < root_container.children->length; i++) {
		struct sway_container *other = root_container.children->items[i];
		if (other == output) {
			continue;
		}

		for (int j = 0; j < other->children->length; j++) {
			struct sway_container *ws = other->children->items[j];
			struct sway_container *highest =
				workspace_output_get_highest_available(ws, NULL);
			if (highest == output) {
				container_remove_child(ws);
				container_add_child(output, ws);
				ipc_event_workspace(NULL, ws, "move");
				j--;
			}
		}
	}

	container_sort_workspaces(output);
}

struct sway_container *output_create(
		struct sway_output *sway_output) {
	const char *name = sway_output->wlr_output->name;
	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), sway_output);

	struct output_config *oc = NULL, *all = NULL;
	for (int i = 0; i < config->output_configs->length; ++i) {
		struct output_config *cur = config->output_configs->items[i];

		if (strcasecmp(name, cur->name) == 0 ||
				strcasecmp(identifier, cur->name) == 0) {
			wlr_log(WLR_DEBUG, "Matched output config for %s", name);
			oc = cur;
		}
		if (strcasecmp("*", cur->name) == 0) {
			wlr_log(WLR_DEBUG, "Matched wildcard output config for %s", name);
			all = cur;
		}

		if (oc && all) {
			break;
		}
	}
	if (!oc) {
		oc = all;
	}

	if (oc && !oc->enabled) {
		return NULL;
	}

	struct sway_container *output = container_create(C_OUTPUT);
	output->sway_output = sway_output;
	output->name = strdup(name);
	if (output->name == NULL) {
		container_destroy(output);
		return NULL;
	}

	apply_output_config(oc, output);
	container_add_child(&root_container, output);
	load_swaybars();

	struct wlr_box size;
	wlr_output_effective_resolution(sway_output->wlr_output, &size.width,
		&size.height);
	output->width = size.width;
	output->height = size.height;

	restore_workspaces(output);

	if (!output->children->length) {
		// Create workspace
		char *ws_name = workspace_next_name(output->name);
		wlr_log(WLR_DEBUG, "Creating default workspace %s", ws_name);
		struct sway_container *ws = workspace_create(output, ws_name);
		// Set each seat's focus if not already set
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &input_manager->seats, link) {
			if (!seat->has_focus) {
				seat_set_focus(seat, ws);
			}
		}
		free(ws_name);
	}

	container_create_notify(output);
	return output;
}

