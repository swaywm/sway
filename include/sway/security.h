#ifndef _SWAY_SECURITY_H
#define _SWAY_SECURITY_H

#include <stdbool.h>
#include <wayland-server.h>
#include "sway/config.h"

bool load_security(struct wl_display *display);
bool check_security_rule(const char *cmd, const char *global);

struct command_policy *alloc_command_policy(const char *command);

#endif
