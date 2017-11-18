#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/security.h"
#include "util.h"
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

struct cmd_results *cmd_permit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "permit", EXPECTED_MORE_THAN, 1))) {
		return error;
	}
	if ((error = check_security_config())) {
		return error;
	}

	bool assign_perms = true;
	char *program = NULL;

	if (!strcmp(argv[0], "*")) {
		program = strdup(argv[0]);
	} else {
		program = resolve_path(argv[0]);
	}
	if (!program) {
		sway_assert(program, "Unable to resolve IPC permit target '%s'."
			" will issue empty policy", argv[0]);
		assign_perms = false;
		program = strdup(argv[0]);
	}

	struct feature_policy *policy = get_feature_policy(program);
	if (policy && assign_perms) {
		policy->features |= get_features(argc, argv, &error);
		sway_log(L_DEBUG, "Permissions granted to %s for features %d",
				policy->program, policy->features);
	}

	free(program);
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

	char *program = NULL;
	if (!strcmp(argv[0], "*")) {
		program = strdup(argv[0]);
	} else {
		program = resolve_path(argv[0]);
	}
	if (!program) {
		// Punt
		sway_log(L_INFO, "Unable to resolve IPC reject target '%s'."
			" Will use provided path", argv[0]);
		program = strdup(argv[0]);
	}

	struct feature_policy *policy = get_feature_policy(program);
	policy->features &= ~get_features(argc, argv, &error);

	sway_log(L_DEBUG, "Permissions granted to %s for features %d",
			policy->program, policy->features);

	free(program);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
