#include <limits.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "list.h"
#include "log.h"

// must be in order for the bsearch
static const struct cmd_handler output_handlers[] = {
	{ "adaptive_sync", output_cmd_adaptive_sync },
	{ "allow_tearing", output_cmd_allow_tearing },
	{ "background", output_cmd_background },
	{ "bg", output_cmd_background },
	{ "color_profile", output_cmd_color_profile },
	{ "disable", output_cmd_disable },
	{ "dpms", output_cmd_dpms },
	{ "enable", output_cmd_enable },
	{ "hdr", output_cmd_hdr },
	{ "max_render_time", output_cmd_max_render_time },
	{ "mode", output_cmd_mode },
	{ "modeline", output_cmd_modeline },
	{ "pos", output_cmd_position },
	{ "position", output_cmd_position },
	{ "power", output_cmd_power },
	{ "render_bit_depth", output_cmd_render_bit_depth },
	{ "res", output_cmd_mode },
	{ "resolution", output_cmd_mode },
	{ "scale", output_cmd_scale },
	{ "scale_filter", output_cmd_scale_filter },
	{ "subpixel", output_cmd_subpixel },
	{ "toggle", output_cmd_toggle },
	{ "transform", output_cmd_transform },
	{ "unplug", output_cmd_unplug },
};

static bool str_eq(const char *a, const char *b) {
	if (!a || !b) {
		return a == b;
	}
	return strcmp(a, b) == 0;
}

// True when merging oc into the stored configs would change nothing, so the
// caller can skip the modeset (and its output event) entirely. Only fields
// actually set in oc are compared, mirroring merge_output_config; anything
// not provably identical counts as a change.
static bool output_config_noop(const struct output_config *oc) {
	for (int i = 0; i < config->output_configs->length; i++) {
		struct output_config *old = config->output_configs->items[i];
		if (strcmp(old->name, oc->name) != 0) {
			continue;
		}
		if (oc->enabled != -1 && old->enabled != oc->enabled) {
			return false;
		}
		if (oc->width != -1 && old->width != oc->width) {
			return false;
		}
		if (oc->height != -1 && old->height != oc->height) {
			return false;
		}
		if (oc->refresh_rate != -1 && old->refresh_rate != oc->refresh_rate) {
			return false;
		}
		if (oc->custom_mode != -1 && old->custom_mode != oc->custom_mode) {
			return false;
		}
		if (oc->drm_mode.type != 0 && oc->drm_mode.type != (uint32_t)-1) {
			return false;
		}
		if (oc->x != INT_MAX && old->x != oc->x) {
			return false;
		}
		if (oc->y != INT_MAX && old->y != oc->y) {
			return false;
		}
		if (oc->scale != -1 && old->scale != oc->scale) {
			return false;
		}
		if (oc->scale_filter != SCALE_FILTER_DEFAULT
				&& old->scale_filter != oc->scale_filter) {
			return false;
		}
		if (oc->subpixel != WL_OUTPUT_SUBPIXEL_UNKNOWN
				&& old->subpixel != oc->subpixel) {
			return false;
		}
		if (oc->transform != -1 && old->transform != oc->transform) {
			return false;
		}
		if (oc->max_render_time != -1 && old->max_render_time != oc->max_render_time) {
			return false;
		}
		if (oc->adaptive_sync != -1 && old->adaptive_sync != oc->adaptive_sync) {
			return false;
		}
		if (oc->render_bit_depth != RENDER_BIT_DEPTH_DEFAULT
				&& old->render_bit_depth != oc->render_bit_depth) {
			return false;
		}
		if (oc->color_profile != COLOR_PROFILE_DEFAULT
				|| oc->color_transform != NULL) {
			return false;
		}
		if (oc->background && !str_eq(old->background, oc->background)) {
			return false;
		}
		if (oc->background_option
				&& !str_eq(old->background_option, oc->background_option)) {
			return false;
		}
		if (oc->background_fallback
				&& !str_eq(old->background_fallback, oc->background_fallback)) {
			return false;
		}
		if (oc->power != -1 && old->power != oc->power) {
			return false;
		}
		if (oc->allow_tearing != -1 && old->allow_tearing != oc->allow_tearing) {
			return false;
		}
		if (oc->hdr != -1 && old->hdr != oc->hdr) {
			return false;
		}
		return true;
	}
	return false;
}

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1);
	if (error != NULL) {
		return error;
	}

	// The HEADLESS-1 output is a dummy output used when there's no outputs
	// connected. It should never be configured.
	if (strcasecmp(argv[0], root->fallback_output->wlr_output->name) == 0) {
		return cmd_results_new(CMD_FAILURE,
				"Refusing to configure the no op output");
	}

	struct output_config *output = NULL;
	if (strcmp(argv[0], "-") == 0 || strcmp(argv[0], "--") == 0) {
		if (config->reading) {
			return cmd_results_new(CMD_FAILURE,
					"Current output alias (%s) cannot be used in the config",
					argv[0]);
		}
		struct sway_output *sway_output = config->handler_context.node ?
			node_get_output(config->handler_context.node) : NULL;
		if (!sway_output) {
			return cmd_results_new(CMD_FAILURE, "Unknown output");
		}
		if (sway_output == root->fallback_output) {
			return cmd_results_new(CMD_FAILURE,
					"Refusing to configure the no op output");
		}
		if (strcmp(argv[0], "-") == 0) {
			output = new_output_config(sway_output->wlr_output->name);
		} else {
			char identifier[128];
			output_get_identifier(identifier, 128, sway_output);
			output = new_output_config(identifier);
		}
	} else {
		output = new_output_config(argv[0]);
	}
	if (!output) {
		sway_log(SWAY_ERROR, "Failed to allocate output config");
		return NULL;
	}
	argc--; argv++;

	config->handler_context.output_config = output;

	while (argc > 0) {
		config->handler_context.leftovers.argc = 0;
		config->handler_context.leftovers.argv = NULL;

		if (find_handler(*argv, output_handlers, sizeof(output_handlers))) {
			error = config_subcommand(argv, argc, output_handlers,
					sizeof(output_handlers));
		} else {
			error = cmd_results_new(CMD_INVALID,
				"Invalid output subcommand: %s.", *argv);
		}

		if (error != NULL) {
			goto fail;
		}

		argc = config->handler_context.leftovers.argc;
		argv = config->handler_context.leftovers.argv;
	}

	config->handler_context.output_config = NULL;
	config->handler_context.leftovers.argc = 0;
	config->handler_context.leftovers.argv = NULL;

	bool background = output->background;

	if (output_config_noop(output)) {
		free_output_config(output);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	store_output_config(output);

	if (config->reading) {
		// When reading the config file, we wait till the end to do a single
		// modeset and swaybg spawn.
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	request_modeset();

	if (background && !spawn_swaybg()) {
		return cmd_results_new(CMD_FAILURE,
			"Failed to apply background configuration");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);

fail:
	config->handler_context.output_config = NULL;
	free_output_config(output);
	return error;
}
