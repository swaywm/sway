#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/types/wlr_output.h>
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/server.h"
#include "log.h"

static void create_output(struct wlr_backend *backend, void *data) {
	struct wlr_output **result = data;
	if (*result) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		*result = wlr_wl_output_create(backend);
	} else if (wlr_backend_is_headless(backend)) {
		*result = wlr_headless_add_output(backend, 1920, 1080);
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		*result = wlr_x11_output_create(backend);
	}
#endif
}

/**
 * This command is intended for developer use only.
 */
struct cmd_results *cmd_create_output(int argc, char **argv) {
	sway_assert(wlr_backend_is_multi(server.backend),
			"Expected a multi backend");

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "create_output", EXPECTED_AT_MOST, 1))) {
		return error;
	}
	if (argc > 0 && all_output_by_name_or_id(argv[0])) {
		return cmd_results_new(CMD_INVALID, "Output name already in use");
	}
	struct wlr_output *result = NULL;
	wlr_multi_for_each_backend(server.backend, create_output, &result);

	if (!result) {
		return cmd_results_new(CMD_INVALID,
			"Can only create outputs for Wayland, X11 or headless backends");
	}

	if (argc > 0) {
		wlr_output_set_name(result, argv[0]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
