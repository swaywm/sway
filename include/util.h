#ifndef _SWAY_UTIL_H
#define _SWAY_UTIL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Wrap i into the range [0, max[
 */
int wrap(int i, int max);

/**
 * Given a string that represents an RGB(A) color, return a uint32_t
 * version of the color.
 */
uint32_t parse_color(const char *color);

/**
 * Given a string that represents a boolean, return the boolean value. This
 * function also takes in the current boolean value to support toggling. If
 * toggling is not desired, pass in true for current so that toggling values
 * get parsed as not true.
 */
bool parse_boolean(const char *boolean, bool current);

/**
 * Given a string that represents a floating point value, return a float.
 * Returns NAN on error.
 */
float parse_float(const char *value);

#endif
