#include <strings.h>
#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/wayland.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

static bool is_backend_allowed(struct wlr_backend *backend) {
	if (wlr_backend_is_headless(backend)) {
		return true;
	}
	if (wlr_backend_is_wl(backend)) {
		return true;
	}
#if WLR_HAS_X11_BACKEND
	if (wlr_backend_is_x11(backend)) {
		return true;
	}
#endif
	return false;
}

/**
 * This command is intended for developer use only.
 */
struct cmd_results *output_cmd_unplug(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	const char *oc_name = config->handler_context.output_config->name;
	if (strcmp(oc_name, "*") == 0) {
		return cmd_results_new(CMD_INVALID, "Won't unplug all outputs");
	}

	struct sway_output *sway_output = all_output_by_name_or_id(oc_name);
	if (!sway_output) {
		return cmd_results_new(CMD_INVALID,
			"Cannot unplug unknown output %s", oc_name);
	}

	if (!is_backend_allowed(sway_output->wlr_output->backend)) {
		return cmd_results_new(CMD_INVALID,
			"Can only unplug outputs with headless, wayland or x11 backend");
	}

	wlr_output_destroy(sway_output->wlr_output);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
