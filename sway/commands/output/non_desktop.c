#include <strings.h>
#include <wlr/types/wlr_output_damage.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/server.h"
#include "log.h"
#include "util.h"

static int find_non_desktop_output(const void *item, const void *data)
{
	const struct sway_output_non_desktop *output = item;
	const char *name = data;
	return strcmp(output->wlr_output->name, name);
}

static int find_output(const void *item, const void *data)
{
	const struct sway_output *output = item;
	const char *name = data;
	return strcmp(output->wlr_output->name, name);
}

struct cmd_results *output_cmd_non_desktop(int argc, char **argv) {
	if (argc == 0) {
		return cmd_results_new(CMD_INVALID, "Missing non_desktop argument");
	}

	struct output_config *oc = config->handler_context.output_config;

	if (strcmp(oc->name, "*") == 0) {
		return cmd_results_new(CMD_INVALID,
			"Cannot apply non_desktop to all outputs");
	}

	if (parse_boolean(argv[0], true)) {
		int index = list_seq_find(root->outputs, find_output, oc->name);
		if (index == -1) {
			return cmd_results_new(CMD_FAILURE, "unknown output %s ", oc->name);
		}

		struct sway_output *output = root->outputs->items[index];
		struct wlr_output *wlr_output = output->wlr_output;
		wlr_output->non_desktop = true;

		sway_log(SWAY_DEBUG, "output %s (%p) non-desktop on", wlr_output->name,
			wlr_output);

		output_disable(output);
		wlr_output_layout_remove(root->output_layout, wlr_output);

		struct sway_output_non_desktop *non_desktop =
			output_non_desktop_create(wlr_output);
		list_add(root->non_desktop_outputs, non_desktop);
		
		if (server.drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(server.drm_lease_manager,
				wlr_output);
		}

	} else {
		int index = list_seq_find(root->non_desktop_outputs,
			find_non_desktop_output, oc->name);
		if (index == -1) {
			return cmd_results_new(CMD_FAILURE, "unknown non-desktop output %s",
				oc->name);
		}
		struct sway_output_non_desktop *non_desktop =
			root->non_desktop_outputs->items[index];
		struct wlr_output *wlr_output = non_desktop->wlr_output;

		wlr_output->non_desktop = false;

		sway_log(SWAY_DEBUG, "output %s (%p) non-desktop off", wlr_output->name,
			wlr_output);
		
		if (server.drm_lease_manager) {
			wlr_drm_lease_v1_manager_withdraw_output(server.drm_lease_manager,
				wlr_output);
		}

		struct sway_output *output = wlr_output->data;
		if (output) {
			output_enable(output);
		} else {
			struct sway_output *output = output_create(wlr_output);
			if (!output) {
				sway_log(SWAY_ERROR, "failed to allocate wlr_output");
				wlr_output->non_desktop = true;
				if (server.drm_lease_manager) {
					wlr_drm_lease_v1_manager_withdraw_output(
						server.drm_lease_manager, wlr_output);
				}
				return cmd_results_new(CMD_FAILURE, "failed to create output");;
			}

			output_init(output, &server);
		}

		list_del(root->non_desktop_outputs, index);
		wl_list_remove(&non_desktop->destroy.link);
		free(non_desktop);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
