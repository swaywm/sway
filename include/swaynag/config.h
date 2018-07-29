#ifndef _SWAY_NAGBAR_CONFIG_H
#define _SWAY_NAGBAR_CONFIG_H
#include "swaynag/nagbar.h"
#include "list.h"

int nagbar_parse_options(int argc, char **argv, struct sway_nagbar *nagbar,
		list_t *types, char **config, bool *debug);

char *nagbar_get_config_path(void);

int nagbar_load_config(char *path, struct sway_nagbar *nagbar, list_t *types);

#endif
