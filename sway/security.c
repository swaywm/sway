#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "sway/security.h"

struct feature_name feature_names[] = {
	{ "data_control_manager", FEATURE_DATA_CONTROL_MGR },
	{ "export_dmabuf_manager", FEATURE_DMABUF_EXPORT },
	{ "screencopy_manager", FEATURE_SCREENCOPY },
	{ "gamma_control", FEATURE_GAMMA_CONTROL },
	{ "input_inhibit", FEATURE_INPUT_INHIBIT },
	{ "layer_shell", FEATURE_LAYER_SHELL },
	{ "virtual_keyboard", FEATURE_VIRTUAL_KEYBOARD },
	{ NULL, 0 },
};

struct feature_policy *get_feature_policy(
		struct sway_config *config, const char *program) {
	if (!program) {
		return &config->default_policy;
	}

	struct feature_policy *policy;
	for (int i = 0; i < config->feature_policies->length; ++i) {
		policy = config->feature_policies->items[i];
		if (strcmp(policy->program, program) == 0) {
			return policy;
		}
	}
	policy = calloc(1, sizeof(struct feature_policy));
	policy->program = strdup(program);
	return policy;
}
