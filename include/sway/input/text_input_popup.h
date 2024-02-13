#ifndef _SWAY_INPUT_TEXT_INPUT_POPUP_H
#define _SWAY_INPUT_TEXT_INPUT_POPUP_H

#include "sway/tree/view.h"

struct sway_input_popup {
	struct sway_input_method_relay *relay;

	struct wlr_scene_tree *scene_tree;
	struct sway_popup_desc desc;
	struct wlr_input_popup_surface_v2 *popup_surface;

	struct wl_list link;

	struct wl_listener popup_destroy;
	struct wl_listener popup_surface_commit;

	struct wl_listener focused_surface_unmap;
};
#endif
