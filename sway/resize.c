#include <wlc/wlc.h>
#include <math.h>
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "input_state.h"
#include "handlers.h"
#include "resize.h"

static bool set_size_floating(int new_dimension, bool use_width) {
	swayc_t *view = get_focused_float(swayc_active_workspace());
	if (view) {
		if (use_width) {
			int current_width = view->width;
			view->desired_width = new_dimension;
			floating_view_sane_size(view);

			int new_x = view->x + (int)(((view->desired_width - current_width) / 2) * -1);
			view->width = view->desired_width;
			view->x = new_x;

			update_geometry(view);
		} else {
			int current_height = view->height;
			view->desired_height = new_dimension;
			floating_view_sane_size(view);

			int new_y = view->y + (int)(((view->desired_height - current_height) / 2) * -1);
			view->height = view->desired_height;
			view->y = new_y;

			update_geometry(view);
		}

		return true;
	}

	return false;
}

static bool resize_floating(int amount, bool use_width) {
	swayc_t *view = get_focused_float(swayc_active_workspace());

	if (view) {
		if (use_width) {
			return set_size_floating(view->width + amount, true);
		} else {
			return set_size_floating(view->height + amount, false);
		}
	}

	return false;
}

static bool resize_tiled(int amount, bool use_width) {
	swayc_t *parent = get_focused_view(swayc_active_workspace());
	swayc_t *focused = parent;
	swayc_t *sibling;
	if (!parent) {
		return true;
	}
	// Find the closest parent container which has siblings of the proper layout.
	// Then apply the resize to all of them.
	int i;
	if (use_width) {
		int lnumber = 0;
		int rnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_HORIZ && parent->parent->children) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->x != focused->x) {
						if (sibling->x < parent->x) {
							lnumber++;
						} else if (sibling->x > parent->x) {
							rnumber++;
						}
					}
				}
				if (rnumber || lnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent == &root_container) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d l conts, and %d r conts", parent->parent, lnumber, rnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		bool valid = true;
		for (i = 0; i < parent->parent->children->length; i++) {
			sibling = parent->parent->children->items[i];
			if (sibling->x != focused->x) {
				if (sibling->x < parent->x) {
					double pixels = -1 * amount;
					pixels /= lnumber;
					if (rnumber) {
						if ((sibling->width + pixels/2) < min_sane_w) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->width + pixels) < min_sane_w) {
							valid = false;
							break;
						}
					}
				} else if (sibling->x > parent->x) {
					double pixels = -1 * amount;
					pixels /= rnumber;
					if (lnumber) {
						if ((sibling->width + pixels/2) < min_sane_w) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->width + pixels) < min_sane_w) {
							valid = false;
							break;
						}
					}
				}
			} else {
				double pixels = amount;
				if (parent->width + pixels < min_sane_w) {
					valid = false;
					break;
				}
			}
		}
		if (valid) {
			for (i = 0; i < parent->parent->children->length; i++) {
				sibling = parent->parent->children->items[i];
				if (sibling->x != focused->x) {
					if (sibling->x < parent->x) {
						double pixels = -1 * amount;
						pixels /= lnumber;
						if (rnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_RIGHT);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_RIGHT);
						}
					} else if (sibling->x > parent->x) {
						double pixels = -1 * amount;
						pixels /= rnumber;
						if (lnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_LEFT);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_LEFT);
						}
					}
				} else {
					if (rnumber != 0 && lnumber != 0) {
						double pixels = amount;
						pixels /= 2;
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_LEFT);
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_RIGHT);
					} else if (rnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_RIGHT);
					} else if (lnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_LEFT);
					}
				}
			}
			// Recursive resize does not handle positions, let arrange_windows
			// take care of that.
			arrange_windows(swayc_active_workspace(), -1, -1);
		}
		return true;
	} else {
		int tnumber = 0;
		int bnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_VERT) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->y != focused->y) {
						if (sibling->y < parent->y) {
							bnumber++;
						} else if (sibling->y > parent->y) {
							tnumber++;
						}
					}
				}
				if (bnumber || tnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent->parent == NULL || parent->parent->children == NULL) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d b conts, and %d t conts", parent->parent, bnumber, tnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		bool valid = true;
		for (i = 0; i < parent->parent->children->length; i++) {
			sibling = parent->parent->children->items[i];
			if (sibling->y != focused->y) {
				if (sibling->y < parent->y) {
					double pixels = -1 * amount;
					pixels /= bnumber;
					if (tnumber) {
						if ((sibling->height + pixels/2) < min_sane_h) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->height + pixels) < min_sane_h) {
							valid = false;
							break;
						}
					}
				} else if (sibling->y > parent->y) {
					double pixels = -1 * amount;
					pixels /= tnumber;
					if (bnumber) {
						if ((sibling->height + pixels/2) < min_sane_h) {
							valid = false;
							break;
						}
					} else {
						if ((sibling->height + pixels) < min_sane_h) {
							valid = false;
							break;
						}
					}
				}
			} else {
				double pixels = amount;
				if (parent->height + pixels < min_sane_h) {
					valid = false;
					break;
				}
			}
		}
		if (valid) {
			for (i = 0; i < parent->parent->children->length; i++) {
				sibling = parent->parent->children->items[i];
				if (sibling->y != focused->y) {
					if (sibling->y < parent->y) {
						double pixels = -1 * amount;
						pixels /= bnumber;
						if (tnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_BOTTOM);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_BOTTOM);
						}
					} else if (sibling->x > parent->x) {
						double pixels = -1 * amount;
						pixels /= tnumber;
						if (bnumber) {
							recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_TOP);
						} else {
							recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_TOP);
						}
					}
				} else {
					if (bnumber != 0 && tnumber != 0) {
						double pixels = amount/2;
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_TOP);
						recursive_resize(parent, pixels, WLC_RESIZE_EDGE_BOTTOM);
					} else if (tnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_TOP);
					} else if (bnumber) {
						recursive_resize(parent, amount, WLC_RESIZE_EDGE_BOTTOM);
					}
				}
			}
			arrange_windows(swayc_active_workspace(), -1, -1);
		}
		return true;
	}
	return true;
}

