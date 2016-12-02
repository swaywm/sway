#ifndef _SWAY_SECURITY_H
#define _SWAY_SECURITY_H
#include <unistd.h>
#include "sway/config.h"

enum secure_features get_feature_policy(pid_t pid);
enum command_context get_command_policy(const char *cmd);

#endif
