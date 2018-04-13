#define _XOPEN_SOURCE 700
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
