#include <stdlib.h>
#include <stdbool.h>
#include "list.h"
#include "log.h"
#include "layout.h"
#include "movement.h"

int move_focus(enum movement_direction direction) {
	swayc_t *current = get_focused_container(&root_container);
	swayc_t *parent = current->parent;

	if(direction == MOVE_PARENT) {
		current = parent;
		parent  = parent->parent;
		if(parent->type == C_ROOT) {
			sway_log(L_DEBUG, "Focus cannot move to parent");
			return 1;
		} else {
			sway_log(L_DEBUG, "Moving focus away from %p", current);
			unfocus_all(parent);
			focus_view (parent);
			return 0;
		}
	}

	while (true) {
		sway_log(L_DEBUG, "Moving focus away from %p", current);

		// Test if we can even make a difference here
		bool can_move = false;
		int diff = 0;
		if (direction == MOVE_LEFT || direction == MOVE_RIGHT) {
			if (parent->layout == L_HORIZ) {
				can_move = true;
				diff = direction == MOVE_LEFT ? -1 : 1;
			}
		} else {
			if (parent->layout == L_VERT) {
				can_move = true;
				diff = direction == MOVE_UP ? -1 : 1;
			}
		}
		sway_log(L_DEBUG, "Can move? %s", can_move ? "yes" : "no");
		if (can_move) {
			int i;
			for (i = 0; i < parent->children->length; ++i) {
				swayc_t *child = parent->children->items[i];
				if (child == current) {
					break;
				}
			}
			int desired = i + diff;
			sway_log(L_DEBUG, "Moving from %d to %d", i, desired);
			if (desired < 0 || desired >= parent->children->length) {
				can_move = false;
			} else {
				unfocus_all(&root_container);
				focus_view(parent->children->items[desired]);
				return 0;
			}
		}
		if (!can_move) {
			sway_log(L_DEBUG, "Can't move at current level, moving up tree");
			current = parent;
			parent = parent->parent;
			if (parent->type == C_ROOT) {
				// Nothing we can do
				return 1;
			}
		}
	}
}
