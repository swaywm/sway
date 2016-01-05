#ifndef _SWAY_UTIL_H
#define _SWAY_UTIL_H

#include <stdint.h>
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

#endif
