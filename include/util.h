#ifndef _SWAY_UTIL_H
#define _SWAY_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server-protocol.h>

/**
 * Wrap i into the range [0, max[
 */
int wrap(int i, int max);

/**
 * Given a string that represents an RGB(A) color, result will be set to a
 * uint32_t version of the color, as long as it is valid. If it is invalid,
 * then false will be returned and result will be untouched.
 */
bool parse_color(const char *color, uint32_t *result);

void color_to_rgba(float dest[static 4], uint32_t color);

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

const char *sway_wl_output_subpixel_to_string(enum wl_output_subpixel subpixel);

bool sway_set_cloexec(int fd, bool cloexec);

#endif
