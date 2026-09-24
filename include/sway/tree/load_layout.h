#ifndef _SWAY_LOAD_LAYOUT_H
#define _SWAY_LOAD_LAYOUT_H

#include <stdbool.h>

struct sway_workspace;
struct sway_view;
struct sway_container;

/**
 * Append the container tree described by the JSON file at `path` to the given
 * workspace. The file may be either strict JSON (a single object or array) or
 * the i3-save-tree concatenated-object form (}\n{ between siblings). On error
 * leaves the workspace unmodified, returns false, and writes an allocated
 * error string to *error_out. Caller frees the error string.
 *
 * Currently tiling-only. Top-level `floating_nodes` (and any nested
 * floating_nodes) are skipped with a debug log.
 */
bool load_layout_from_file(struct sway_workspace *ws, const char *path,
		char **error_out);

/**
 * Walk the tree looking for a placeholder container (is_placeholder == true)
 * whose swallows list contains a criteria that matches the given view. Walks
 * tiling lists only, depth-first, document order. Returns the first match, or
 * NULL if none.
 *
 * Used by view_map to decide whether an incoming view should be installed
 * into a pre-existing placeholder slot rather than a freshly created one.
 */
struct sway_container *find_swallow_match(struct sway_view *view);

#endif
