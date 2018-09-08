#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
		struct sway_config *config, const char *command) {
	if (!command) {
		return &config->default_policy;
	}

	struct feature_policy *policy;
	for (int i = 0; i < config->feature_policies->length; ++i) {
		policy = config->feature_policies->items[i];
		if (strcmp(policy->command, command) == 0) {
			return policy;
		}
	}
	policy = calloc(1, sizeof(struct feature_policy));
	policy->command = strdup(command);
	return policy;
}

struct wl_client *create_secure_client(struct wl_display *display,
		int fd, const char *command) {
	struct feature_policy *policy;
	int i;
	for (i = 0; i < config->feature_policies->length; ++i) {
		policy = config->feature_policies->items[i];
		if (strcmp(policy->command, command) == 0) {
			break;
		}
	}
	if (i == config->feature_policies->length) {
		policy = &config->default_policy;
	}
	// TODO: Something useful with policy
	struct wl_client *client = wl_client_create(display, fd);
	// TODO: Destroy listener
	return client;
}

bool create_client_socket(int sv[2]) {
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0 || errno == EINVAL) {
		return false;
	}
	int flags;
	if ((flags = fcntl(sv[0], F_GETFD)) == -1) {
		return false;
	}
	if ((fcntl(sv[0], F_SETFD, flags | FD_CLOEXEC)) == -1) {
		return false;
	}
	if ((flags = fcntl(sv[1], F_GETFD)) == -1) {
		return false;
	}
	if ((fcntl(sv[1], F_SETFD, flags | FD_CLOEXEC)) == -1) {
		return false;
	}
	return true;
}
