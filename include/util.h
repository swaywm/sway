#ifndef _SWAY_UTIL_H
#define _SWAY_UTIL_H

#include <stdint.h>
#include <unistd.h>
#include <wlc/wlc.h>
#include <xkbcommon/xkbcommon.h>

/**
 * Wrap i into the range [0, max[
 */
int wrap(int i, int max);

/**
 * Count number of digits in int
 */
int numlen(int n);

/**
 * Get modifier mask from modifier name.
 *
 * Returns the modifer mask or 0 if the name isn't found.
 */
uint32_t get_modifier_mask_by_name(const char *name);

/**
 * Get modifier name from modifier mask.
 *
 * Returns the modifier name or NULL if it isn't found.
 */
const char *get_modifier_name_by_mask(uint32_t modifier);

/**
 * Get an array of modifier names from modifier_masks
 *
 * Populates the names array and return the number of names added.
 */
int get_modifier_names(const char **names, uint32_t modifier_masks);

/**
 * Get the pid of a parent process given the pid of a child process.
 *
 * Returns the parent pid or NULL if the parent pid cannot be determined.
 */
pid_t get_parent_pid(pid_t pid);

/**
 * Given a string that represents an RGB(A) color, return a uint32_t
 * version of the color.
 */
uint32_t parse_color(const char *color);

#endif
