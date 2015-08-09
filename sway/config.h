#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H

#include <stdint.h>
#include <wlc/wlc.h>
#include "list.h"

struct sway_variable {
    char *name;
    char *value;
};

struct sway_binding {
    list_t *keys;
    uint32_t modifiers;
    char *command;
};

struct sway_mode {
    char *name;
    list_t *bindings;
};

struct sway_config {
    list_t *symbols;
    list_t *modes;
    struct sway_mode *current_mode;

    // Flags
    bool focus_follows_mouse;
    bool mouse_warping;
};

struct sway_config *read_config(FILE *file);
char *do_var_replacement(struct sway_config *config, char *str);

extern struct sway_config *config;

#endif
