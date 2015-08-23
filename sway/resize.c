#include <wlc/wlc.h>
#include <math.h>
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "input_state.h"
#include "handlers.h"

bool mouse_resize_tiled(struct wlc_origin prev_pos) {
	swayc_t *view = container_under_pointer();
	bool valid = true;
	bool changed_tiling = false;
	double dx = mouse_origin.x - prev_pos.x;
	double dy = mouse_origin.y - prev_pos.y;
	if (view != pointer_state.tiling.init_view) {
		changed_tiling = true;
		valid = false;
		if (view->type != C_WORKSPACE) {
			if (get_swayc_in_direction(pointer_state.tiling.init_view, MOVE_LEFT) == view) {
				pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + 20;
				pointer_state.lock.temp_left = true;
			} else if (get_swayc_in_direction(pointer_state.tiling.init_view, MOVE_RIGHT) == view) {
				pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + pointer_state.tiling.init_view->width - 20;
				pointer_state.lock.temp_right = true;
			} else if (get_swayc_in_direction(pointer_state.tiling.init_view, MOVE_UP) == view) {
				pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + 20;
				pointer_state.lock.temp_up = true;
			} else if (get_swayc_in_direction(pointer_state.tiling.init_view, MOVE_DOWN) == view) {
				pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + pointer_state.tiling.init_view->height - 20;
				pointer_state.lock.temp_down = true;
			}
		}
	}

	if ((dx < 0 || mouse_origin.x < pointer_state.tiling.lock_pos.x) && pointer_state.lock.temp_left) {
		changed_tiling = true;
		valid = false;
	} else if (dx > 0 && pointer_state.lock.temp_left) {
		pointer_state.lock.temp_left = false;
		pointer_state.tiling.lock_pos.x = 0;
	}

	if ((dx > 0 || mouse_origin.x > pointer_state.tiling.lock_pos.x) && pointer_state.lock.temp_right) {
		changed_tiling = true;
		valid = false;
	} else if (dx < 0 && pointer_state.lock.temp_right) {
		pointer_state.lock.temp_right = false;
		pointer_state.tiling.lock_pos.x = 0;
	}

	if ((dy < 0 || mouse_origin.y < pointer_state.tiling.lock_pos.y) && pointer_state.lock.temp_up) {
		changed_tiling = true;
		valid = false;
	} else if (dy > 0 && pointer_state.lock.temp_up) {
		pointer_state.lock.temp_up = false;
		pointer_state.tiling.lock_pos.y = 0;
	}

	if ((dy > 0 || mouse_origin.y > pointer_state.tiling.lock_pos.y) && pointer_state.lock.temp_down) {
		changed_tiling = true;
		valid = false;
	} else if (dy < 0 && pointer_state.lock.temp_down) {
		pointer_state.lock.temp_down = false;
		pointer_state.tiling.lock_pos.y = 0;
	}

	if (!view->is_floating && valid) {
		// Handle layout resizes -- Find the biggest parent container then apply resizes to that
		// and its bordering siblings
		swayc_t *parent = view;
		if (!pointer_state.lock.bottom) {
			while (parent->type != C_WORKSPACE) {
				// TODO: Absolute value is a bad hack here to compensate for rounding. Find a better
				// way of doing this.
				if (fabs(parent->parent->y + parent->parent->height - (view->y + view->height)) <= 1) {
					parent = parent->parent;
				} else {
					break;
				}
			}
			if (parent->parent->children->length > 1 && parent->parent->layout == L_VERT) {
				swayc_t *sibling = get_swayc_in_direction(parent, MOVE_DOWN);
				if (sibling) {
					if ((parent->height > min_sane_h || dy > 0) && (sibling->height > min_sane_h || dy < 0)) {
						recursive_resize(parent, dy, WLC_RESIZE_EDGE_BOTTOM);
						recursive_resize(sibling, -1 * dy, WLC_RESIZE_EDGE_TOP);
						changed_tiling = true;
					} else {
						if (parent->height < min_sane_h) {
							//pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + 20;
							pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + pointer_state.tiling.init_view->height - 20;
							pointer_state.lock.temp_up = true;
						} else if (sibling->height < min_sane_h) {
							pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + pointer_state.tiling.init_view->height - 20;
							pointer_state.lock.temp_down = true;
						}
					}
				}
			}
		} else if (!pointer_state.lock.top) {
			while (parent->type != C_WORKSPACE) {
				if (fabs(parent->parent->y - view->y) <= 1) {
					parent = parent->parent;
				} else {
					break;
				}
			}
			if (parent->parent->children->length > 1 && parent->parent->layout == L_VERT) {
				swayc_t *sibling = get_swayc_in_direction(parent, MOVE_UP);
				if (sibling) {
					if ((parent->height > min_sane_h || dy < 0) && (sibling->height > min_sane_h || dy > 0)) {
						recursive_resize(parent, -1 * dy, WLC_RESIZE_EDGE_TOP);
						recursive_resize(sibling, dy, WLC_RESIZE_EDGE_BOTTOM);
						changed_tiling = true;
					} else {
						if (parent->height < min_sane_h) {
							//pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + pointer_state.tiling.init_view->height - 20;
							pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + 20;
							pointer_state.lock.temp_down = true;
						} else if (sibling->height < min_sane_h) {
							pointer_state.tiling.lock_pos.y = pointer_state.tiling.init_view->y + 20;
							pointer_state.lock.temp_up = true;
						}
					}
				}
			}
		}

		parent = view;
		if (!pointer_state.lock.right) {
			while (parent->type != C_WORKSPACE) {
				if (fabs(parent->parent->x + parent->parent->width - (view->x + view->width)) <= 1) {
					parent = parent->parent;
				} else {
					sway_log(L_DEBUG, "view: %f vs parent: %f", view->x + view->width, parent->parent->x + parent->parent->width);
					break;
				}
			}
			if (parent->parent->children->length > 1 && parent->parent->layout == L_HORIZ) {
				swayc_t *sibling = get_swayc_in_direction(parent, MOVE_RIGHT);
				if (sibling) {
					if ((parent->width > min_sane_w || dx > 0) && (sibling->width > min_sane_w || dx < 0)) {
						recursive_resize(parent, dx, WLC_RESIZE_EDGE_RIGHT);
						recursive_resize(sibling, -1 * dx, WLC_RESIZE_EDGE_LEFT);
						changed_tiling = true;
					} else {
						if (parent->width < min_sane_w) {
							pointer_state.lock.temp_left = true;
							pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + pointer_state.tiling.init_view->width - 20;
						} else if (sibling->width < min_sane_w) {
							pointer_state.lock.temp_right = true;
							pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + pointer_state.tiling.init_view->width - 20;
						}
					}
				}
			}
		} else if (!pointer_state.lock.left) {
			while (parent->type != C_WORKSPACE) {
				if (fabs(parent->parent->x - view->x) <= 1 && parent->parent) {
					parent = parent->parent;
				} else {
					break;
				}
			}
			if (parent->parent->children->length > 1 && parent->parent->layout == L_HORIZ) {
				swayc_t *sibling = get_swayc_in_direction(parent, MOVE_LEFT);
				if (sibling) {
					if ((parent->width > min_sane_w || dx < 0) && (sibling->width > min_sane_w || dx > 0)) {
						recursive_resize(parent, -1 * dx, WLC_RESIZE_EDGE_LEFT);
						recursive_resize(sibling, dx, WLC_RESIZE_EDGE_RIGHT);
						changed_tiling = true;
					} else {
						if (parent->width < min_sane_w) {
							pointer_state.lock.temp_right = true;
							pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + 20;
						} else if (sibling->width < min_sane_w) {
							pointer_state.lock.temp_left = true;
							pointer_state.tiling.lock_pos.x = pointer_state.tiling.init_view->x + 20;
						}
					}
				}
			}
		}
		arrange_windows(swayc_active_workspace(), -1, -1);
	}
	return changed_tiling;
}

