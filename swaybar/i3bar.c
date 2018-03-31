#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "swaybar/config.h"
#include "swaybar/status_line.h"

static void i3bar_parse_json(struct status_line *status, const char *text) {
	wlr_log(L_DEBUG, "got json: %s", text);
}

int i3bar_readable(struct status_line *status) {
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

	int handled = 0;
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
					return -1;
				}
				state->nodes[state->depth] = JSON_NODE_ARRAY;
				if (state->depth == 1) {
					state->current_node = cur;
				}
				break;
			case ']':
				if (state->nodes[state->depth] != JSON_NODE_ARRAY) {
					status_error(status, "[failed to parse i3bar json]");
					return -1;
				}
				--state->depth;
				if (state->depth == 0) {
					// cur[1] is valid since cur[0] != '\0'
					char p = cur[1];
					cur[1] = '\0';
					i3bar_parse_json(status, state->current_node);
					cur[1] = p;
					memmove(state->buffer, cur,
							state->buffer_size - (cur - state->buffer));
					++handled;
					cur = state->buffer;
					state->current_node = cur + 1;
				}
				break;
			case '"':
				++state->depth;
				if (state->depth >
						sizeof(state->nodes) / sizeof(state->nodes[0])) {
					status_error(status, "[i3bar json too deep]");
					return -1;
				}
				state->nodes[state->depth] = JSON_NODE_STRING;
				break;
			}
		}
		++cur;
	}
	state->buffer_index = cur - state->buffer;
	return handled;
}
