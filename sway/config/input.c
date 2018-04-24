#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "sway/config.h"
#include "log.h"

struct input_config *new_input_config(const char* identifier) {
	struct input_config *input = calloc(1, sizeof(struct input_config));
	if (!input) {
		wlr_log(L_DEBUG, "Unable to allocate input config");
		return NULL;
	}
	wlr_log(L_DEBUG, "new_input_config(%s)", identifier);
	if (!(input->identifier = strdup(identifier))) {
		free(input);
		wlr_log(L_DEBUG, "Unable to allocate input config");
		return NULL;
	}

	input->tap = INT_MIN;
	input->drag_lock = INT_MIN;
	input->dwt = INT_MIN;
	input->send_events = INT_MIN;
	input->click_method = INT_MIN;
	input->middle_emulation = INT_MIN;
	input->natural_scroll = INT_MIN;
	input->accel_profile = INT_MIN;
	input->pointer_accel = FLT_MIN;
	input->scroll_method = INT_MIN;
	input->left_handed = INT_MIN;
	input->repeat_delay = INT_MIN;
	input->repeat_rate = INT_MIN;

	return input;
}

void merge_input_config(struct input_config *dst, struct input_config *src) {
	if (src->identifier) {
		free(dst->identifier);
		dst->identifier = strdup(src->identifier);
	}
	if (src->accel_profile != INT_MIN) {
		dst->accel_profile = src->accel_profile;
	}
	if (src->click_method != INT_MIN) {
		dst->click_method = src->click_method;
	}
	if (src->drag_lock != INT_MIN) {
		dst->drag_lock = src->drag_lock;
	}
	if (src->dwt != INT_MIN) {
		dst->dwt = src->dwt;
	}
	if (src->middle_emulation != INT_MIN) {
		dst->middle_emulation = src->middle_emulation;
	}
	if (src->natural_scroll != INT_MIN) {
		dst->natural_scroll = src->natural_scroll;
	}
	if (src->pointer_accel != FLT_MIN) {
		dst->pointer_accel = src->pointer_accel;
	}
	if (src->repeat_delay != INT_MIN) {
		dst->repeat_delay = src->repeat_delay;
	}
	if (src->repeat_rate != INT_MIN) {
		dst->repeat_rate = src->repeat_rate;
	}
	if (src->scroll_method != INT_MIN) {
		dst->scroll_method = src->scroll_method;
	}
	if (src->send_events != INT_MIN) {
		dst->send_events = src->send_events;
	}
	if (src->tap != INT_MIN) {
		dst->tap = src->tap;
	}
	if (src->xkb_layout) {
		free(dst->xkb_layout);
		dst->xkb_layout = strdup(src->xkb_layout);
	}
	if (src->xkb_model) {
		free(dst->xkb_model);
		dst->xkb_model = strdup(src->xkb_model);
	}
	if (src->xkb_options) {
		free(dst->xkb_options);
		dst->xkb_options = strdup(src->xkb_options);
	}
	if (src->xkb_rules) {
		free(dst->xkb_rules);
		dst->xkb_rules = strdup(src->xkb_rules);
	}
	if (src->xkb_variant) {
		free(dst->xkb_variant);
		dst->xkb_variant = strdup(src->xkb_variant);
	}
	if (src->mapped_from_region) {
		free(dst->mapped_from_region);
		dst->mapped_from_region =
			malloc(sizeof(struct input_config_mapped_from_region));
		memcpy(dst->mapped_from_region, src->mapped_from_region,
			sizeof(struct input_config_mapped_from_region));
	}
	if (src->mapped_to_output) {
		free(dst->mapped_to_output);
		dst->mapped_to_output = strdup(src->mapped_to_output);
	}
}

struct input_config *copy_input_config(struct input_config *ic) {
	struct input_config *copy = calloc(1, sizeof(struct input_config));
	if (copy == NULL) {
		wlr_log(L_ERROR, "could not allocate input config");
		return NULL;
	}
	merge_input_config(copy, ic);
	return copy;
}

void free_input_config(struct input_config *ic) {
	if (!ic) {
		return;
	}
	free(ic->identifier);
	free(ic);
}

int input_identifier_cmp(const void *item, const void *data) {
	const struct input_config *ic = item;
	const char *identifier = data;
	return strcmp(ic->identifier, identifier);
}
