#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include "sway/commands.h"
#include "sway/server.h"
#include "log.h"

struct create_result {
	bool done;
	struct wlr_output *new_output;
};

static void create_output(struct wlr_backend *backend, void *data) {
	struct create_result *result = data;
	if (result->done) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		result->new_output = wlr_wl_output_create(backend);
		result->done = true;
	} else if (wlr_backend_is_headless(backend)) {
		result->new_output = wlr_headless_add_output(backend, 1920, 1080);
		result->done = true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		result->new_output = wlr_x11_output_create(backend);
		result->done = true;
	}
#endif
}

/**
 * This command is intended for developer use only.
 */
struct cmd_results *cmd_create_output(int argc, char **argv) {
	sway_assert(wlr_backend_is_multi(server.backend),
			"Expected a multi backend");

	struct create_result result = { false, NULL };
	wlr_multi_for_each_backend(server.backend, create_output, &result);

	if (!result.done) {
		return cmd_results_new(CMD_INVALID,
			"Can only create outputs for Wayland, X11 or headless backends");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