bool resize_floating(struct wlc_origin prev_pos) {
	bool changed = false;
	swayc_t *view = container_under_pointer();
	uint32_t edge = 0;
	int dx = mouse_origin.x - prev_pos.x;
	int dy = mouse_origin.y - prev_pos.y;

	// Move and resize the view based on the dx/dy and mouse position
	int midway_x = view->x + view->width/2;
	int midway_y = view->y + view->height/2;
	if (dx < 0) {
		if (!pointer_state.lock.right) {
			if (view->width > min_sane_w) {
				changed = true;
				view->width += dx;
				edge += WLC_RESIZE_EDGE_RIGHT;
			}
		} else if (mouse_origin.x < midway_x && !pointer_state.lock.left) {
			changed = true;
			view->x += dx;
			view->width -= dx;
			edge += WLC_RESIZE_EDGE_LEFT;
		}
	} else if (dx > 0) {
		if (mouse_origin.x > midway_x && !pointer_state.lock.right) {
			changed = true;
			view->width += dx;
			edge += WLC_RESIZE_EDGE_RIGHT;
		} else if (!pointer_state.lock.left) {
			if (view->width > min_sane_w) {
				changed = true;
				view->x += dx;
				view->width -= dx;
				edge += WLC_RESIZE_EDGE_LEFT;
			}
		}
	}

	if (dy < 0) {
		if (!pointer_state.lock.bottom) {
			if (view->height > min_sane_h) {
				changed = true;
				view->height += dy;
				edge += WLC_RESIZE_EDGE_BOTTOM;
			}
		} else if (mouse_origin.y < midway_y && !pointer_state.lock.top) {
			changed = true;
			view->y += dy;
			view->height -= dy;
			edge += WLC_RESIZE_EDGE_TOP;
		}
	} else if (dy > 0) {
		if (mouse_origin.y > midway_y && !pointer_state.lock.bottom) {
			changed = true;
			view->height += dy;
			edge += WLC_RESIZE_EDGE_BOTTOM;
		} else if (!pointer_state.lock.top) {
			if (view->height > min_sane_h) {
				changed = true;
				view->y += dy;
				view->height -= dy;
				edge += WLC_RESIZE_EDGE_TOP;
			}
		}
	}
	if (changed) {
		struct wlc_geometry geometry = {
			.origin = {
				.x = view->x,
				.y = view->y
			},
			.size = {
				.w = view->width,
				.h = view->height
			}
		};
		wlc_view_set_geometry(view->handle, edge, &geometry);
	}
	return changed;
}

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
			if (parent->parent->layout == L_HORIZ) {
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
		if (parent == &root_container) {
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
