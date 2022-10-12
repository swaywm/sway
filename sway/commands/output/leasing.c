#include "sway/commands.h"
#include "sway/output.h"
#include "log.h"
#include "util.h"

struct cmd_results *output_cmd_leasing(int argc, char **argv) {
	if (!server.drm_lease_manager) {
		return cmd_results_new(CMD_FAILURE,
			"DRM lease manager interface not available");
	}

	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	
	const char *oc_name = config->handler_context.output_config->name;
	if (strcmp(oc_name, "*") == 0) {
		return cmd_results_new(CMD_INVALID,
			"Cannot apply leasing to all outputs");
	}
	
	struct sway_output *sway_output = all_output_by_name_or_id(oc_name);
	if (!sway_output || !sway_output->wlr_output) {
		return cmd_results_new(CMD_FAILURE,
			"Cannot apply leasing to unknown output %s", oc_name);
	}

	if (argc == 0) {
		return cmd_results_new(CMD_INVALID, "Missing leasing argument");
	}

	bool status = true;
	if (parse_boolean(argv[0], status)) {
		if (sway_output->leasing) {
			return cmd_results_new(CMD_INVALID,
				"Output %s is already out for leasing", oc_name);
		}
		
		sway_output->leasing = true;
		output_disable(sway_output);
		wlr_output_layout_remove(root->output_layout, sway_output->wlr_output);
		
		sway_log(SWAY_DEBUG, "Offering output %s for leasing", oc_name);
		wlr_drm_lease_v1_manager_offer_output(server.drm_lease_manager,
			sway_output->wlr_output);
	
		config->handler_context.output_config->enabled = 0;
		
	} else {
		if (!sway_output->leasing) {
			return cmd_results_new(CMD_INVALID,
				"Output %s is already not available for leasing", oc_name);
		}

		sway_log(SWAY_DEBUG, "Withdrawing output %s from leasing", oc_name);
		wlr_drm_lease_v1_manager_withdraw_output(server.drm_lease_manager,
			sway_output->wlr_output);

		sway_output->leasing = false;
		output_enable(sway_output);
		
		config->handler_context.output_config->enabled = 1;
	}

	return NULL;
}
