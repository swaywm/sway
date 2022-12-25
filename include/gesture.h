#ifndef _SWAY_GESTURE_H
#define _SWAY_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * A gesture type used in binding.
 */
enum gesture_type {
	GESTURE_TYPE_NONE = 0,
	GESTURE_TYPE_HOLD,
	GESTURE_TYPE_PINCH,
	GESTURE_TYPE_SWIPE,
};

// Turns single type enum value to constant string representation.
const char *gesture_type_string(enum gesture_type direction);

// Value to use to accept any finger count
extern const uint8_t GESTURE_FINGERS_ANY;

/**
 * A gesture direction used in binding.
 */
enum gesture_direction {
	GESTURE_DIRECTION_NONE = 0,
	// Directions based on delta x and y
	GESTURE_DIRECTION_UP = 1 << 0,
	GESTURE_DIRECTION_DOWN = 1 << 1,
	GESTURE_DIRECTION_LEFT = 1 << 2,
	GESTURE_DIRECTION_RIGHT = 1 << 3,
	// Directions based on scale
	GESTURE_DIRECTION_INWARD = 1 << 4,
	GESTURE_DIRECTION_OUTWARD = 1 << 5,
	// Directions based on rotation
	GESTURE_DIRECTION_CLOCKWISE = 1 << 6,
	GESTURE_DIRECTION_COUNTERCLOCKWISE = 1 << 7,
};

// Turns single direction enum value to constant string representation.
const char *gesture_direction_string(enum gesture_direction direction);

/**
 * Struct representing a pointer gesture
 */
struct gesture {
	enum gesture_type type;
	uint8_t fingers;
	uint32_t directions;
};

/**
 * Parses gesture from <gesture>[:<fingers>][:<directions>] string.
 *
 * Return NULL on success, otherwise error message string
 */
char *gesture_parse(const char *input, struct gesture *output);

// Turns gesture into string representation
char *gesture_to_string(struct gesture *gesture);

// Check if gesture is of certain type and finger count.
bool gesture_check(struct gesture *target,
		enum gesture_type type, uint8_t fingers);

// Check if a gesture target/binding is match by other gesture/input
bool gesture_match(struct gesture *target,
		struct gesture *to_match, bool exact);

// Returns true if gesture are exactly the same
bool gesture_equal(struct gesture *a, struct gesture *b);

// Compare distance between two matched target gestures.
int8_t gesture_compare(struct gesture *a, struct gesture *b);

// Small helper struct to track gestures over time
struct gesture_tracker {
	enum gesture_type type;
	uint8_t fingers;
	double dx, dy;
	double scale;
	double rotation;
};

// Begin gesture tracking
void gesture_tracker_begin(struct gesture_tracker *tracker,
		enum gesture_type type, uint8_t fingers);

// Check if the provides type is currently being tracked
bool gesture_tracker_check(struct gesture_tracker *tracker,
		enum gesture_type type);

// Update gesture track with new data point
void gesture_tracker_update(struct gesture_tracker *tracker, double dx,
		double dy, double scale, double rotation);

// Reset tracker
void gesture_tracker_cancel(struct gesture_tracker *tracker);

// Reset tracker and return gesture tracked
struct gesture *gesture_tracker_end(struct gesture_tracker *tracker);

#endif
