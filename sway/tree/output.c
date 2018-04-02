#include <strings.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "log.h"

struct sway_container *container_output_destroy(struct sway_container *output) {
	if (!sway_assert(output, "cannot destroy null output")) {
		return NULL;
	}

	if (output->children->length > 0) {
		// TODO save workspaces when there are no outputs.
		// TODO also check if there will ever be no outputs except for exiting
		// program
		if (root_container.children->length > 1) {
			int p = root_container.children->items[0] == output;
			// Move workspace from this output to another output
			while (output->children->length) {
				struct sway_container *child = output->children->items[0];
				container_remove_child(child);
				container_add_child(root_container.children->items[p], child);
			}
			container_sort_workspaces(root_container.children->items[p]);
			arrange_windows(root_container.children->items[p],
				-1, -1);
		}
	}

	wl_list_remove(&output->sway_output->destroy.link);
	wl_list_remove(&output->sway_output->mode.link);
	wl_list_remove(&output->sway_output->transform.link);
	wl_list_remove(&output->sway_output->scale.link);

	wl_list_remove(&output->sway_output->damage_destroy.link);
	wl_list_remove(&output->sway_output->damage_frame.link);

	wlr_log(L_DEBUG, "OUTPUT: Destroying output '%s'", output->name);
	container_destroy(output);
	return &root_container;
}

struct sway_container *output_by_name(const char *name) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		if (strcasecmp(output->name, name) == 0){
			return output;
		}
	}
	return NULL;
}
