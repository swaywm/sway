#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/security.h"
#include "log.h"

static enum secure_feature get_features(int argc, char **argv,
		struct cmd_results **error) {
	enum secure_feature features = 0;

	struct {
		char *name;
		enum secure_feature feature;
	} feature_names[] = {
		{ "lock", FEATURE_LOCK },
		{ "panel", FEATURE_PANEL },
		{ "background", FEATURE_BACKGROUND },
		{ "screenshot", FEATURE_SCREENSHOT },
		{ "fullscreen", FEATURE_FULLSCREEN },
		{ "keyboard", FEATURE_KEYBOARD },
		{ "mouse", FEATURE_MOUSE },
	};

	for (int i = 1; i < argc; ++i) {
		size_t j;
		for (j = 0; j < sizeof(feature_names) / sizeof(feature_names[0]); ++j) {
			if (strcmp(feature_names[j].name, argv[i]) == 0) {
				break;
			}
		}
		if (j == sizeof(feature_names) / sizeof(feature_names[0])) {
			*error = cmd_results_new(CMD_INVALID,
					"permit", "Invalid feature grant %s", argv[i]);
			return 0;
		}
		features |= feature_names[j].feature;
	}
	return features;
}

static struct feature_policy *get_feature_policy(const char *name) {
	struct feature_policy *policy = NULL;
	for (int i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *p = config->feature_policies->items[i];
		if (strcmp(p->program, name) == 0) {
			policy = p;
			break;
		}
	}
	if (!policy) {
		policy = alloc_feature_policy(name);
		if (!policy) {
			sway_abort("Unable to allocate security policy");
		}
		list_add(config->feature_policies, policy);
	}
	return policy;
}

struct cmd_results *cmd_permit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "permit", EXPECTED_MORE_THAN, 1))) {
		return error;
	}
	if ((error = check_security_config())) {
		return error;
	}

	struct feature_policy *policy = get_feature_policy(argv[0]);
	policy->features |= get_features(argc, argv, &error);

	sway_log(L_DEBUG, "Permissions granted to %s for features %d",
			policy->program, policy->features);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_reject(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "reject", EXPECTED_MORE_THAN, 1))) {
		return error;
	}
	if ((error = check_security_config())) {
		return error;
	}

	struct feature_policy *policy = get_feature_policy(argv[0]);
	policy->features &= ~get_features(argc, argv, &error);

	sway_log(L_DEBUG, "Permissions granted to %s for features %d",
			policy->program, policy->features);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
