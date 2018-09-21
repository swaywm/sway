#include <wlr/config.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#ifdef WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include "sway/commands.h"
#include "sway/server.h"
#include "log.h"

static void create_output(struct wlr_backend *backend, void *data) {
	bool *done = data;
	if (*done) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		wlr_wl_output_create(backend);
		*done = true;
	}
#ifdef WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		wlr_x11_output_create(backend);
		*done = true;
	}
#endif
}

/**
 * This command is intended for developer use only.
 */
struct cmd_results *cmd_create_output(int argc, char **argv) {
	sway_assert(wlr_backend_is_multi(server.backend),
			"Expected a multi backend");

	bool done = false;
	wlr_multi_for_each_backend(server.backend, create_output, &done);

	if (!done) {
		return cmd_results_new(CMD_INVALID, "create_output",
				"Can only create outputs for Wayland or X11 backends");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
