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

static struct output_config *get_output_config(char *name, char *identifier) {
	int i = list_seq_find(config->output_configs, output_name_cmp, name);
	if (i >= 0) {
		return config->output_configs->items[i];
	}

	i = list_seq_find(config->output_configs, output_name_cmp, identifier);
	if (i >= 0) {
		return config->output_configs->items[i];
	}

	return NULL;
}

static void apply_output_config_to_outputs(struct output_config *oc) {
	// Try to find the output container and apply configuration now. If
	// this is during startup then there will be no container and config
	// will be applied during normal "new output" event from wlroots.
	bool wildcard = strcmp(oc->name, "*") == 0;
	char id[128];
	struct sway_output *sway_output;
	wl_list_for_each(sway_output, &root_container.sway_root->outputs, link) {
		char *name = sway_output->wlr_output->name;
		output_get_identifier(id, sizeof(id), sway_output);
		if (wildcard || !strcmp(name, oc->name) || !strcmp(id, oc->name)) {
			if (!sway_output->swayc) {
				if (!oc->enabled) {
					if (!wildcard) {
						break;
					}
					continue;
				}

				output_enable(sway_output);
			}

			struct output_config *current = oc;
			if (wildcard) {
				struct output_config *tmp = get_output_config(name, id);
				if (tmp) {
					current = tmp;
				}
			}
			apply_output_config(current, sway_output->swayc);

			if (!wildcard) {
				// Stop looking if the output config isn't applicable to all
				// outputs
				break;
			}
		}
	}
}

struct cmd_results *cmd_output(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "output", EXPECTED_AT_LEAST, 1);
	if (error != NULL) {
		return error;
	}

	struct output_config *output = new_output_config(argv[0]);
	if (!output) {
		wlr_log(WLR_ERROR, "Failed to allocate output config");
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

	output = store_output_config(output);
	apply_output_config_to_outputs(output);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);

fail:
	config->handler_context.output_config = NULL;
	free_output_config(output);
	return error;
}
