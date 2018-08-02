#ifndef _SWAY_SECURITY_H
#define _SWAY_SECURITY_H
#include <unistd.h>
#include "sway/config.h"

/** Returns a mask of all features this pid is permitted to use */
uint64_t get_feature_policy_mask(struct wl_client *client);

/**
 * Returns the feature policy for a given program. Creates one if it doesn't
 * exist.
 */
struct feature_policy *get_feature_policy(const char *program);

/** Creates a wayland client with a feature policy applied. */
struct wl_client *create_secure_client(struct wl_display *display,
		int fd, const struct feature_policy *policy);

#endif
