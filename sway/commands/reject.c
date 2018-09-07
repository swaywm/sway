#include <string.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/security.h"

struct cmd_results *cmd_reject(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "reject", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct feature_policy *policy = get_feature_policy(config, argv[0]);
	for (int i = 1; i < argc; ++i) {
		int j;
		for (j = 0; feature_names[j].name; ++j) {
			if (strcmp(argv[i], feature_names[j].name) == 0) {
				policy->reject_features |= feature_names[j].value;
				break;
			}
		}
		if (!feature_names[j].name) {
			return cmd_results_new(CMD_INVALID, "reject",
					"'%s' is not a valid feature policy", argv[i]);
		}
	}

	wlr_log(WLR_DEBUG, "Rejecting features %08X for %s",
			policy->reject_features, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
