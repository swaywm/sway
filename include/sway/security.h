#ifndef _SWAY_SECURITY_H
#define _SWAY_SECURITY_H
#include <unistd.h>
#include "sway/config.h"

/** Returns a mask of all features this client is permitted to use */
uint64_t get_feature_policy_mask(struct wl_client *client);

/** Returns the policy for a command, or creates one if it doesn't exist. */
struct feature_policy *get_feature_policy(
		struct sway_config *config, const char *command);

/** Creates a wayland client with the appropriate feature policy. */
struct wl_client *create_secure_client(struct wl_display *display,
		int fd, const char *command);

bool create_client_socket(int sv[2]);

struct feature_name {
	char *name;
	uint64_t value;
};

extern struct feature_name feature_names[];

#endif
