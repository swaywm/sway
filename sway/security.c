#define _XOPEN_SOURCE 500
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "sway/config.h"
#include "sway/security.h"
#include "log.h"

static bool validate_ipc_target(const char *program) {
	struct stat sb;

	sway_log(L_DEBUG, "Validating IPC target '%s'", program);

	if (!strcmp(program, "*")) {
		return true;
	}
	if (lstat(program, &sb) == -1) {
		return false;
	}
	if (!S_ISREG(sb.st_mode)) {
		sway_log(L_ERROR,
				"IPC target '%s' MUST be/point at an existing regular file",
				program);
		return false;
	}
	if (sb.st_uid != 0) {
#ifdef NDEBUG
		sway_log(L_ERROR, "IPC target '%s' MUST be owned by root", program);
		return false;
#else
		sway_log(L_INFO, "IPC target '%s' MUST be owned by root (waived for debug build)", program);
		return true;
#endif
	}
	if (sb.st_mode & S_IWOTH) {
		sway_log(L_ERROR, "IPC target '%s' MUST NOT be world writable", program);
		return false;
	}

	return true;
}

struct feature_policy *alloc_feature_policy(const char *program) {
	uint32_t default_policy = 0;

	if (!validate_ipc_target(program)) {
		return NULL;
	}
	for (size_t i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *policy = *(struct feature_policy **)list_get(config->feature_policies, i);
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
			break;
		}
	}

	struct feature_policy *policy = malloc(sizeof(struct feature_policy));
	if (!policy) {
		return NULL;
	}
	policy->program = strdup(program);
	if (!policy->program) {
		free(policy);
		return NULL;
	}
	policy->features = default_policy;

	return policy;
}

struct ipc_policy *alloc_ipc_policy(const char *program) {
	uint32_t default_policy = 0;

	if (!validate_ipc_target(program)) {
		return NULL;
	}
	for (size_t i = 0; i < config->ipc_policies->length; ++i) {
		struct ipc_policy *policy = *(struct ipc_policy **)list_get(config->ipc_policies, i);
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
			break;
		}
	}

	struct ipc_policy *policy = malloc(sizeof(struct ipc_policy));
	if (!policy) {
		return NULL;
	}
	policy->program = strdup(program);
	if (!policy->program) {
		free(policy);
		return NULL;
	}
	policy->features = default_policy;

	return policy;
}

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

static const char *get_pid_exe(pid_t pid) {
#ifdef __FreeBSD__
	const char *fmt = "/proc/%d/file";
#else
	const char *fmt = "/proc/%d/exe";
#endif
	int pathlen = snprintf(NULL, 0, fmt, pid);
	char *path = malloc(pathlen + 1);
	if (path) {
		snprintf(path, pathlen + 1, fmt, pid);
	}

	static char link[2048];

	ssize_t len = !path ? -1 : readlink(path, link, sizeof(link));
	if (len < 0) {
		sway_log(L_INFO,
			"WARNING: unable to read %s for security check. Using default policy.",
			path);
		strcpy(link, "*");
	} else {
		link[len] = '\0';
	}
	free(path);

	return link;
}

struct feature_policy *get_feature_policy(const char *name) {
	struct feature_policy *policy = NULL;

	for (size_t i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *p = *(struct feature_policy **)list_get(config->feature_policies, i);
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
		list_add(config->feature_policies, &policy);
	}
	return policy;
}

uint32_t get_feature_policy_mask(pid_t pid) {
	uint32_t default_policy = 0;
	const char *link = get_pid_exe(pid);

	for (size_t i = 0; i < config->feature_policies->length; ++i) {
		struct feature_policy *policy = *(struct feature_policy **)list_get(config->feature_policies, i);
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
		}
		if (strcmp(policy->program, link) == 0) {
			return policy->features;
		}
	}

	return default_policy;
}

uint32_t get_ipc_policy_mask(pid_t pid) {
	uint32_t default_policy = 0;
	const char *link = get_pid_exe(pid);

	for (size_t i = 0; i < config->ipc_policies->length; ++i) {
		struct ipc_policy *policy = *(struct ipc_policy **)list_get(config->ipc_policies, i);
		if (strcmp(policy->program, "*") == 0) {
			default_policy = policy->features;
		}
		if (strcmp(policy->program, link) == 0) {
			return policy->features;
		}
	}

	return default_policy;
}

uint32_t get_command_policy_mask(const char *cmd) {
	uint32_t default_policy = 0;

	for (size_t i = 0; i < config->command_policies->length; ++i) {
		struct command_policy *policy = *(struct command_policy **)list_get(config->command_policies, i);
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
