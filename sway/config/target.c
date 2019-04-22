#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "sway/config.h"
#include "log.h"
#include <libtouch.h>

struct gesture_target_config *get_gesture_target_config(const char* identifier) {
	int i = list_seq_find(config->gesture_target_configs, gesture_target_identifier_cmp, identifier);
	struct gesture_target_config *cfg = NULL;
	if(i >= 0) {
		cfg = config->gesture_target_configs->items[i];
	} else {
		cfg = create_gesture_target_config(identifier);
		list_add(config->gesture_target_configs, cfg);
	}

	return cfg;
}

struct gesture_target_config *create_gesture_target_config(const char* identifier) {
	struct gesture_target_config *cfg =
		calloc(sizeof(struct gesture_target_config), 1);
	if (!cfg) {
		sway_log(
			SWAY_ERROR, "Could not allocate gesture_target_config");
	}
	cfg->identifier = strdup(identifier);

	return cfg;
}

int gesture_target_identifier_cmp(const void *item, const void *data) {
	const struct gesture_target_config *tc = item;
	const char* identifier = data;

	return strcmp(identifier, tc->identifier);
}
