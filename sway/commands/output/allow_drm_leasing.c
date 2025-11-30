#include "sway/commands.h"
#include "sway/output.h"
#include "sway/server.h"
#include "log.h"
#include "util.h"

#if WLR_HAS_DRM_BACKEND
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#endif

struct cmd_results *output_cmd_allow_drm_leasing(int argc, char **argv) {
	if (!server.drm_lease_manager) {
		return cmd_results_new(CMD_FAILURE,
			"DRM lease manager interface not available");
	}

	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	if (argc == 0) {
		return cmd_results_new(CMD_INVALID, "Missing allow_leasing argument");
	}

	const char *oc_name = config->handler_context.output_config->name;
	if (strcmp(oc_name, "*") == 0) {
		return cmd_results_new(CMD_INVALID,
			"Cannot apply leasing to all outputs");
	}

	struct sway_output *sway_output = all_output_by_name_or_id(oc_name);

	if (parse_boolean(argv[0],
			(config->handler_context.output_config->allow_drm_leasing == 1))) {
		config->handler_context.output_config->allow_drm_leasing = 1;

#if WLR_HAS_DRM_BACKEND
		if (sway_output) {
			sway_log(SWAY_DEBUG, "Offering output %s for leasing", oc_name);
			wlr_drm_lease_v1_manager_offer_output(server.drm_lease_manager,
				sway_output->wlr_output);
		}
#endif
	} else {
		config->handler_context.output_config->allow_drm_leasing = 0;

#if WLR_HAS_DRM_BACKEND
		if (sway_output) {
			sway_log(SWAY_DEBUG, "Withdrawing output %s from leasing", oc_name);
			wlr_drm_lease_v1_manager_withdraw_output(server.drm_lease_manager,
				sway_output->wlr_output);
		}
#endif
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
