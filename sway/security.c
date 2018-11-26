#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "sway/security.h"

struct command_policy *alloc_command_policy(const char *command) {
	struct command_policy *policy = malloc(sizeof(struct command_policy));
	if (!policy) {
		return NULL;
	}
	policy->command = strdup(command);
	if (!policy->command) {
		free(policy);
		return NULL;
	}
	policy->context = 0;
	return policy;
}
