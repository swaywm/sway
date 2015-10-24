#include <strings.h>
#include "output.h"
#include "log.h"

swayc_t *output_by_name(const char* name) {
	if (strcasecmp(name, "left") == 0) {
		return swayc_adjacent_output(NULL, MOVE_LEFT);
	}
	else if (strcasecmp(name, "right") == 0) {
		return swayc_adjacent_output(NULL, MOVE_RIGHT);
	}
	else if (strcasecmp(name, "up") == 0) {
		return swayc_adjacent_output(NULL, MOVE_UP);
	}
	else if (strcasecmp(name, "down") == 0) {
		return swayc_adjacent_output(NULL, MOVE_DOWN);
	}
	else {
		for(int i = 0; i < root_container.children->length; ++i) {
			swayc_t *c = root_container.children->items[i];
			if (c->type == C_OUTPUT && strcasecmp(c->name, name) == 0) {
				return c;
			}
		}
	}
	return NULL;
}

swayc_t *swayc_adjacent_output(swayc_t *output, enum movement_direction dir) {
	// TODO: This implementation is naïve: We assume all outputs are
	// perfectly aligned (ie. only a single output per edge which covers
	// the whole edge).
	if (!output) {
		output = swayc_active_output();
	}
	swayc_t *adjacent = NULL;
	switch(dir) {
		case MOVE_LEFT:
			for(int i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = root_container.children->items[i];
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				if (c->y == output->y && c->x + c->width == output->x) {
					sway_log(L_DEBUG, "%s is left of current output %s", c->name, output->name);
					adjacent = c;
					break;
				}
			}
			break;
		case MOVE_RIGHT:
			for(int i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = root_container.children->items[i];
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				if (c->y == output->y && output->x + output->width == c->x) {
					sway_log(L_DEBUG, "%s is right of current output %s", c->name, output->name);
					adjacent = c;
					break;
				}
			}
			break;
		case MOVE_UP:
			for(int i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = root_container.children->items[i];
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				if (output->x == c->x && c->y + c->height == output->y) {
					sway_log(L_DEBUG, "%s is above current output %s", c->name, output->name);
					adjacent = c;
					break;
				}
			}
			break;
		case MOVE_DOWN:
			for(int i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = root_container.children->items[i];
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				if (output->x == c->x && output->y + output->height == c->y) {
					sway_log(L_DEBUG, "%s is below current output %s", c->name, output->name);
					adjacent = c;
					break;
				}
			}
			break;
		default:
			sway_abort("Function called with invalid argument.");
			break;
	}
	return adjacent;
}