static bool set_size_tiled(int amount, bool use_width) {
	int desired;
	swayc_t *focused = get_focused_view(swayc_active_workspace());

	if (use_width) {
		desired = amount - focused->width;
	} else {
		desired = amount - focused->height;
	}

	return resize_tiled(desired, use_width);
}

bool set_size(int dimension, bool use_width) {
	swayc_t *focused = get_focused_view_include_floating(swayc_active_workspace());

	if (focused) {
		if (focused->is_floating) {
			return set_size_floating(dimension, use_width);
		} else {
			return set_size_tiled(dimension, use_width);
		}
	}

	return false;
}

bool resize(int dimension, bool use_width, enum resize_dim_types dim_type) {
	swayc_t *focused = get_focused_view_include_floating(swayc_active_workspace());

	// translate "10 ppt" (10%) to appropriate # of pixels in case we need it
	float ppt_dim = (float)dimension / 100;

	if (use_width) {
		ppt_dim = focused->width * ppt_dim;
	} else {
		ppt_dim = focused->height * ppt_dim;
	}

	if (focused) {
		if (focused->is_floating) {
			// floating view resize dimensions should default to px, so only
			// use ppt if specified
			if (dim_type == RESIZE_DIM_PPT) {
				dimension = (int)ppt_dim;
			}

			return resize_floating(dimension, use_width);
		} else {
			// tiled view resize dimensions should default to ppt, so only use
			// px if specified
			if (dim_type != RESIZE_DIM_PX) {
				dimension = (int)ppt_dim;
			}

			return resize_tiled(dimension, use_width);
		}
	}

	return false;
}
