#ifndef _SWAY_SECURITY_H
#define _SWAY_SECURITY_H
#include <unistd.h>
#include "sway/config.h"

const struct feature_permissions *get_permissions(pid_t pid);
enum command_context get_command_context(const char *cmd);

#endif
