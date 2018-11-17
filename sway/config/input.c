#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "sway/config.h"
#include "log.h"

struct input_config *new_input_config(const char* identifier) {
	struct input_config *input = calloc(1, sizeof(struct input_config));
	if (!input) {
		wlr_log(WLR_DEBUG, "Unable to allocate input config");
		return NULL;
	}
	wlr_log(WLR_DEBUG, "new_input_config(%s)", identifier);
	if (!(input->identifier = strdup(identifier))) {
		free(input);
		wlr_log(WLR_DEBUG, "Unable to allocate input config");
		return NULL;
	}

	input->tap = INT_MIN;
	input->tap_button_map = INT_MIN;
	input->drag = INT_MIN;
	input->drag_lock = INT_MIN;
	input->dwt = INT_MIN;
	input->send_events = INT_MIN;
	input->click_method = INT_MIN;
	input->middle_emulation = INT_MIN;
	input->natural_scroll = INT_MIN;
	input->accel_profile = INT_MIN;
	input->pointer_accel = FLT_MIN;
	input->scroll_factor = FLT_MIN;
	input->scroll_button = INT_MIN;
	input->scroll_method = INT_MIN;
	input->left_handed = INT_MIN;
	input->repeat_delay = INT_MIN;
	input->repeat_rate = INT_MIN;
	input->xkb_numlock = INT_MIN;
	input->xkb_capslock = INT_MIN;

	return input;
}

void merge_input_config(struct input_config *dst, struct input_config *src) {
	if (src->accel_profile != INT_MIN) {
		dst->accel_profile = src->accel_profile;
	}
	if (src->click_method != INT_MIN) {
		dst->click_method = src->click_method;
	}
	if (src->drag != INT_MIN) {
		dst->drag = src->drag;
	}
	if (src->drag_lock != INT_MIN) {
		dst->drag_lock = src->drag_lock;
	}
	if (src->dwt != INT_MIN) {
		dst->dwt = src->dwt;
	}
	if (src->left_handed != INT_MIN) {
		dst->left_handed = src->left_handed;
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
	if (src->scroll_factor != FLT_MIN) {
		dst->scroll_factor = src->scroll_factor;
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
	if (src->scroll_button != INT_MIN) {
		dst->scroll_button = src->scroll_button;
	}
	if (src->send_events != INT_MIN) {
		dst->send_events = src->send_events;
	}
	if (src->tap != INT_MIN) {
		dst->tap = src->tap;
	}
	if (src->tap_button_map != INT_MIN) {
		dst->tap_button_map = src->tap_button_map;
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
	if (src->xkb_numlock != INT_MIN) {
		dst->xkb_numlock = src->xkb_numlock;
	}
	if (src->xkb_capslock != INT_MIN) {
		dst->xkb_capslock = src->xkb_capslock;
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

static void merge_wildcard_on_all(struct input_config *wildcard) {
	for (int i = 0; i < config->input_configs->length; i++) {
		struct input_config *ic = config->input_configs->items[i];
		if (strcmp(wildcard->identifier, ic->identifier) != 0) {
			wlr_log(WLR_DEBUG, "Merging input * config on %s", ic->identifier);
			merge_input_config(ic, wildcard);
		}
	}
}

struct input_config *store_input_config(struct input_config *ic) {
	bool wildcard = strcmp(ic->identifier, "*") == 0;
	if (wildcard) {
		merge_wildcard_on_all(ic);
	}

	int i = list_seq_find(config->input_configs, input_identifier_cmp,
			ic->identifier);
	if (i >= 0) {
		wlr_log(WLR_DEBUG, "Merging on top of existing input config");
		struct input_config *current = config->input_configs->items[i];
		merge_input_config(current, ic);
		free_input_config(ic);
		ic = current;
	} else if (!wildcard) {
		wlr_log(WLR_DEBUG, "Adding non-wildcard input config");
		i = list_seq_find(config->input_configs, input_identifier_cmp, "*");
		if (i >= 0) {
			wlr_log(WLR_DEBUG, "Merging on top of input * config");
			struct input_config *current = new_input_config(ic->identifier);
			merge_input_config(current, config->input_configs->items[i]);
			merge_input_config(current, ic);
			free_input_config(ic);
			ic = current;
		}
		list_add(config->input_configs, ic);
	} else {
		// New wildcard config. Just add it
		wlr_log(WLR_DEBUG, "Adding input * config");
		list_add(config->input_configs, ic);
	}

	wlr_log(WLR_DEBUG, "Config stored for input %s", ic->identifier);

	return ic;
}

void free_input_config(struct input_config *ic) {
	if (!ic) {
		return;
	}
	free(ic->identifier);
	free(ic->xkb_layout);
	free(ic->xkb_model);
	free(ic->xkb_options);
	free(ic->xkb_rules);
	free(ic->xkb_variant);
	free(ic->mapped_from_region);
	free(ic->mapped_to_output);
	free(ic);
}

int input_identifier_cmp(const void *item, const void *data) {
	const struct input_config *ic = item;
	const char *identifier = data;
	return strcmp(ic->identifier, identifier);
}
