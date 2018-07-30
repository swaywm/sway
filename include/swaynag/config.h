#ifndef _SWAYNAG_CONFIG_H
#define _SWAYNAG_CONFIG_H
#include "swaynag/swaynag.h"
#include "list.h"

int swaynag_parse_options(int argc, char **argv, struct swaynag *swaynag,
		list_t *types, struct swaynag_type *type, char **config, bool *debug);

char *swaynag_get_config_path(void);

int swaynag_load_config(char *path, struct swaynag *swaynag, list_t *types);

#endif
