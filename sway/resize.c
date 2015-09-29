#include <wlc/wlc.h>
#include <math.h>
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "input_state.h"
#include "handlers.h"

bool resize_tiled(int amount, bool use_width) {
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
