#define _POSIX_C_SOURCE 200809L
#include "gesture.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"

const uint8_t GESTURE_FINGERS_ANY = 0;

// Helper to easily allocate and format string
static char *strformat(const char *format, ...) {
	va_list args;
	va_start(args, format);
	int length = vsnprintf(NULL, 0, format, args) + 1;
	va_end(args);

	char *result = malloc(length);
	if (result) {
		va_start(args, format);
		vsnprintf(result, length, format, args);
		va_end(args);
	}

	return result;
}

char *gesture_parse(const char *input, struct gesture *output) {
	// Clear output in case of failure
	output->type = GESTURE_TYPE_NONE;
	output->fingers = GESTURE_FINGERS_ANY;
	output->directions = GESTURE_DIRECTION_NONE;

	// Split input type, fingers and directions
	list_t *split = split_string(input, ":");
	if (split->length < 1 || split->length > 3) {
		return strformat(
				"expected <gesture>[:<fingers>][:direction], got %s",
				input);
	}

	// Parse gesture type
	if (strcmp(split->items[0], "hold") == 0) {
		output->type = GESTURE_TYPE_HOLD;
	} else if (strcmp(split->items[0], "pinch") == 0) {
		output->type = GESTURE_TYPE_PINCH;
	} else if (strcmp(split->items[0], "swipe") == 0) {
		output->type = GESTURE_TYPE_SWIPE;
	} else {
		return strformat("expected hold|pinch|swipe, got %s",
				split->items[0]);
	}

	// Parse optional arguments
	if (split->length > 1) {
		char *next = split->items[1];

		// Try to parse as finger count (1-9)
		if (strlen(next) == 1 && '1' <= next[0] && next[0] <= '9') {
			output->fingers = atoi(next);

			// Move to next if available
			next = split->length == 3 ? split->items[2] : NULL;
		} else if (split->length == 3) {
			// Fail here if argument can only be finger count
			return strformat("expected 1-9, got %s", next);
		}

		// If there is an argument left, try to parse as direction
		if (next) {
			list_t *directions = split_string(next, "+");

			for (int i = 0; i < directions->length; ++i) {
				const char *item = directions->items[i];
				if (strcmp(item, "any") == 0) {
					continue;
				} else if (strcmp(item, "up") == 0) {
					output->directions |= GESTURE_DIRECTION_UP;
				} else if (strcmp(item, "down") == 0) {
					output->directions |= GESTURE_DIRECTION_DOWN;
				} else if (strcmp(item, "left") == 0) {
					output->directions |= GESTURE_DIRECTION_LEFT;
				} else if (strcmp(item, "right") == 0) {
					output->directions |= GESTURE_DIRECTION_RIGHT;
				} else if (strcmp(item, "inward") == 0) {
					output->directions |= GESTURE_DIRECTION_INWARD;
				} else if (strcmp(item, "outward") == 0) {
					output->directions |= GESTURE_DIRECTION_OUTWARD;
				} else if (strcmp(item, "clockwise") == 0) {
					output->directions |= GESTURE_DIRECTION_CLOCKWISE;
				} else if (strcmp(item, "counterclockwise") == 0) {
					output->directions |= GESTURE_DIRECTION_COUNTERCLOCKWISE;
				} else {
					return strformat("expected direction, got %s", item);
				}
			}
			list_free_items_and_destroy(directions);
		}
	} // if optional args

	list_free_items_and_destroy(split);

	return NULL;
}

const char *gesture_type_string(enum gesture_type type) {
	switch (type) {
	case GESTURE_TYPE_NONE:
		return "none";
	case GESTURE_TYPE_HOLD:
		return "hold";
	case GESTURE_TYPE_PINCH:
		return "pinch";
	case GESTURE_TYPE_SWIPE:
		return "swipe";
	}

	return NULL;
}

const char *gesture_direction_string(enum gesture_direction direction) {
	switch (direction) {
	case GESTURE_DIRECTION_NONE:
		return "none";
	case GESTURE_DIRECTION_UP:
		return "up";
	case GESTURE_DIRECTION_DOWN:
		return "down";
	case GESTURE_DIRECTION_LEFT:
		return "left";
	case GESTURE_DIRECTION_RIGHT:
		return "right";
	case GESTURE_DIRECTION_INWARD:
		return "inward";
	case GESTURE_DIRECTION_OUTWARD:
		return "outward";
	case GESTURE_DIRECTION_CLOCKWISE:
		return "clockwise";
	case GESTURE_DIRECTION_COUNTERCLOCKWISE:
		return "counterclockwise";
	}

	return NULL;
}

// Helper to turn combination of directions flags into string representation.
static char *gesture_directions_to_string(uint32_t directions) {
	char *result = NULL;

	for (uint8_t bit = 0; bit < 32; bit++) {
		uint32_t masked = directions & (1 << bit);
		if (masked) {
			const char *name = gesture_direction_string(masked);

			if (!name) {
				name = "unknown";
			}

			if (!result) {
				result = strdup(name);
			} else {
				char *new = strformat("%s+%s", result, name);
				free(result);
				result = new;
			}
		}
	}

	if(!result) {
		return strdup("any");
	}

	return result;
}

