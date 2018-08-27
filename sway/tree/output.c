#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/output.h"
#include "sway/tree/workspace.h"
#include "log.h"

static void restore_workspaces(struct sway_container *output) {
	// Workspace output priority
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

	// Saved workspaces
	list_t *saved = root_container.sway_root->saved_workspaces;
	for (int i = 0; i < saved->length; ++i) {
		struct sway_container *ws = saved->items[i];
		container_add_child(output, ws);
		ipc_event_workspace(NULL, ws, "move");
	}
	saved->length = 0;

	output_sort_workspaces(output);
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
		output_begin_destroy(output);
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

static void output_evacuate(struct sway_container *output) {
	if (!output->children->length) {
		return;
	}
	struct sway_container *fallback_output = NULL;
	if (root_container.children->length > 1) {
		fallback_output = root_container.children->items[0];
		if (fallback_output == output) {
			fallback_output = root_container.children->items[1];
		}
	}

	while (output->children->length) {
		struct sway_container *workspace = output->children->items[0];

		container_remove_child(workspace);

		if (workspace_is_empty(workspace)) {
			workspace_begin_destroy(workspace);
			continue;
		}

		struct sway_container *new_output =
			workspace_output_get_highest_available(workspace, output);
		if (!new_output) {
			new_output = fallback_output;
		}

		if (new_output) {
			workspace_output_add_priority(workspace, new_output);
			container_add_child(new_output, workspace);
			output_sort_workspaces(new_output);
			ipc_event_workspace(NULL, workspace, "move");
		} else {
			list_add(root_container.sway_root->saved_workspaces, workspace);
		}
	}
}

void output_destroy(struct sway_container *output) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	if (!sway_assert(output->destroying,
				"Tried to free output which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(output->ntxnrefs == 0, "Tried to free output "
				"which is still referenced by transactions")) {
		return;
	}
	free(output->name);
	free(output->formatted_title);
	wlr_texture_destroy(output->title_focused);
	wlr_texture_destroy(output->title_focused_inactive);
	wlr_texture_destroy(output->title_unfocused);
	wlr_texture_destroy(output->title_urgent);
	list_free(output->children);
	list_free(output->current.children);
	list_free(output->outputs);
	free(output);

	// NOTE: We don't actually destroy the sway_output here
}

static void untrack_output(struct sway_container *con, void *data) {
	struct sway_output *output = data;
	int index = list_find(con->outputs, output);
	if (index != -1) {
		list_del(con->outputs, index);
	}
}

void output_begin_destroy(struct sway_container *output) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	wlr_log(WLR_DEBUG, "OUTPUT: Destroying output '%s'", output->name);
	wl_signal_emit(&output->events.destroy, output);

	output_evacuate(output);

	output->destroying = true;
	container_set_dirty(output);

	root_for_each_container(untrack_output, output->sway_output);

	wl_list_remove(&output->sway_output->mode.link);
	wl_list_remove(&output->sway_output->transform.link);
	wl_list_remove(&output->sway_output->scale.link);
	wl_list_remove(&output->sway_output->damage_destroy.link);
	wl_list_remove(&output->sway_output->damage_frame.link);

	output->sway_output->swayc = NULL;
	output->sway_output = NULL;

	if (output->parent) {
		container_remove_child(output);
	}
}

struct sway_container *output_from_wlr_output(struct wlr_output *output) {
	if (output == NULL) {
		return NULL;
	}
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *o = root_container.children->items[i];
		if (o->type == C_OUTPUT && o->sway_output->wlr_output == output) {
			return o;
		}
	}
	return NULL;
}

void output_for_each_workspace(struct sway_container *output,
		void (*f)(struct sway_container *con, void *data), void *data) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		f(workspace, data);
	}
}

void output_for_each_container(struct sway_container *output,
		void (*f)(struct sway_container *con, void *data), void *data) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		workspace_for_each_container(workspace, f, data);
	}
}

struct sway_container *output_find_workspace(struct sway_container *output,
		bool (*test)(struct sway_container *con, void *data), void *data) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return NULL;
	}
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		if (test(workspace, data)) {
			return workspace;
		}
	}
	return NULL;
}

struct sway_container *output_find_container(struct sway_container *output,
		bool (*test)(struct sway_container *con, void *data), void *data) {
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return NULL;
	}
	struct sway_container *result = NULL;
	for (int i = 0; i < output->children->length; ++i) {
		struct sway_container *workspace = output->children->items[i];
		if ((result = workspace_find_container(workspace, test, data))) {
			return result;
		}
	}
	return NULL;
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	struct sway_container *a = *(void **)_a;
	struct sway_container *b = *(void **)_b;

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

void output_sort_workspaces(struct sway_container *output) {
	list_stable_sort(output->children, sort_workspace_cmp_qsort);
}
