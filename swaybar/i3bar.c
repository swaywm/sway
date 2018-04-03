#define _POSIX_C_SOURCE 200809L
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "config.h"
#include "swaybar/config.h"
#include "swaybar/status_line.h"

static void i3bar_block_free(struct i3bar_block *block) {
	if (!block) {
		return;
	}
	wl_list_remove(&block->link);
	free(block->full_text);
	free(block->short_text);
	free(block->align);
	free(block->name);
	free(block->instance);
	free(block->color);
}

static bool i3bar_parse_json(struct status_line *status, const char *text) {
	struct i3bar_block *block, *tmp;
	wl_list_for_each_safe(block, tmp, &status->blocks, link) {
		i3bar_block_free(block);
	}
	json_object *results = json_tokener_parse(text);
	if (!results) {
		status_error(status, "[failed to parse i3bar json]");
		return false;
	}
	wlr_log(L_DEBUG, "Got i3bar json: '%s'", text);
	for (size_t i = 0; i < json_object_array_length(results); ++i) {
		json_object *full_text, *short_text, *color, *min_width, *align, *urgent;
		json_object *name, *instance, *separator, *separator_block_width;
		json_object *background, *border, *border_top, *border_bottom;
		json_object *border_left, *border_right, *markup;
		json_object *json = json_object_array_get_idx(results, i);
		if (!json) {
			continue;
		}
		json_object_object_get_ex(json, "full_text", &full_text);
		json_object_object_get_ex(json, "short_text", &short_text);
		json_object_object_get_ex(json, "color", &color);
		json_object_object_get_ex(json, "min_width", &min_width);
		json_object_object_get_ex(json, "align", &align);
		json_object_object_get_ex(json, "urgent", &urgent);
		json_object_object_get_ex(json, "name", &name);
		json_object_object_get_ex(json, "instance", &instance);
		json_object_object_get_ex(json, "markup", &markup);
		json_object_object_get_ex(json, "separator", &separator);
		json_object_object_get_ex(json, "separator_block_width", &separator_block_width);
		json_object_object_get_ex(json, "background", &background);
		json_object_object_get_ex(json, "border", &border);
		json_object_object_get_ex(json, "border_top", &border_top);
		json_object_object_get_ex(json, "border_bottom", &border_bottom);
		json_object_object_get_ex(json, "border_left", &border_left);
		json_object_object_get_ex(json, "border_right", &border_right);

		struct i3bar_block *block = calloc(1, sizeof(struct i3bar_block));
		block->full_text = full_text ?
			strdup(json_object_get_string(full_text)) : NULL;
		block->short_text = short_text ?
			strdup(json_object_get_string(short_text)) : NULL;
		if (color) {
			block->color = malloc(sizeof(uint32_t));
			*block->color = parse_color(json_object_get_string(color));
		}
		if (min_width) {
			json_type type = json_object_get_type(min_width);
			if (type == json_type_int) {
				block->min_width = json_object_get_int(min_width);
			} else if (type == json_type_string) {
				/* the width will be calculated when rendering */
				block->min_width = 0;
			}
		}
		block->align = strdup(align ? json_object_get_string(align) : "left");
		block->urgent = urgent ? json_object_get_int(urgent) : false;
		block->name = name ? strdup(json_object_get_string(name)) : NULL;
		block->instance = instance ?
			strdup(json_object_get_string(instance)) : NULL;
		if (markup) {
			block->markup = false;
			const char *markup_str = json_object_get_string(markup);
			if (strcmp(markup_str, "pango") == 0) {
				block->markup = true;
			}
		}
		block->separator = separator ? json_object_get_int(separator) : true;
		block->separator_block_width = separator_block_width ?
			json_object_get_int(separator_block_width) : 9;
		// Airblader features
		block->background = background ?
			parse_color(json_object_get_string(background)) : 0;
		block->border = border ? 
			parse_color(json_object_get_string(border)) : 0;
		block->border_top = border_top ? json_object_get_int(border_top) : 1;
		block->border_bottom = border_bottom ?
			json_object_get_int(border_bottom) : 1;
		block->border_left = border_left ? json_object_get_int(border_left) : 1;
		block->border_right = border_right ?
			json_object_get_int(border_right) : 1;
		wl_list_insert(&status->blocks, &block->link);
	}
	return true;
}

bool i3bar_handle_readable(struct status_line *status) {
	struct i3bar_protocol_state *state = &status->i3bar_state;

	char *cur = &state->buffer[state->buffer_index];
	ssize_t n = read(status->read_fd, cur,
			state->buffer_size - state->buffer_index);
	if (n == 0) {
		return 0;
	}

	if (n == (ssize_t)(state->buffer_size - state->buffer_index)) {
		state->buffer_size = state->buffer_size * 2;
		char *new_buffer = realloc(state->buffer, state->buffer_size);
		if (!new_buffer) {
			free(state->buffer);
			status_error(status, "[failed to allocate buffer]");
			return -1;
		}
		state->buffer = new_buffer;
	}

	bool redraw = false;
	while (*cur) {
		if (state->nodes[state->depth] == JSON_NODE_STRING) {
			if (!state->escape && *cur == '"') {
				--state->depth;
			}
			state->escape = !state->escape && *cur == '\\';
		} else {
			switch (*cur) {
			case '[':
				++state->depth;
				if (state->depth >
						sizeof(state->nodes) / sizeof(state->nodes[0])) {
					status_error(status, "[i3bar json too deep]");
					return false;
				}
				state->nodes[state->depth] = JSON_NODE_ARRAY;
				if (state->depth == 1) {
					state->current_node = cur;
				}
				break;
			case ']':
				if (state->nodes[state->depth] != JSON_NODE_ARRAY) {
					status_error(status, "[failed to parse i3bar json]");
					return false;
				}
				--state->depth;
				if (state->depth == 0) {
					// cur[1] is valid since cur[0] != '\0'
					char p = cur[1];
					cur[1] = '\0';
					redraw = i3bar_parse_json(
							status, state->current_node) || redraw;
					cur[1] = p;
					memmove(state->buffer, cur,
							state->buffer_size - (cur - state->buffer));
					cur = state->buffer;
					state->current_node = cur + 1;
				}
				break;
			case '"':
				++state->depth;
				if (state->depth >
						sizeof(state->nodes) / sizeof(state->nodes[0])) {
					status_error(status, "[i3bar json too deep]");
					return false;
				}
				state->nodes[state->depth] = JSON_NODE_STRING;
				break;
			}
		}
		++cur;
	}
	state->buffer_index = cur - state->buffer;
	return redraw;
}

void i3bar_block_send_click(struct status_line *status,
		struct i3bar_block *block, int x, int y, uint32_t button) {
	wlr_log(L_DEBUG, "block %s clicked", block->name ? block->name : "(nil)");
	if (!block->name || !status->i3bar_state.click_events) {
		return;
	}

	struct json_object *event_json = json_object_new_object();
	json_object_object_add(event_json, "name",
			json_object_new_string(block->name));
	if (block->instance) {
		json_object_object_add(event_json, "instance",
				json_object_new_string(block->instance));
	}

	json_object_object_add(event_json, "button", json_object_new_int(button));
	json_object_object_add(event_json, "x", json_object_new_int(x));
	json_object_object_add(event_json, "y", json_object_new_int(y));
	dprintf(status->write_fd, "%s\n", json_object_to_json_string(event_json));
	json_object_put(event_json);
}
