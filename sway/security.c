#include <unistd.h>
#include <stdio.h>
#include "sway/config.h"
#include "sway/security.h"
#include "log.h"

struct feature_policy *alloc_feature_policy(const char *program) {
	uint32_t default_policy = 0;
	for (int i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *policy = config->feature_policies->items[i];
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
			break;
		}
	}

	struct feature_policy *policy = malloc(sizeof(struct feature_policy));
	policy->program = strdup(program);
	policy->features = default_policy;
	return policy;
}

struct command_policy *alloc_command_policy(const char *command) {
	struct command_policy *policy = malloc(sizeof(struct command_policy));
	policy->command = strdup(command);
	policy->context = 0;
	return policy;
}

enum secure_feature get_feature_policy(pid_t pid) {
	const char *fmt = "/proc/%d/exe";
	int pathlen = snprintf(NULL, 0, fmt, pid);
	char *path = malloc(pathlen + 1);
	snprintf(path, pathlen + 1, fmt, pid);
	static char link[2048];

	uint32_t default_policy = 0;

	ssize_t len = readlink(path, link, sizeof(link));
	if (len < 0) {
		sway_log(L_INFO,
			"WARNING: unable to read %s for security check. Using default policy.",
			path);
		strcpy(link, "*");
	} else {
		link[len] = '\0';
	}
	free(path);

	for (int i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *policy = config->feature_policies->items[i];
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
		}
		if (strcmp(policy->program, link) == 0) {
			return policy->features;
		}
	}

	return default_policy;
}

enum command_context get_command_policy(const char *cmd) {
	uint32_t default_policy = 0;

	for (int i = 0; i < config->command_policies->length; ++i) {
		struct command_policy *policy = config->command_policies->items[i];
		if (strcmp(policy->command, "*") == 0) {
			default_policy = policy->context;
		}
		if (strcmp(policy->command, cmd) == 0) {
			return policy->context;
		}
	}

	return default_policy;
}

const char *command_policy_str(enum command_context context) {
	switch (context) {
		case CONTEXT_ALL:
			return "all";
		case CONTEXT_CONFIG:
			return "config";
		case CONTEXT_BINDING:
			return "binding";
		case CONTEXT_IPC:
			return "IPC";
		case CONTEXT_CRITERIA:
			return "criteria";
		default:
			return "unknown";
	}
}