char *gesture_to_string(struct gesture *gesture) {
	char *directions = gesture_directions_to_string(gesture->directions);
	char *result = strformat("%s:%u:%s",
		gesture_type_string(gesture->type),
		gesture->fingers, directions);
	free(directions);
	return result;
}

bool gesture_check(struct gesture *target, enum gesture_type type, uint8_t fingers) {
	// Check that gesture type matches
	if (target->type != type) {
		return false;
	}

	// Check that finger count matches
	if (target->fingers != GESTURE_FINGERS_ANY && target->fingers != fingers) {
		return false;
	}

	return true;
}

bool gesture_match(struct gesture *target, struct gesture *to_match, bool exact) {
	// Check type and fingers
	if (!gesture_check(target, to_match->type, to_match->fingers)) {
		return false;
	}

	// Enforce exact matches ...
	if (exact && target->directions != to_match->directions) {
		return false;
	}

	// ... or ensure all target directions are matched
	return (target->directions & to_match->directions) == target->directions;
}

bool gesture_equal(struct gesture *a, struct gesture *b) {
	return a->type == b->type
		&& a->fingers == b->fingers
		&& a->directions == b->directions;
}

// Return count of set bits in directions bit field.
static uint8_t gesture_directions_count(uint32_t directions) {
	uint8_t count = 0;
	for (; directions; directions >>= 1) {
		count += directions & 1;
	}
	return count;
}

// Compare direction bit count of two direction.
static int8_t gesture_directions_compare(uint32_t a, uint32_t b) {
	return gesture_directions_count(a) - gesture_directions_count(b);
}

// Compare two direction based on their direction bit count
int8_t gesture_compare(struct gesture *a, struct gesture *b) {
	return gesture_directions_compare(a->directions, b->directions);
}

void gesture_tracker_begin(struct gesture_tracker *tracker,
		enum gesture_type type, uint8_t fingers) {
	tracker->type = type;
	tracker->fingers = fingers;

	tracker->dx = 0.0;
	tracker->dy = 0.0;
	tracker->scale = 1.0;
	tracker->rotation = 0.0;

	sway_log(SWAY_DEBUG, "begin tracking %s:%u:? gesture",
		gesture_type_string(type), fingers);
}

bool gesture_tracker_check(struct gesture_tracker *tracker, enum gesture_type type) {
	return tracker->type == type;
}

void gesture_tracker_update(struct gesture_tracker *tracker,
		double dx, double dy, double scale, double rotation) {
	if (tracker->type == GESTURE_TYPE_HOLD) {
		sway_assert(false, "hold does not update.");
		return;
	}

	tracker->dx += dx;
	tracker->dy += dy;

	if (tracker->type == GESTURE_TYPE_PINCH) {
		tracker->scale = scale;
		tracker->rotation += rotation;
	}

	sway_log(SWAY_DEBUG, "update tracking %s:%u:? gesture: %f %f %f %f",
			gesture_type_string(tracker->type),
			tracker->fingers,
			tracker->dx, tracker->dy,
			tracker->scale, tracker->rotation);
}

void gesture_tracker_cancel(struct gesture_tracker *tracker) {
	sway_log(SWAY_DEBUG, "cancel tracking %s:%u:? gesture",
			gesture_type_string(tracker->type), tracker->fingers);

	tracker->type = GESTURE_TYPE_NONE;
}

struct gesture *gesture_tracker_end(struct gesture_tracker *tracker) {
	struct gesture *result = calloc(1, sizeof(struct gesture));

	// Ignore gesture under some threshold
	// TODO: Make configurable
	const double min_rotation = 5;
	const double min_scale_delta = 0.1;

	// Determine direction
	switch(tracker->type) {
	// Gestures with scale and rotation
	case GESTURE_TYPE_PINCH:
		if (tracker->rotation > min_rotation) {
			result->directions |= GESTURE_DIRECTION_CLOCKWISE;
		}
		if (tracker->rotation < -min_rotation) {
			result->directions |= GESTURE_DIRECTION_COUNTERCLOCKWISE;
		}

		if (tracker->scale > (1.0 + min_scale_delta)) {
			result->directions |= GESTURE_DIRECTION_OUTWARD;
		}
		if (tracker->scale < (1.0 - min_scale_delta)) {
			result->directions |= GESTURE_DIRECTION_INWARD;
		}
		__attribute__ ((fallthrough));
	// Gestures with dx and dy
	case GESTURE_TYPE_SWIPE:
		if (fabs(tracker->dx) > fabs(tracker->dy)) {
			if (tracker->dx > 0) {
				result->directions |= GESTURE_DIRECTION_RIGHT;
			} else {
				result->directions |= GESTURE_DIRECTION_LEFT;
			}
		} else {
			if (tracker->dy > 0) {
				result->directions |= GESTURE_DIRECTION_DOWN;
			} else {
				result->directions |= GESTURE_DIRECTION_UP;
			}
		}
	// Gesture without any direction
	case GESTURE_TYPE_HOLD:
		break;
	// Not tracking any gesture
	case GESTURE_TYPE_NONE:
		sway_assert(false, "Not tracking any gesture.");
		return result;
	}

	result->type = tracker->type;
	result->fingers = tracker->fingers;

	char *description = gesture_to_string(result);
	sway_log(SWAY_DEBUG, "end tracking gesture: %s", description);
	free(description);

	tracker->type = GESTURE_TYPE_NONE;

	return result;
}
