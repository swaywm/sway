#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output_damage.h>
#include "sway/ipc-server.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/output.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "util.h"

static void restore_workspaces(struct sway_output *output) {
	// Workspace output priority
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *other = root->outputs->items[i];
		if (other == output) {
			continue;
		}

		for (int j = 0; j < other->workspaces->length; j++) {
			struct sway_workspace *ws = other->workspaces->items[j];
			struct sway_output *highest =
				workspace_output_get_highest_available(ws, NULL);
			if (highest == output) {
				workspace_detach(ws);
				output_add_workspace(output, ws);
				ipc_event_workspace(NULL, ws, "move");
				j--;
			}
		}
	}

	// Saved workspaces
	for (int i = 0; i < root->saved_workspaces->length; ++i) {
		struct sway_workspace *ws = root->saved_workspaces->items[i];
		output_add_workspace(output, ws);
		ipc_event_workspace(NULL, ws, "move");
	}
	root->saved_workspaces->length = 0;

	output_sort_workspaces(output);
}

struct sway_output *output_create(struct wlr_output *wlr_output) {
	struct sway_output *output = calloc(1, sizeof(struct sway_output));
	node_init(&output->node, N_OUTPUT, output);
	output->wlr_output = wlr_output;
	wlr_output->data = output;

	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&root->all_outputs, &output->link);

	output->workspaces = create_list();
	output->current.workspaces = create_list();

	return output;
}

void output_enable(struct sway_output *output, struct output_config *oc) {
	if (!sway_assert(!output->enabled, "output is already enabled")) {
		return;
	}
	struct wlr_output *wlr_output = output->wlr_output;
	output->enabled = true;
	apply_output_config(oc, output);
	list_add(root->outputs, output);

	output->lx = wlr_output->lx;
	output->ly = wlr_output->ly;
	wlr_output_transformed_resolution(wlr_output,
			&output->width, &output->height);

	restore_workspaces(output);

	if (!output->workspaces->length) {
		// Create workspace
		char *ws_name = workspace_next_name(wlr_output->name);
		wlr_log(WLR_DEBUG, "Creating default workspace %s", ws_name);
		struct sway_workspace *ws = workspace_create(output, ws_name);
		// Set each seat's focus if not already set
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &input_manager->seats, link) {
			if (!seat->has_focus) {
				seat_set_focus_workspace(seat, ws);
			}
		}
		free(ws_name);
	}

	size_t len = sizeof(output->layers) / sizeof(output->layers[0]);
	for (size_t i = 0; i < len; ++i) {
		wl_list_init(&output->layers[i]);
	}
	wl_signal_init(&output->events.destroy);

	input_manager_configure_xcursor(input_manager);

	wl_signal_add(&wlr_output->events.mode, &output->mode);
	wl_signal_add(&wlr_output->events.transform, &output->transform);
	wl_signal_add(&wlr_output->events.scale, &output->scale);
	wl_signal_add(&output->damage->events.frame, &output->damage_frame);
	wl_signal_add(&output->damage->events.destroy, &output->damage_destroy);

	output_add_listeners(output);

	wl_signal_emit(&root->events.new_node, &output->node);

	arrange_layers(output);
	arrange_root();
}

static void output_evacuate(struct sway_output *output) {
	if (!output->workspaces->length) {
		return;
	}
	struct sway_output *fallback_output = NULL;
	if (root->outputs->length > 1) {
		fallback_output = root->outputs->items[0];
		if (fallback_output == output) {
			fallback_output = root->outputs->items[1];
		}
	}

	while (output->workspaces->length) {
		struct sway_workspace *workspace = output->workspaces->items[0];

		workspace_detach(workspace);

		if (workspace_is_empty(workspace)) {
			workspace_begin_destroy(workspace);
			continue;
		}

		struct sway_output *new_output =
			workspace_output_get_highest_available(workspace, output);
		if (!new_output) {
			new_output = fallback_output;
		}

		if (new_output) {
			workspace_output_add_priority(workspace, new_output);
			output_add_workspace(new_output, workspace);
			output_sort_workspaces(new_output);
			ipc_event_workspace(NULL, workspace, "move");
		} else {
			list_add(root->saved_workspaces, workspace);
		}
	}
}

void output_destroy(struct sway_output *output) {
	if (!sway_assert(output->node.destroying,
				"Tried to free output which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(output->wlr_output == NULL,
				"Tried to free output which still had a wlr_output")) {
		return;
	}
	if (!sway_assert(output->node.ntxnrefs == 0, "Tried to free output "
				"which is still referenced by transactions")) {
		return;
	}
	list_free(output->workspaces);
	list_free(output->current.workspaces);
	free(output);
}

static void untrack_output(struct sway_container *con, void *data) {
	struct sway_output *output = data;
	int index = list_find(con->outputs, output);
	if (index != -1) {
		list_del(con->outputs, index);
	}
}

