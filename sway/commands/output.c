#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/layout.h"
#include "list.h"
#include "log.h"

// must be in order for the bsearch
static struct cmd_handler output_handlers[] = {
	{ "background", output_cmd_background },
	{ "bg", output_cmd_background },
	{ "disable", output_cmd_disable },
	{ "dpms", output_cmd_dpms },
	{ "enable", output_cmd_enable },
	{ "mode", output_cmd_mode },
	{ "pos", output_cmd_position },
	{ "position", output_cmd_position },
	{ "res", output_cmd_mode },
	{ "resolution", output_cmd_mode },
	{ "scale", output_cmd_scale },
	{ "transform", output_cmd_transform },
};

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1);
	if (error != NULL) {
		return error;
	}

	struct output_config *output = new_output_config(argv[0]);
	if (!output) {
		wlr_log(L_ERROR, "Failed to allocate output config");
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
			error = cmd_results_new(CMD_INVALID, "output",
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

	int i = list_seq_find(config->output_configs, output_name_cmp, output->name);
	if (i >= 0) {
		// Merge existing config
		struct output_config *current = config->output_configs->items[i];
		merge_output_config(current, output);
		free_output_config(output);
		output = current;
	} else {
		list_add(config->output_configs, output);
	}

	wlr_log(L_DEBUG, "Config stored for output %s (enabled: %d) (%dx%d@%fHz "
		"position %d,%d scale %f transform %d) (bg %s %s) (dpms %d)",
		output->name, output->enabled, output->width, output->height,
		output->refresh_rate, output->x, output->y, output->scale,
		output->transform, output->background, output->background_option, output->dpms_state);

	// Try to find the output container and apply configuration now. If
	// this is during startup then there will be no container and config
	// will be applied during normal "new output" event from wlroots.
	char identifier[128];
	bool all = strcmp(output->name, "*") == 0;
	list_t *sway_outputs = root_container.sway_root->outputs;
	for (int i = 0; i < sway_outputs->length; ++i) {
		struct sway_output *sway_output = sway_outputs->items[i];
		output_get_identifier(identifier, sizeof(identifier), sway_output);
		wlr_log(L_DEBUG, "Checking identifier %s", identifier);
		if (all || strcmp(sway_output->wlr_output->name, output->name) == 0
				|| strcmp(identifier, output->name) == 0) {
			if (!sway_output->swayc) {
				if (!output->enabled) {
					if (!all) {
						break;
					}
					continue;
				}

				output_enable(sway_output);
			}

			apply_output_config(output, sway_output->swayc);

			if (!all) {
				// Stop looking if the output config isn't applicable to all
				// outputs
				break;
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);

fail:
	config->handler_context.output_config = NULL;
	free_output_config(output);
	return error;
}
