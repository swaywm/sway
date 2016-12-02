#include <unistd.h>
#include <stdio.h>
#include "sway/config.h"
#include "sway/security.h"
#include "log.h"

struct feature_policy *alloc_feature_policy(const char *program) {
	struct feature_policy *policy = malloc(sizeof(struct feature_policy));
	policy->program = strdup(program);
	policy->features = FEATURE_FULLSCREEN | FEATURE_KEYBOARD | FEATURE_MOUSE;
	return policy;
}

enum secure_feature get_feature_policy(pid_t pid) {
	const char *fmt = "/proc/%d/exe";
	int pathlen = snprintf(NULL, 0, fmt, pid);
	char *path = malloc(pathlen + 1);
	snprintf(path, pathlen + 1, fmt, pid);
	static char link[2048];

	enum secure_feature default_policy =
		FEATURE_FULLSCREEN | FEATURE_KEYBOARD | FEATURE_MOUSE;

	ssize_t len = readlink(path, link, sizeof(link));
	free(path);
	if (len < 0) {
		sway_log(L_INFO,
			"WARNING: unable to read %s for security check. Using default policy.",
			path);
		strcpy(link, "*");
	} else {
		link[len] = '\0';
	}

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
	enum command_context default_policy = CONTEXT_ALL;

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