void output_disable(struct sway_output *output) {
	if (!sway_assert(output->enabled, "Expected an enabled output")) {
		return;
	}
	wlr_log(WLR_DEBUG, "Disabling output '%s'", output->wlr_output->name);
	wl_signal_emit(&output->events.destroy, output);

	output_evacuate(output);

	root_for_each_container(untrack_output, output);

	int index = list_find(root->outputs, output);
	list_del(root->outputs, index);

	wl_list_remove(&output->mode.link);
	wl_list_remove(&output->transform.link);
	wl_list_remove(&output->scale.link);
	wl_list_remove(&output->damage_destroy.link);
	wl_list_remove(&output->damage_frame.link);

	output->enabled = false;

	arrange_root();
}

void output_begin_destroy(struct sway_output *output) {
	if (!sway_assert(!output->enabled, "Expected a disabled output")) {
		return;
	}
	wlr_log(WLR_DEBUG, "Destroying output '%s'", output->wlr_output->name);

	output->node.destroying = true;
	node_set_dirty(&output->node);

	wl_list_remove(&output->link);
	wl_list_remove(&output->destroy.link);
	output->wlr_output->data = NULL;
	output->wlr_output = NULL;
}

struct output_config *output_find_config(struct sway_output *output) {
	const char *name = output->wlr_output->name;
	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), output);

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

	return oc;
}

struct sway_output *output_from_wlr_output(struct wlr_output *output) {
	return output->data;
}

struct sway_output *output_get_in_direction(struct sway_output *reference,
		enum movement_direction direction) {
	enum wlr_direction wlr_dir = 0;
	if (!sway_assert(sway_dir_to_wlr(direction, &wlr_dir),
				"got invalid direction: %d", direction)) {
		return NULL;
	}
	int lx = reference->wlr_output->lx + reference->width / 2;
	int ly = reference->wlr_output->ly + reference->height / 2;
	struct wlr_output *wlr_adjacent = wlr_output_layout_adjacent_output(
			root->output_layout, wlr_dir, reference->wlr_output, lx, ly);
	if (!wlr_adjacent) {
		return NULL;
	}
	return output_from_wlr_output(wlr_adjacent);
}

void output_add_workspace(struct sway_output *output,
		struct sway_workspace *workspace) {
	if (workspace->output) {
		workspace_detach(workspace);
	}
	list_add(output->workspaces, workspace);
	workspace->output = output;
	node_set_dirty(&output->node);
	node_set_dirty(&workspace->node);
}

void output_for_each_workspace(struct sway_output *output,
		void (*f)(struct sway_workspace *ws, void *data), void *data) {
	for (int i = 0; i < output->workspaces->length; ++i) {
		struct sway_workspace *workspace = output->workspaces->items[i];
		f(workspace, data);
	}
}

void output_for_each_container(struct sway_output *output,
		void (*f)(struct sway_container *con, void *data), void *data) {
	for (int i = 0; i < output->workspaces->length; ++i) {
		struct sway_workspace *workspace = output->workspaces->items[i];
		workspace_for_each_container(workspace, f, data);
	}
}

struct sway_workspace *output_find_workspace(struct sway_output *output,
		bool (*test)(struct sway_workspace *ws, void *data), void *data) {
	for (int i = 0; i < output->workspaces->length; ++i) {
		struct sway_workspace *workspace = output->workspaces->items[i];
		if (test(workspace, data)) {
			return workspace;
		}
	}
	return NULL;
}

struct sway_container *output_find_container(struct sway_output *output,
		bool (*test)(struct sway_container *con, void *data), void *data) {
	struct sway_container *result = NULL;
	for (int i = 0; i < output->workspaces->length; ++i) {
		struct sway_workspace *workspace = output->workspaces->items[i];
		if ((result = workspace_find_container(workspace, test, data))) {
			return result;
		}
	}
	return NULL;
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	struct sway_workspace *a = *(void **)_a;
	struct sway_workspace *b = *(void **)_b;

	if (isdigit(a->name[0]) && isdigit(b->name[0])) {
		int a_num = strtol(a->name, NULL, 10);
		int b_num = strtol(b->name, NULL, 10);
		return (a_num < b_num) ? -1 : (a_num > b_num);
	} else if (isdigit(a->name[0])) {
		return -1;
	} else if (isdigit(b->name[0])) {
		return 1;
	}
	return 0;
}

void output_sort_workspaces(struct sway_output *output) {
	list_stable_sort(output->workspaces, sort_workspace_cmp_qsort);
}

void output_get_box(struct sway_output *output, struct wlr_box *box) {
	box->x = output->lx;
	box->y = output->ly;
	box->width = output->width;
	box->height = output->height;
}

enum sway_container_layout output_get_default_layout(
		struct sway_output *output) {
	if (config->default_layout != L_NONE) {
		return config->default_layout;
	}
	if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	}
	if (output->height > output->width) {
		return L_VERT;
	}
	return L_HORIZ;
}
