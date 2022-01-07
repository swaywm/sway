#ifndef _SWAY_MIRROR_H
#define _SWAY_MIRROR_H

#include <wlr/types/wlr_mirror_v1.h>
#include <wlr/util/box.h>
#include "sway/output.h"

/**
 * Allows mirroring: rendering some contents of one output (the src) on another
 * output (the dst). dst is fixed for the duration of the session, src may vary.
 *
 * See wlr_mirror_v1.h for full details.
 */

enum sway_mirror_flavour {
	/**
	 * Mirror the entirety of src on dst.
	 */
	SWAY_MIRROR_FLAVOUR_ENTIRE,

	/**
	 * Mirror a fixed box on one src on dst.
	 */
	SWAY_MIRROR_FLAVOUR_BOX,

	/**
	 * Mirror a container from its current src on dst, adjusting size and
	 * position as required.
	 */
	SWAY_MIRROR_FLAVOUR_CONTAINER,
};

/**
 * Immutable over session.
 */
struct sway_mirror_params {

	struct wlr_mirror_v1_params wlr_params;

	enum sway_mirror_flavour flavour;

	// ENTIRE, BOX
	struct wlr_output *output_src;
	struct wlr_box box;

	// CONTAINER
	size_t con_id;
};

struct sway_mirror {
	struct wl_list link;

	struct sway_mirror_params params;

	/**
	 * Frame is ready, from the potential src passed.
	 */
	struct wl_listener ready;

	/**
	 * Mirror session ended prematurely.
	 */
	struct wl_listener destroy;

	struct wlr_mirror_v1 *wlr_mirror_v1;
};

/**
 * Start a mirror session, adding a sway_mirror to server::mirrors.
 */
bool mirror_create(struct sway_mirror_params *params);

/**
 * Stop a mirror session.
 */
void mirror_destroy(struct sway_mirror *mirror);

/**
 * Stop all mirror sessions.
 */
void mirror_destroy_all();

/**
 * Output is currently in use as a mirror.
 */
bool mirror_output_is_mirror_dst(struct sway_output *output);

/**
 * Translate a layout box to local output. Returns true if within output.
 */
bool mirror_layout_box_within_output(struct wlr_box *box, struct wlr_output *output);

#endif

