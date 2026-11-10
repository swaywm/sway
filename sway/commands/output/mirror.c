#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

// Returns the source that the output config named `name` mirrors, or NULL if
// it isn't configured to mirror anything.
static const char *configured_source_of(const char *name) {
	for (int i = 0; i < config->output_configs->length; i++) {
		struct output_config *oc = config->output_configs->items[i];
		if (oc->mirror && oc->mirror[0] != '\0' &&
				strcmp(oc->name, name) == 0) {
			return oc->mirror;
		}
	}
	return NULL;
}

// Returns the name of an output (other than `source` itself) configured to
// mirror `source`, or NULL if there is none.
static const char *configured_mirror_of(const char *source) {
	for (int i = 0; i < config->output_configs->length; i++) {
		struct output_config *oc = config->output_configs->items[i];
		if (oc->mirror && oc->mirror[0] != '\0' &&
				strcmp(oc->name, source) != 0 &&
				strcmp(oc->mirror, source) == 0) {
			return oc->name;
		}
	}
	return NULL;
}

struct cmd_results *output_cmd_mirror(int argc, char **argv) {
	struct output_config *oc = config->handler_context.output_config;
	if (!oc) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing mirror argument.");
	}
	if (strcmp(oc->name, "*") == 0) {
		return cmd_results_new(CMD_INVALID,
				"Cannot configure mirroring for all outputs.");
	}

	const char *source = argv[0];
	if (strcasecmp(source, "off") == 0) {
		// Turning mirroring off is always allowed, even if the output isn't
		// mirroring or is itself the source of another output's mirror.
		source = "";
	} else {
		if (source[0] == '\0' || strcmp(source, "*") == 0) {
			return cmd_results_new(CMD_INVALID,
					"Invalid mirror source '%s'.", source);
		}
		if (strcmp(source, oc->name) == 0) {
			return cmd_results_new(CMD_FAILURE,
					"Output %s cannot mirror itself.", oc->name);
		}
		const char *chained = configured_source_of(source);
		if (chained) {
			return cmd_results_new(CMD_FAILURE,
					"Output %s mirrors %s and cannot be a mirror source.",
					source, chained);
		}
		const char *mirror = configured_mirror_of(oc->name);
		if (mirror) {
			return cmd_results_new(CMD_FAILURE,
					"Output %s is the mirror source of %s and cannot "
					"mirror another output.", oc->name, mirror);
		}
	}

	free(oc->mirror);
	oc->mirror = strdup(source);
	if (!oc->mirror) {
		sway_log(SWAY_ERROR, "Failed to allocate mirror source");
		return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
