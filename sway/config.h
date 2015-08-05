#ifndef _SWAY_CONFIG_H
#define _SWAY_CONFIG_H

#include <stdint.h>
#include <wlc/wlc.h>
#include "list.h"

struct sway_binding {
    list_t *keys;
    struct wlc_modifiers modifiers;
    char *command;
};

struct sway_mode {
    char *name;
    list_t *bindings;
};

struct sway_config {
    list_t *symbols;
    list_t *modes;
};

struct sway_config *read_config(FILE *file);

#endif
