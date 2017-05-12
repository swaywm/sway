#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include "sway/output.h"
#include "log.h"
#include "list.h"

void output_get_scaled_size(wlc_handle handle, struct wlc_size *size) {
	*size = *wlc_output_get_resolution(handle);
	uint32_t scale = wlc_output_get_scale(handle);
	size->w /= scale;
	size->h /= scale;
}

swayc_t *output_by_name(const char* name, const struct wlc_point *abs_pos) {
	swayc_t *output = NULL;
	// If there is no output directly next to the current one, use
	// swayc_opposite_output to wrap.
	if (strcasecmp(name, "left") == 0) {
		output = swayc_adjacent_output(NULL, MOVE_LEFT, abs_pos, true);
		if (!output) {
			output = swayc_opposite_output(MOVE_RIGHT, abs_pos);
		}
	} else if (strcasecmp(name, "right") == 0) {
		output = swayc_adjacent_output(NULL, MOVE_RIGHT, abs_pos, true);
		if (!output) {
			output = swayc_opposite_output(MOVE_LEFT, abs_pos);
		}
	} else if (strcasecmp(name, "up") == 0) {
		output = swayc_adjacent_output(NULL, MOVE_UP, abs_pos, true);
		if (!output) {
			output = swayc_opposite_output(MOVE_DOWN, abs_pos);
		}
	} else if (strcasecmp(name, "down") == 0) {
		output = swayc_adjacent_output(NULL, MOVE_DOWN, abs_pos, true);
		if (!output) {
			output = swayc_opposite_output(MOVE_UP, abs_pos);
		}
	} else {
		for (size_t i = 0; i < root_container.children->length; ++i) {
			swayc_t *c = *(swayc_t **)list_get(root_container.children, i);
			if (c->type == C_OUTPUT && strcasecmp(c->name, name) == 0) {
				return c;
			}
		}
	}
	return output;
}

swayc_t *swayc_opposite_output(enum movement_direction dir,
		const struct wlc_point *abs_pos) {

	// Search through all the outputs and pick the output whose edge covers the
	// given position, and is at leftmost/rightmost/upmost/downmost side of the
	// screen (decided by the direction given).
	swayc_t *opposite = NULL;
	char *dir_text = NULL;
	switch(dir) {
		case MOVE_LEFT:
		case MOVE_RIGHT: ;
			for (size_t i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = *(swayc_t **)list_get(root_container.children, i);
				if (abs_pos->y >= c->y && abs_pos->y <= c->y + c->height) {
					if (!opposite) {
						opposite = c;
					} else if ((dir == MOVE_LEFT && c->x < opposite->x)
							|| (dir == MOVE_RIGHT && c->x > opposite->x)) {
						opposite = c;
					}
				}
			}
			dir_text = dir == MOVE_LEFT ? "leftmost" : "rightmost";
			break;
		case MOVE_UP:
		case MOVE_DOWN: ;
			for (size_t i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = *(swayc_t **)list_get(root_container.children, i);
				if (abs_pos->x >= c->x && abs_pos->x <= c->x + c->width) {
					if (!opposite) {
						opposite = c;
					} else if ((dir == MOVE_UP && c->y < opposite->y)
							|| (dir == MOVE_DOWN && c->y > opposite->y)) {
						opposite = c;
					}
				}
			}
			dir_text = dir == MOVE_UP ? "upmost" : "downmost";
			break;
		default:
			sway_abort("Function called with invalid argument.");
			break;
	}
	if (opposite) {
		sway_log(L_DEBUG, "%s (%.0fx%.0f+%.0f+%.0f) is %s from y-position %i",
				opposite->name, opposite->width, opposite->height, opposite->x, opposite->y,
				dir_text, abs_pos->y);
	}
	return opposite;
}

