#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include "sway/commands.h"
#include "sway/output.h"
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
	} else if (wlr_backend_is_headless(backend)) {
		wlr_headless_add_output(backend, 1920, 1080);
		*done = true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		wlr_x11_output_create(backend);
		*done = true;
	}
#endif
}

static bool output_is_destroyable(struct wlr_output *output) {
	if (wlr_output_is_wl(output)) {
		return true;
	} else if (wlr_output_is_headless(output)) {
		return true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_output_is_x11(output)) {
		return true;
	}
#endif
	return false;
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
		return cmd_results_new(CMD_INVALID,
			"Can only create outputs for Wayland, X11 or headless backends");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_destroy_output(int argc, char **argv) {
	sway_assert(wlr_backend_is_multi(server.backend),
			"Expected a multi backend");

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "destroy_output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	char *output_name = argv[0];

	struct sway_output *sway_output;
	wl_list_for_each(sway_output, &root->all_outputs, link) {
		if (sway_output == root->fallback_output ||
				strcmp(sway_output->wlr_output->name, output_name) != 0) {
			continue;
		}

		if (!output_is_destroyable(sway_output->wlr_output)) {
			return cmd_results_new(CMD_INVALID,
				"Can only destroy outputs for Wayland, X11 or headless backends");
		}

		wlr_output_destroy(sway_output->wlr_output);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	return cmd_results_new(CMD_INVALID, "No matching output found");
}