// Position is where on the edge (as absolute position) the adjacent output should be searched for.
swayc_t *swayc_adjacent_output(swayc_t *output, enum movement_direction dir,
		const struct wlc_point *abs_pos, bool pick_closest) {

	if (!output) {
		output = swayc_active_output();
	}
	// In order to find adjacent outputs we need to test that the outputs are
	// aligned on one axis (decided by the direction given) and that the given
	// position is within the edge of the adjacent output. If no such output
	// exists we pick the adjacent output within the edge that is closest to
	// the given position, if any.
	swayc_t *adjacent = NULL;
	char *dir_text = NULL;
	switch(dir) {
		case MOVE_LEFT:
		case MOVE_RIGHT: ;
			double delta_y = 0;
			for (size_t i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = *(swayc_t **)list_get(root_container.children, i);
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				bool x_aligned = dir == MOVE_LEFT ?
					c->x + c->width == output->x :
					c->x == output->x + output->width;
				if (!x_aligned) {
					continue;
				}
				if (abs_pos->y >= c->y && abs_pos->y <= c->y + c->height) {
					delta_y = 0;
					adjacent = c;
					break;
				} else if (pick_closest) {
					// track closest adjacent output
					double top_y = c->y, bottom_y = c->y + c->height;
					if (top_y >= output->y && top_y <= output->y + output->height) {
						double delta = top_y - abs_pos->y;
						if (delta < 0) delta = -delta;
						if (delta < delta_y || !adjacent) {
							delta_y = delta;
							adjacent = c;
						}
					}
					// we check both points and pick the closest
					if (bottom_y >= output->y && bottom_y <= output->y + output->height) {
						double delta = bottom_y - abs_pos->y;
						if (delta < 0) delta = -delta;
						if (delta < delta_y || !adjacent) {
							delta_y = delta;
							adjacent = c;
						}
					}
				}
			}
			dir_text = dir == MOVE_LEFT ? "left of" : "right of";
			if (adjacent && delta_y == 0) {
				sway_log(L_DEBUG, "%s (%.0fx%.0f+%.0f+%.0f) is %s current output %s (y-position %i)",
						adjacent->name, adjacent->width, adjacent->height, adjacent->x, adjacent->y,
						dir_text, output->name, abs_pos->y);
			} else if (adjacent) {
				// so we end up picking the closest adjacent output because
				// there is no directly adjacent to the given position
				sway_log(L_DEBUG, "%s (%.0fx%.0f+%.0f+%.0f) is %s current output %s (y-position %i, delta: %.0f)",
					adjacent->name, adjacent->width, adjacent->height, adjacent->x, adjacent->y,
					dir_text, output->name, abs_pos->y, delta_y);
			}
			break;
		case MOVE_UP:
		case MOVE_DOWN: ;
			double delta_x = 0;
			for (size_t i = 0; i < root_container.children->length; ++i) {
				swayc_t *c = *(swayc_t **)list_get(root_container.children, i);
				if (c == output || c->type != C_OUTPUT) {
					continue;
				}
				bool y_aligned = dir == MOVE_UP ?
					c->y + c->height == output->y :
					c->y == output->y + output->height;
				if (!y_aligned) {
					continue;
				}
				if (abs_pos->x >= c->x && abs_pos->x <= c->x + c->width) {
					delta_x = 0;
					adjacent = c;
					break;
				} else if (pick_closest) {
					// track closest adjacent output
					double left_x = c->x, right_x = c->x + c->width;
					if (left_x >= output->x && left_x <= output->x + output->width) {
						double delta = left_x - abs_pos->x;
						if (delta < 0) delta = -delta;
						if (delta < delta_x || !adjacent) {
							delta_x = delta;
							adjacent = c;
						}
					}
					// we check both points and pick the closest
					if (right_x >= output->x && right_x <= output->x + output->width) {
						double delta = right_x - abs_pos->x;
						if (delta < 0) delta = -delta;
						if (delta < delta_x || !adjacent) {
							delta_x = delta;
							adjacent = c;
						}
					}
				}
			}
			dir_text = dir == MOVE_UP ? "above" : "below";
			if (adjacent && delta_x == 0) {
				sway_log(L_DEBUG, "%s (%.0fx%.0f+%.0f+%.0f) is %s current output %s (x-position %i)",
						adjacent->name, adjacent->width, adjacent->height, adjacent->x, adjacent->y,
						dir_text, output->name, abs_pos->x);
			} else if (adjacent) {
				// so we end up picking the closest adjacent output because
				// there is no directly adjacent to the given position
				sway_log(L_DEBUG, "%s (%.0fx%.0f+%.0f+%.0f) is %s current output %s (x-position %i, delta: %.0f)",
					adjacent->name, adjacent->width, adjacent->height, adjacent->x, adjacent->y,
					dir_text, output->name, abs_pos->x, delta_x);
			}
			break;
		default:
			sway_abort("Function called with invalid argument.");
			break;
	}
	return adjacent;
}

void get_absolute_position(swayc_t *container, struct wlc_point *point) {
	if (!container || !point)
		sway_abort("Need container and wlc_point (was %p, %p).", container, point);

	if (container->type == C_OUTPUT) {
		// Coordinates are already absolute.
		point->x = container->x;
		point->y = container->y;
	} else {
		swayc_t *output = swayc_parent_by_type(container, C_OUTPUT);
		if (container->type == C_WORKSPACE) {
			// Workspace coordinates are actually wrong/arbitrary, but should
			// be same as output.
			point->x = output->x;
			point->y = output->y;
		} else {
			point->x = output->x + container->x;
			point->y = output->y + container->y;
		}
	}
}

void get_absolute_center_position(swayc_t *container, struct wlc_point *point) {
	get_absolute_position(container, point);
	point->x += container->width/2;
	point->y += container->height/2;
}

static int sort_workspace_cmp_qsort(const void *_a, const void *_b) {
	swayc_t *a = *(void **)_a;
	swayc_t *b = *(void **)_b;
	int retval = 0;

	if (isdigit(a->name[0]) && isdigit(b->name[0])) {
		int a_num = strtol(a->name, NULL, 10);
		int b_num = strtol(b->name, NULL, 10);
		retval = (a_num < b_num) ? -1 : (a_num > b_num);
	} else if (isdigit(a->name[0])) {
		retval = -1;
	} else if (isdigit(b->name[0])) {
		retval = 1;
	}

	return retval;
}

void sort_workspaces(swayc_t *output) {
	list_isort(output->children, sort_workspace_cmp_qsort);
}
