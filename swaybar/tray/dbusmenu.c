#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
#include <pool-buffer.h>
#include <stdlib.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-protocol.h>

#include "background-image.h"
#include "cairo.h"
#include "cairo_util.h"
#include "list.h"
#include "log.h"
#include "pango.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/input.h"
#include "swaybar/tray/icon.h"
#include "swaybar/tray/item.h"
#include "swaybar/tray/tray.h"

static const char *menu_interface = "com.canonical.dbusmenu";

static void swaybar_dbusmenu_get_layout_root(struct swaybar_dbusmenu *menu);
static void swaybar_dbusmenu_get_layout(struct swaybar_dbusmenu *menu, int id);
static void swaybar_dbusmenu_draw(struct swaybar_dbusmenu *dbusmenu, int id);

struct swaybar_dbusmenu_hotspot {
	int x, y, width, height;
};

struct swaybar_dbusmenu_surface {
	struct xdg_popup *xdg_popup;
	struct xdg_surface *xdg_surface;
	struct wl_surface *surface;
	struct pool_buffer *current_buffer;
	struct pool_buffer buffers[2];
	int width, height;
	bool configured;
};

enum menu_toggle_type {
	MENU_NONE,
	MENU_CHECKMARK,
	MENU_RADIO
};

struct swaybar_dbusmenu_menu_item {
	struct swaybar_dbusmenu_hotspot hotspot;
	// Set if the item has a submenu
	struct swaybar_dbusmenu_menu *submenu;
	// The menu in which the item is displayed
	struct swaybar_dbusmenu_menu *menu;
	struct swaybar_dbusmenu_menu_item *parent_item;
	int id;
	int toggle_state;
	char *label;
	char *icon_name;
	cairo_surface_t *icon_data;
	enum menu_toggle_type toggle_type;
	bool enabled;
	bool visible;
	bool is_separator;
};

struct swaybar_dbusmenu_menu {
	struct swaybar_dbusmenu *dbusmenu;
	struct swaybar_dbusmenu_surface *surface;
	struct swaybar_dbusmenu_menu_item *last_hovered_item;
	list_t *items; // struct swaybar_dbusmenu_menu_item
	int item_id;
	int x, y;
};

struct swaybar_dbusmenu {
	struct swaybar_sni *sni;
	struct swaybar_output *output;
	struct swaybar_seat *seat;
	struct swaybar_dbusmenu_menu *menu;
	struct swaybar *bar;
	int serial;
	int x, y;
};

struct get_layout_callback_data {
	struct swaybar_dbusmenu *menu;
	int id;
};

struct png_stream {
	const void *data;
	size_t left;
};

static void commit_menu_surface(struct swaybar_dbusmenu_menu *menu)
{
	struct swaybar_dbusmenu_surface * dbusmenu_surface = menu->surface;
	if (!dbusmenu_surface->configured || dbusmenu_surface->current_buffer == NULL) {
		return;
	}

	struct wl_surface *surface = dbusmenu_surface->surface;
	wl_surface_set_buffer_scale(surface, menu->dbusmenu->output->scale);
	wl_surface_attach(surface, dbusmenu_surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, dbusmenu_surface->width, dbusmenu_surface->height);
	wl_surface_commit(surface);
}

static int handle_items_properties_updated(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct swaybar_sni *sni = data;
	sway_log(SWAY_DEBUG, "%s%s item properties updated", sni->service, sni->menu);

	// TODO: Optimize. Update only needed properties
	if (sni->tray->menu) {
		swaybar_dbusmenu_get_layout_root(sni->tray->menu);
	}
	return 0;
}

static int handle_layout_updated(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct swaybar_sni *sni = data;
	sway_log(SWAY_DEBUG, "%s%s layout updated", sni->service, sni->menu);

	int id;
	sd_bus_message_read(msg, "ui", NULL, &id);
	if (sni->tray->menu) {
		swaybar_dbusmenu_get_layout(sni->tray->menu, id);
	}
	return 0;
}

static int handle_item_activation_requested(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	return 0; // TODO: Implement handling of hotkeys for opening the menu
}

static struct swaybar_dbusmenu_surface *swaybar_dbusmenu_surface_create() {
	struct swaybar_dbusmenu_surface *dbusmenu = calloc(1, 
			sizeof(struct swaybar_dbusmenu_surface));
	if (!dbusmenu) {
		sway_log(SWAY_DEBUG, "Could not allocate dbusmenu");
	}
	return dbusmenu;
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);

	struct swaybar_dbusmenu_menu *menu = data;
	menu->surface->configured = true;
	commit_menu_surface(menu);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_popup_configure(void *data, struct xdg_popup *xdg_popup,
		int32_t x, int32_t y, int32_t width, int32_t height) {
	// intentionally left blank
}

static void destroy_dbusmenu_surface(
		struct swaybar_dbusmenu_surface *dbusmenu_surface) {
	if (!dbusmenu_surface) {
		return;
	}

	if (dbusmenu_surface->xdg_popup) {
		xdg_popup_destroy(dbusmenu_surface->xdg_popup);
		dbusmenu_surface->xdg_popup = NULL;
	}
	if (dbusmenu_surface->surface) {
		xdg_surface_destroy(dbusmenu_surface->xdg_surface);
		wl_surface_destroy(dbusmenu_surface->surface);
		dbusmenu_surface->surface = NULL;
	}
	destroy_buffer(&dbusmenu_surface->buffers[0]);
	destroy_buffer(&dbusmenu_surface->buffers[1]);

	free(dbusmenu_surface);
}

static void close_menu(struct swaybar_dbusmenu_menu *menu) {
	if (!menu) {
		return;
	}

	if (menu->surface) {
		destroy_dbusmenu_surface(menu->surface);
		menu->surface = NULL;

		int id = menu->item_id;
		struct swaybar_sni *sni = menu->dbusmenu->sni;
		sd_bus_call_method_async(sni->tray->bus, NULL, sni->service, sni->menu,
				menu_interface, "Event", NULL, NULL, "isvu", id, "closed", "y",
				0, time(NULL));
		sway_log(SWAY_DEBUG, "%s%s closed id %d", sni->service, sni->menu, id);
	}
}

static void close_menus(struct swaybar_dbusmenu_menu *menu) {
	if (!menu) {
		return;
	}

	if (menu->items) {
		for (int i = 0; i < menu->items->length; ++i) {
			struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
			if (item->submenu && item->submenu->item_id != 0) {
				close_menus(item->submenu);
			}
		}
	}

	close_menu(menu);
}

static void swaybar_dbusmenu_menu_destroy(struct swaybar_dbusmenu_menu *menu) {
	if (!menu) {
		return;
	}

	if (menu->items) {
		for (int i = 0; i < menu->items->length; ++i) {
			struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
			struct swaybar_dbusmenu_menu *child_menu = item->submenu;
			if (child_menu && child_menu->item_id != 0) {
				swaybar_dbusmenu_menu_destroy(item->submenu);
			}
			free(item->label);
			free(item->icon_name);
			free(item->icon_data);
			free(item);
		}
	}
	list_free(menu->items);
	free(menu);
}

void swaybar_dbusmenu_destroy(struct swaybar_dbusmenu *menu) {
	if (!menu) {
		return;
	}

	menu->sni->tray->menu = NULL;
	menu->sni->tray->menu_pointer_focus = NULL;

	close_menus(menu->menu);
	swaybar_dbusmenu_menu_destroy(menu->menu);
	free(menu);
}

static void xdg_popup_done(void *data, struct xdg_popup *xdg_popup) {
	struct swaybar_dbusmenu_menu *menu = data;
	swaybar_dbusmenu_destroy(menu->dbusmenu);
}

static const struct xdg_popup_listener xdg_popup_listener = {
	.configure = xdg_popup_configure, .popup_done = xdg_popup_done};

static struct swaybar_dbusmenu_menu_item *
find_item_under_menu(struct swaybar_dbusmenu_menu *menu, int item_id) {
	if (!menu->items) {
		return NULL;
	}

	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
		if (item->id == item_id) {
			return item;
		}
		if (item->submenu && item->submenu->item_id != 0) {
			struct swaybar_dbusmenu_menu_item *found_item = 
				find_item_under_menu(item->submenu, item_id);
			if (found_item) {
				return found_item;
			}
		}
	}

	return NULL;
}

static struct swaybar_dbusmenu_menu_item *
find_item(struct swaybar_dbusmenu *dbusmenu, int item_id) {
	if (!dbusmenu->menu) {
		return NULL;
	}

	return find_item_under_menu(dbusmenu->menu, item_id);
}

static struct swaybar_dbusmenu_menu *
find_parent_menu_under_menu(struct swaybar_dbusmenu_menu *menu, 
		struct swaybar_dbusmenu_menu *child_menu) {
	if (!menu->items) {
		return NULL;
	}

	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
		struct swaybar_dbusmenu_menu *maybe_child_menu = item->submenu;
		if (maybe_child_menu && maybe_child_menu->item_id != 0) {
			if (maybe_child_menu == child_menu) {
				return menu;
			}
			maybe_child_menu = find_parent_menu_under_menu(maybe_child_menu, child_menu);
			if (maybe_child_menu) {
				return maybe_child_menu;
			}
		}
	}

	return NULL;
}

static struct swaybar_dbusmenu_menu *
find_parent_menu(struct swaybar_dbusmenu_menu *menu) {
	if (menu && menu->item_id == 0) {
		return NULL;
	}
	struct swaybar_dbusmenu_menu *root_menu = menu->dbusmenu->menu;
	return find_parent_menu_under_menu(root_menu, menu);
}

static bool is_in_hotspot(struct swaybar_dbusmenu_hotspot *hotspot, int x, int y) {
	if (!hotspot) {
		return false;
	}

	if (hotspot->x <= x && x < hotspot->x + hotspot->width && hotspot->y <= y &&
			y < hotspot->y + hotspot->height) {
		return true;
	}

	return false;
}

static void draw_menu_items(cairo_t *cairo, struct swaybar_dbusmenu_menu *menu,
		int *surface_x, int *surface_y, int *surface_width, int *surface_height,
		bool open) {
	struct swaybar_sni *sni = menu->dbusmenu->sni;
	struct swaybar_tray *tray = sni->tray;
	struct swaybar_output *output = menu->dbusmenu->output;
	struct swaybar_config *config = menu->dbusmenu->output->bar->config;

	int padding = config->tray_padding * output->scale;

	list_t *items = menu->items;
	int height = 0;

	*surface_y = 0;
	*surface_x = 0;
	*surface_width = 0;
	bool is_icon_drawn = false;
	int icon_size = 0;

	for (int i = 0; i < items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = items->items[i];

		if (!item->visible) {
			continue;
		}

		int new_height = height;
		if (item->is_separator) {
			// drawn later, after the width is known
			new_height = height + output->scale;
		} else if (item->label) {
			cairo_move_to(cairo, padding, height + padding);

			// draw label
			if (item->enabled) {
				cairo_set_source_u32(cairo, config->colors.focused_statusline);
			} else {
				uint32_t c = config->colors.focused_statusline;
				uint32_t disabled_color = c - ((c & 0xFF) >> 1);
				cairo_set_source_u32(cairo, disabled_color);
			}
			render_text(cairo, config->font_description, output->scale, false, "%s",
					item->label);

			// draw icon or menu indicator if needed
			int text_height;
			int text_width;
			get_text_size(cairo, config->font_description, &text_width, &text_height,
					NULL, output->scale, false, "%s", item->label);
			text_width += padding;
			int size = text_height;
			int x = -2 * padding - size;
			int y = height + padding;
			icon_size = 2 * padding + size;
			cairo_set_source_u32(cairo, config->colors.focused_statusline);
			if (item->icon_name) {
				list_t *icon_search_paths = create_list();
				list_cat(icon_search_paths, tray->basedirs);
				if (sni->menu_icon_theme_paths) {
					for (char **path = sni->menu_icon_theme_paths; *path; ++path) {
						list_add(icon_search_paths, *path);
					}
				}
				if (sni->icon_theme_path) {
					list_add(icon_search_paths, sni->icon_theme_path);
				}
				int min_size, max_size;
				char *icon_path =
				find_icon(tray->themes, icon_search_paths, item->icon_name, size,
						config->icon_theme, &min_size, &max_size);
				list_free(icon_search_paths);

				if (icon_path) {
					cairo_surface_t *icon = load_background_image(icon_path);
					free(icon_path);
					cairo_surface_t *icon_scaled =
					cairo_image_surface_scale(icon, size, size);
					cairo_surface_destroy(icon);

					cairo_set_source_surface(cairo, icon_scaled, x, y);
					cairo_rectangle(cairo, x, y, size, size);
					cairo_fill(cairo);
					cairo_surface_destroy(icon_scaled);
					is_icon_drawn = true;
				}
			} else if (item->icon_data) {
				cairo_surface_t *icon = cairo_image_surface_scale(item->icon_data, size, size);
				cairo_set_source_surface(cairo, icon, x, y);
				cairo_rectangle(cairo, x, y, size, size);
				cairo_fill(cairo);
				cairo_surface_destroy(icon);
				is_icon_drawn = true;
			} else if (item->toggle_type == MENU_CHECKMARK) {
				cairo_rectangle(cairo, x, y, size, size);
				cairo_fill(cairo);
				cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
				if (item->toggle_state == 1) { // tick
					cairo_move_to(cairo, x + size * 3.0 / 4, y + size * 5.0 / 16.0);
					cairo_line_to(cairo, x + size * 3.0 / 8, y + size * 11.0 / 16.0);
					cairo_line_to(cairo, x + size / 4.0, y + size * 9.0 / 16.0);
					cairo_stroke(cairo);
				} else if (item->toggle_state != 0) { // horizontal line
					cairo_rectangle(cairo, x + size / 4.0, y + size / 2.0 - 1,
							size / 2.0, 2);
					cairo_fill(cairo);
				}
				cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
				is_icon_drawn = true;
			} else if (item->toggle_type == MENU_RADIO) {
				cairo_arc(cairo, x + size / 2.0, y + size / 2.0, size / 2.0, 0, 7);
				cairo_fill(cairo);
				if (item->toggle_state == 1) {
					cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
					cairo_arc(cairo, x + size / 2.0, y + size / 2.0, size / 4.0, 0, 7);
					cairo_fill(cairo);
					cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
				}
				is_icon_drawn = true;
			} else if (item->submenu) { // arrowhead
				cairo_move_to(cairo, x + size / 4.0, y + size / 2.0);
				cairo_line_to(cairo, x + size * 3.0 / 4, y + size / 4.0);
				cairo_line_to(cairo, x + size * 3.0 / 4, y + size * 3.0 / 4);
				cairo_fill(cairo);
				is_icon_drawn = true;
			}

			*surface_width = *surface_width < text_width ? text_width : *surface_width;
			new_height = height + text_height + 2 * padding;
		} else {
			continue;
		}

		struct swaybar_dbusmenu_hotspot *hotspot = &item->hotspot;
		hotspot->y = height;

		hotspot->y = height;
		hotspot->height = new_height - height;
		// x and width is not known at the moment

		height = new_height;
	}

	if (height == 0) {
		return;
	}

	if (is_icon_drawn) {
		*surface_x = -icon_size - padding;
		*surface_width += icon_size + padding;
	}

	*surface_width += padding;
	*surface_height = height;

	// Make sure height and width are divideable by scale
	// otherwise the menu will not showup
	if (*surface_width % output->scale != 0) {
		*surface_width -= *surface_width % output->scale;
	}
	if (*surface_height % output->scale != 0) {
		*surface_height -= *surface_height % output->scale;
	}

	cairo_set_line_width(cairo, output->scale);
	cairo_set_source_u32(cairo, config->colors.focused_separator);
	for (int i = 0; i < items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = items->items[i];
		struct swaybar_dbusmenu_hotspot *hotspot = &item->hotspot;
		hotspot->x = 0;
		hotspot->width = *surface_width;
		if (item->is_separator) {
			int y = hotspot->y + hotspot->height / 2.0;
			cairo_move_to(cairo, *surface_x, y);
			cairo_line_to(cairo, *surface_x + *surface_width, y);
			cairo_stroke(cairo);
		} else if (!open && item->enabled &&
					is_in_hotspot(hotspot,
						tray->menu->seat->pointer.x * output->scale,
						tray->menu->seat->pointer.y * output->scale)) {
			cairo_save(cairo);
			cairo_set_operator(cairo, CAIRO_OPERATOR_DEST_OVER);
			cairo_rectangle(cairo, *surface_x, hotspot->y, *surface_width,
					hotspot->height);
			cairo_set_source_u32(cairo,
					sni->tray->bar->config->colors.focused_separator);
			cairo_fill(cairo);
			cairo_restore(cairo);
		}
	}
}

struct swaybar_dbusmenu_menu *find_menu_id(struct swaybar_dbusmenu_menu *menu,
		int id) {
	if (!menu) {
		return NULL;
	}
	if (menu->item_id == id) {
		return menu;
	}

	if (menu->items) {
		for (int i = 0; i < menu->items->length; ++i) {
			struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
			struct swaybar_dbusmenu_menu *child_menu = item->submenu;
			if (child_menu) {
				if (child_menu->item_id == id) {
					return child_menu;
				}
				if (child_menu->item_id == 0) {
					continue;
				}
				struct swaybar_dbusmenu_menu *child_child_menu = find_menu_id(child_menu, id);
				if (child_child_menu) {
					return child_child_menu;
				}
			}
		}
	}

	return NULL;
}

static void swaybar_dbusmenu_draw_menu(struct swaybar_dbusmenu_menu *menu,
		int id, bool open) {
	// For now just search for menu with id
	struct swaybar_tray *tray = menu->dbusmenu->sni->tray;
	menu = find_menu_id(menu->dbusmenu->menu, id);
	if (!menu) {
		return;
	}

	if (!menu->surface) {
		menu->surface = swaybar_dbusmenu_surface_create();
		if (!menu->surface) {
			sway_log(SWAY_ERROR, "Could not create surface for menu %d", menu->item_id);
			return;
		}
	}

	cairo_surface_t *recorder = 
		cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, NULL);
	if (!recorder) {
		return;
	}
	cairo_t *cairo = cairo_create(recorder);
	if (!cairo) {
		cairo_surface_destroy(recorder);
		return;
	}
	int surface_x, surface_y, surface_width, surface_height;
	draw_menu_items(cairo, menu, &surface_x, &surface_y, &surface_width,
					&surface_height, open);

	struct swaybar *bar = menu->dbusmenu->sni->tray->bar;
	struct swaybar_dbusmenu_surface *dbusmenu_surface = menu->surface;
	dbusmenu_surface->current_buffer = get_next_buffer(
		bar->shm, dbusmenu_surface->buffers, surface_width, surface_height);

	if (!dbusmenu_surface->current_buffer) {
		cairo_surface_destroy(recorder);
		cairo_destroy(cairo);
		return;
	}

	cairo_t *shm = dbusmenu_surface->current_buffer->cairo;
	cairo_set_operator(shm, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_u32(
		shm, menu->dbusmenu->sni->tray->bar->config->colors.focused_background);
	cairo_paint(shm);

	cairo_set_operator(shm, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface(shm, recorder, -surface_x, -surface_y);
	cairo_paint(shm);

	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);

	if (dbusmenu_surface->width != surface_width ||
			dbusmenu_surface->height != surface_height) {
		if (dbusmenu_surface->surface) {
			xdg_surface_destroy(dbusmenu_surface->xdg_surface);
			dbusmenu_surface->xdg_surface = NULL;
			wl_surface_destroy(dbusmenu_surface->surface);
			dbusmenu_surface->surface = NULL;
			sway_log(SWAY_DEBUG, "Destroy xdg popup");
			xdg_popup_destroy(dbusmenu_surface->xdg_popup);
			dbusmenu_surface->xdg_popup = NULL;
		}

		// configure & position popup surface
		struct wl_surface *surface = wl_compositor_create_surface(bar->compositor);
		struct xdg_surface *xdg_surface =
			xdg_wm_base_get_xdg_surface(menu->dbusmenu->bar->wm_base, surface);
		struct xdg_positioner *positioner =
			xdg_wm_base_create_positioner(menu->dbusmenu->bar->wm_base);

		// find the menu item (if any) which requested to open this submenu
		// to find out on which x and y coordinate the submenu should be drawn
		struct swaybar_dbusmenu_menu_item *item =
			find_item(menu->dbusmenu, menu->item_id);
		struct swaybar_output *output = menu->dbusmenu->output;
		int x = menu->item_id == 0 ? menu->dbusmenu->x
				: item->hotspot.x / output->scale;
		int y = menu->item_id == 0 ? menu->dbusmenu->y
				: item->hotspot.y / output->scale;

		xdg_positioner_set_offset(positioner, 0, 0);
		// Need to divide through scale because surface width/height is scaled
		xdg_positioner_set_size(positioner, surface_width / output->scale,
								surface_height / output->scale);

		int padding = (tray->bar->config->tray_padding * output->scale) / 2;
		if (bar->config->position & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) { // top bar
			xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
			xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_LEFT);
			xdg_positioner_set_anchor_rect(positioner, x, y - padding, 1, 1);
		} else {
			xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
			xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP_LEFT);
			xdg_positioner_set_anchor_rect(
			positioner, x, item->hotspot.height / output->scale, 1, 1);
		}

		struct xdg_popup *xdg_popup;
		struct swaybar_dbusmenu_menu *parent_menu = find_parent_menu(menu);
		if (!parent_menu) {
			// Top level menu
			xdg_popup = xdg_surface_get_popup(xdg_surface, NULL, positioner);
			zwlr_layer_surface_v1_get_popup(output->layer_surface, xdg_popup);
		} else {
			// Nested menu
			xdg_popup = xdg_surface_get_popup(
					xdg_surface, parent_menu->surface->xdg_surface, positioner);
		}
		xdg_positioner_destroy(positioner);

		xdg_popup_grab(xdg_popup, menu->dbusmenu->seat->wl_seat,
				menu->dbusmenu->serial);
		xdg_popup_add_listener(xdg_popup, &xdg_popup_listener, menu);
		xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, menu);
		wl_surface_commit(surface);

		dbusmenu_surface->xdg_popup = xdg_popup;
		dbusmenu_surface->xdg_surface = xdg_surface;
		dbusmenu_surface->surface = surface;
		dbusmenu_surface->width = surface_width;
		dbusmenu_surface->height = surface_height;
		dbusmenu_surface->configured = false;
	}

	commit_menu_surface(menu);
}

static void swaybar_dbusmenu_draw(struct swaybar_dbusmenu *dbusmenu, int id) {
	if (!dbusmenu || !dbusmenu->menu) {
		sway_log(SWAY_ERROR, "Can not draw dbusmenu, menu structure not initialized yet!");
		return;
	}
	swaybar_dbusmenu_draw_menu(dbusmenu->menu, id, true);
}

static cairo_status_t read_png_stream(void *closure, unsigned char *data,
		unsigned int length) {
	struct png_stream *png_stream = closure;
	if (length > png_stream->left) {
		return CAIRO_STATUS_READ_ERROR;
	}
	memcpy(data, png_stream->data, length);
	png_stream->data += length;
	png_stream->left -= length;
	return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *read_png(const void *data, size_t data_size) {
	struct png_stream png_stream = {0};
	png_stream.data = data;
	png_stream.left = data_size;
	cairo_surface_t *surface =
		cairo_image_surface_create_from_png_stream(read_png_stream, &png_stream);

	if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
		return surface;
	}

	cairo_surface_destroy(surface);
	return NULL;
}

static int about_to_show_callback(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct swaybar_sni_slot *slot = data;
	struct swaybar_sni *sni = slot->sni;
	int menu_id = slot->menu_id;
	wl_list_remove(&slot->link);
	free(slot);

	int need_update;
	sd_bus_message_read_basic(msg, 'b', &need_update);
	if (need_update) {
		swaybar_dbusmenu_get_layout(sni->tray->menu, menu_id);
	}

	swaybar_dbusmenu_draw(sni->tray->menu, menu_id);

	sd_bus_call_method_async(sni->tray->bus, NULL, sni->service, sni->menu,
		menu_interface, "Event", NULL, NULL, "isvu", menu_id, "opened", "y", 0, 
		time(NULL));

	sway_log(SWAY_DEBUG, "%s%s opened id %d", sni->service, sni->menu, menu_id);

	return 0;
}

static void open_menu_id(struct swaybar_dbusmenu *dbusmenu, int menu_id) {
	struct swaybar_dbusmenu_menu *menu = find_menu_id(dbusmenu->menu, menu_id);
	if (!menu || menu->surface) {
		// menu could not be found or is already shown
		return;
	}

	struct swaybar_sni *sni = dbusmenu->sni;
	struct swaybar_sni_slot *slot = calloc(1, sizeof(struct swaybar_sni_slot));
	slot->sni = sni;
	slot->menu_id = menu_id;

	int ret = sd_bus_call_method_async(sni->tray->bus, &slot->slot, sni->service,
		sni->menu, menu_interface, "AboutToShow", about_to_show_callback, slot, "i", 
		menu_id);

	if (ret >= 0) {
		wl_list_insert(&sni->slots, &slot->link);
	} else {
		sway_log(SWAY_ERROR, "%s%s failed to send AboutToShow signal: %s",
			sni->service, sni->menu, strerror(-ret));
		free(slot);
	}
}

static int update_item_properties(struct swaybar_dbusmenu_menu_item *item,
		sd_bus_message *msg) {
	sd_bus_message_enter_container(msg, 'a', "{sv}");
	while (!sd_bus_message_at_end(msg, 0)) {
		sd_bus_message_enter_container(msg, 'e', "sv");
		char *key, *log_value;
		sd_bus_message_read_basic(msg, 's', &key);
		if (strcmp(key, "type") == 0) {
			char *type;
			sd_bus_message_read(msg, "v", "s", &type);
			item->is_separator = strcmp(type, "separator") == 0;
			log_value = type;
		} else if (strcmp(key, "label") == 0) {
			char *label;
			sd_bus_message_read(msg, "v", "s", &label);
			item->label = realloc(item->label, strlen(label) + 1);
			if (!item->label) {
				return -ENOMEM;
			}
			int i = 0;
			for (char *c = label; *c; ++c) {
				if (*c == '_' && !*++c) {
					break;
				}
				item->label[i++] = *c;
			}
			item->label[i] = '\0';
			log_value = label;
		} else if (strcmp(key, "enabled") == 0) {
			int enabled;
			sd_bus_message_read(msg, "v", "b", &enabled);
			item->enabled = enabled;
			log_value = item->enabled ? "true" : "false";
		} else if (strcmp(key, "visible") == 0) {
			int visible;
			sd_bus_message_read(msg, "v", "b", &visible);
			item->visible = visible;
			log_value = item->visible ? "true" : "false";
		} else if (strcmp(key, "icon-name") == 0) {
			sd_bus_message_read(msg, "v", "s", &item->icon_name);
			item->icon_name = strdup(item->icon_name);
			log_value = item->icon_name;
		} else if (strcmp(key, "icon-data") == 0) {
			const void *data;
			size_t data_size;
			sd_bus_message_enter_container(msg, 'v', "ay");
			sd_bus_message_read_array(msg, 'y', &data, &data_size);
			sd_bus_message_exit_container(msg);
			item->icon_data = read_png(data, data_size);
			log_value = item->icon_data ? "<success>" : "<failure>";
		} else if (strcmp(key, "toggle-type") == 0) {
			char *toggle_type;
			sd_bus_message_read(msg, "v", "s", &toggle_type);
			if (strcmp(toggle_type, "checkmark") == 0) {
				item->toggle_type = MENU_CHECKMARK;
			} else if (strcmp(toggle_type, "radio") == 0) {
				item->toggle_type = MENU_RADIO;
			}
			log_value = toggle_type;
		} else if (strcmp(key, "toggle-state") == 0) {
			sd_bus_message_read(msg, "v", "i", &item->toggle_state);
			log_value = item->toggle_state == 0 ? 
				"off" : item->toggle_state == 1 ? "on" : "indeterminate";
		} else if (strcmp(key, "children-display") == 0) {
			char *children_display;
			sd_bus_message_read(msg, "v", "s", &children_display);
			if (strcmp(children_display, "submenu") == 0) {
				struct swaybar_dbusmenu_menu *submenu;
				if (item->id != 0) {
					submenu = calloc(1, sizeof(struct swaybar_dbusmenu_menu));
					if (!submenu) {
						sway_log(SWAY_ERROR, "Could not allocate submenu");
						return -ENOMEM;
					}
				} else {
					submenu = item->menu;
				}
				submenu->item_id = item->id;
				submenu->dbusmenu = item->menu->dbusmenu;
				item->submenu = submenu;
			}
			log_value = children_display;
		} else {
			// Ignored: shortcut, disposition
			sd_bus_message_skip(msg, "v");
			log_value = "<ignored>";
		}
		sd_bus_message_exit_container(msg);
		sway_log(SWAY_DEBUG, "%s%s %s = '%s'", item->menu->dbusmenu->sni->service,
		item->menu->dbusmenu->sni->menu, key, log_value);
	}
	return sd_bus_message_exit_container(msg);
}

static int get_layout_callback(sd_bus_message *msg, void *data, 
		sd_bus_error *error) {
	struct swaybar_sni_slot *slot = data;
	struct swaybar_sni *sni = slot->sni;
	int menu_id = slot->menu_id;
	wl_list_remove(&slot->link);
	free(slot);

	struct swaybar_dbusmenu *dbusmenu = sni->tray->menu;
	if (dbusmenu == NULL) {
		return 0;
	}

	if (sd_bus_message_is_method_error(msg, NULL)) {
		sway_log(SWAY_ERROR, "%s%s failed to get layout: %s",
			dbusmenu->sni->service, dbusmenu->sni->menu,
		sd_bus_message_get_error(msg)->message);
		return sd_bus_message_get_errno(msg);
	}

	// Parse the layout. The layout comes as a recursive structure as
	// dbus message in the following form (ia{sv}av)

	// Skip the menu revision
	sd_bus_message_skip(msg, "u");

	sni->tray->menu_pointer_focus = NULL;

	bool already_open = false;
	struct swaybar_dbusmenu_menu *menu_to_update =
	find_menu_id(dbusmenu->menu, menu_id);
	if (menu_to_update && menu_to_update->surface) {
		already_open = true;
	}

	if (dbusmenu->menu) {
		close_menus(dbusmenu->menu);
		swaybar_dbusmenu_menu_destroy(dbusmenu->menu);
		dbusmenu->menu = NULL;
	}

	struct swaybar_dbusmenu_menu_item *parent_item = NULL;
	struct swaybar_dbusmenu_menu *menu = calloc(1,
			sizeof(struct swaybar_dbusmenu_menu));
	if (!menu) {
		sway_log(SWAY_ERROR, "Could not allocate menu");
		return -ENOMEM;
	}
	dbusmenu->menu = menu;
	menu->dbusmenu = dbusmenu;
	int ret = 0;
	while (!sd_bus_message_at_end(msg, 1)) {
		sd_bus_message_enter_container(msg, 'r', "ia{sv}av");

		struct swaybar_dbusmenu_menu_item *item 
			= calloc(1, sizeof(struct swaybar_dbusmenu_menu_item));
		if (!item) {
			ret = -ENOMEM;
			break;
		}

		// default properties
		item->parent_item = parent_item;
		item->menu = menu;
		item->enabled = true;
		item->visible = true;
		item->toggle_state = -1;

		// Read the id
		sd_bus_message_read_basic(msg, 'i', &item->id);

		// Process a{sv}. a{sv} contains key-value pairs
		ret = update_item_properties(item, msg);
		if (!menu->items) {
			menu->items = create_list();
		}
		list_add(menu->items, item);
		if (ret < 0) {
			break;
		}
		if (item->id != 0 && item->submenu) {
			menu = item->submenu;
		}

		sd_bus_message_enter_container(msg, 'a', "v");

		parent_item = item;
		while (parent_item && sd_bus_message_at_end(msg, 0)) {
			if (parent_item->submenu) {
				menu = find_parent_menu(menu);
			}
			parent_item = parent_item->parent_item;

			sd_bus_message_exit_container(msg);
			sd_bus_message_exit_container(msg);
			sd_bus_message_exit_container(msg);
		}

		if (parent_item) {
			sd_bus_message_enter_container(msg, 'v', "(ia{sv}av)");
		}
	}

	if (already_open) {
		swaybar_dbusmenu_draw(sni->tray->menu, menu_id);
	} else {
		open_menu_id(dbusmenu, 0);
	}

	return 0;
}

static void swaybar_dbusmenu_subscribe_signal(struct swaybar_dbusmenu *menu,
		const char *signal_name, sd_bus_message_handler_t callback) {
	int ret = sd_bus_match_signal_async( menu->sni->tray->bus, NULL, 
			menu->sni->service, menu->sni->menu, menu_interface, signal_name, callback,
			NULL, menu->sni);

	if (ret < 0) {
		sway_log(SWAY_ERROR, "%s%s failed to subscribe to signal %s: %s",
			menu->sni->service, menu->sni->menu, signal_name, strerror(-ret));
	}
}

static void swaybar_dbusmenu_setup_signals(struct swaybar_dbusmenu *menu) {
	swaybar_dbusmenu_subscribe_signal(menu, "ItemsPropertiesUpdated",
		handle_items_properties_updated);
	swaybar_dbusmenu_subscribe_signal(menu, "LayoutUpdated",
		handle_layout_updated);
	swaybar_dbusmenu_subscribe_signal(menu, "ItemActivationRequested",
		handle_item_activation_requested);
}

static void swaybar_dbusmenu_get_layout(struct swaybar_dbusmenu *menu, int id) {
	if (menu == NULL) {
		return;
	}

	struct swaybar_sni_slot *slot = calloc(1, sizeof(struct swaybar_sni_slot));
	if (slot == NULL) {
		sway_log(SWAY_ERROR, "Could not allocate swaybar_sni_slot");
		return;
	}
	slot->sni = menu->sni;
	slot->menu_id = id;

	int ret =
		sd_bus_call_method_async(menu->sni->tray->bus, NULL, menu->sni->service,
				menu->sni->menu, menu_interface, "GetLayout",
				get_layout_callback, slot, "iias", id, -1, NULL);

	if (ret >= 0) {
		wl_list_insert(&menu->sni->slots, &slot->link);
	} else {
		sway_log(SWAY_ERROR, "%s%s failed to call method GetLayout: %s",
				menu->sni->service, menu->sni->menu, strerror(-ret));
		free(slot);
	}
}

static void swaybar_dbusmenu_get_layout_root(struct swaybar_dbusmenu *menu) {
	swaybar_dbusmenu_get_layout(menu, 0);
}

static int get_icon_theme_path_callback(sd_bus_message *msg, void *data,
		sd_bus_error *error) {
	struct swaybar_sni_slot *slot = data;
	struct swaybar_sni *sni = slot->sni;
	wl_list_remove(&slot->link);
	free(slot);

	int ret;
	if (!sd_bus_message_is_method_error(msg, NULL)) {
		ret = sd_bus_message_enter_container(msg, 'v', NULL);
		if (ret >= 0) {
			ret = sd_bus_message_read_strv(msg, &sni->menu_icon_theme_paths);
		}
	} else {
		ret = -sd_bus_message_get_errno(msg);
	}

	if (ret < 0) {
		sway_log(SWAY_ERROR, "%s%s failed to read IconThemePath: %s", sni->service,
			sni->menu, strerror(-ret));
	}
	return ret;
}

static void swaybar_dbusmenu_setup(struct swaybar_dbusmenu *menu) {
		struct swaybar_sni_slot *slot = calloc(1, sizeof(struct swaybar_sni_slot));
	slot->sni = menu->sni;
	int ret = sd_bus_call_method_async( menu->sni->tray->bus, &slot->slot, 
			menu->sni->service, menu->sni->path, "org.freedesktop.DBus.Properties",
			"Get", get_icon_theme_path_callback, slot, "ss", menu->sni->interface,
			"IconThemePath");
	if (ret >= 0) {
		wl_list_insert(&menu->sni->slots, &slot->link);
	} else {
		sway_log(SWAY_ERROR, "%s%s failed to get IconThemePath: %s",
			menu->sni->service, menu->sni->menu, strerror(-ret));
		free(slot);
	}

	swaybar_dbusmenu_setup_signals(menu);
	swaybar_dbusmenu_get_layout_root(menu);
}

void swaybar_dbusmenu_open(struct swaybar_sni *sni,
		struct swaybar_output *output, struct swaybar_seat *seat, uint32_t serial, 
		int x, int y) {
	struct swaybar_dbusmenu *dbusmenu = sni->tray->menu;
	if (!dbusmenu) {
		dbusmenu = calloc(1, sizeof(struct swaybar_dbusmenu));
		if (!dbusmenu) {
			sway_log(SWAY_DEBUG, "Could not allocate dbusmenu");
			return;
		}
		sni->tray->menu = dbusmenu;
	}

	dbusmenu->sni = sni;
	dbusmenu->output = output;
	dbusmenu->seat = seat;
	dbusmenu->serial = serial;
	dbusmenu->x = seat->pointer.x;
	dbusmenu->y = seat->pointer.y;
	dbusmenu->bar = output->bar;

	swaybar_dbusmenu_setup(dbusmenu);
}

static void close_child_menus_except(struct swaybar_dbusmenu_menu *menu,
		int id) {
	if (!menu || !menu->items) {
		return;
	}
	// close all child menus of menu, except the child menu with the given id
	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
		if (item->id == id) {
			continue;
		}
		struct swaybar_dbusmenu_menu *menu = item->submenu;
		if (menu && menu->item_id != 0) {
			close_menus(menu);
		}
	}
}

static void
pointer_motion_process_item(struct swaybar_dbusmenu_menu *focused_menu,
		struct swaybar_dbusmenu_menu_item *item, struct swaybar_seat *seat) {
	int scale = focused_menu->dbusmenu->output->scale;
	double x = seat->pointer.x * scale;
	double y = seat->pointer.y * scale;
	if (is_in_hotspot(&item->hotspot, x, y) && item->enabled &&
			!item->is_separator) {
		struct swaybar_tray *tray = focused_menu->dbusmenu->sni->tray;
		struct swaybar_sni *sni = tray->menu->sni;
		if (focused_menu->last_hovered_item != item) {
			sd_bus_call_method_async(tray->bus, NULL, sni->service, sni->menu, 
				menu_interface, "Event", NULL, NULL, "isvu", item->id, "hovered",
				"y", 0, time(NULL));

			sway_log(SWAY_DEBUG, "%s%s hovered id %d", sni->service, sni->menu,
				item->id);

			// open child menu if current item has a child menu and close other 
			// potential open child menus. Only one child menu can be open at a time
			close_child_menus_except(focused_menu, item->id);
			open_menu_id(focused_menu->dbusmenu, item->id);
			focused_menu->last_hovered_item = item;

			// a different item needs to be highlighted
			swaybar_dbusmenu_draw_menu(focused_menu, focused_menu->item_id, false);
		}

	}
}

bool dbusmenu_pointer_motion(struct swaybar_seat *seat, 
		struct wl_pointer *wl_pointer, uint32_t time_, wl_fixed_t surface_x,
		wl_fixed_t surface_y) {
	struct swaybar_tray *tray = seat->bar->tray;
	struct swaybar_dbusmenu_menu *focused_menu = tray->menu_pointer_focus;
	if (!(tray && tray->menu && focused_menu)) {
		return false;
	}

	for (int i = 0; i < focused_menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = focused_menu->items->items[i];
		pointer_motion_process_item(focused_menu, item, seat);
	}

	return true;
}

static struct swaybar_dbusmenu_menu *
dbusmenu_menu_find_menu_surface(struct swaybar_dbusmenu_menu *menu,
		struct wl_surface *surface) {
	if (menu->surface && menu->surface->surface == surface) {
		return menu;
	}
	if (!menu->items) {
		return NULL;
	}
	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
		struct swaybar_dbusmenu_menu *child_menu = item->submenu;
		if (child_menu && child_menu->surface 
				&& child_menu->surface->surface == surface) {
			return child_menu;
		}
		if (child_menu) {
			if (child_menu->item_id == 0) {
				continue;
			}
			struct swaybar_dbusmenu_menu *child_child_menu =
					dbusmenu_menu_find_menu_surface(child_menu, surface);
			if (child_child_menu != NULL) {
				return child_child_menu;
			}
		}
	}

	return NULL;
}

static void close_menus_by_id(struct swaybar_dbusmenu_menu *menu, int item_id) {
	if (menu->items == NULL) {
		return;
	}
	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];
		struct swaybar_dbusmenu_menu *child_menu = item->submenu;
		if (child_menu && child_menu->item_id == item_id && child_menu->item_id != 0) {
			close_menus(child_menu);
		}
	}
}

static void close_unfocused_child_menus(struct swaybar_dbusmenu_menu *menu,
		struct swaybar_seat *seat) {
	for (int i = 0; i < menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = menu->items->items[i];

		int scale = menu->dbusmenu->output->scale;
		int x = seat->pointer.x * scale;
		int y = seat->pointer.y * scale;
		if (item->submenu && item->submenu->item_id != 0 
				&& !is_in_hotspot(&item->hotspot, x, y)) {
			close_menus_by_id(menu, item->id);
		}
	}
}

bool dbusmenu_pointer_frame(struct swaybar_seat *data, 
		struct wl_pointer *wl_pointer) {
	struct swaybar_tray *tray = data->bar->tray;
	if (!(tray && tray->menu && tray->menu_pointer_focus)) {
		return false;
	}
	return true;
}

bool dbusmenu_pointer_axis(struct swaybar_seat *data,
		struct wl_pointer *wl_pointer) {
	struct swaybar_tray *tray = data->bar->tray;
	if (!(tray && tray->menu && tray->menu_pointer_focus)) {
		return false;
	}
	return true;
}

bool dbusmenu_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x,
		wl_fixed_t surface_y) {
	struct swaybar_seat *seat = data;
	struct swaybar_tray *tray = seat->bar->tray;
	if (!(tray && tray->menu)) {
		return false;
	}

	struct swaybar_dbusmenu_menu *new_focused_menu =
	dbusmenu_menu_find_menu_surface(tray->menu->menu, surface);

	// Check if there are any child menus
	bool has_child_menus = false;
	if (new_focused_menu && new_focused_menu->items) {
		for (int i = 0; i < new_focused_menu->items->length; ++i) {
			struct swaybar_dbusmenu_menu_item *item = new_focused_menu->items->items[i];
			if (item->submenu && item->submenu->item_id != 0) {
				has_child_menus = true;
			}
		}
	}

	if (has_child_menus) {
		close_unfocused_child_menus(new_focused_menu, seat);
	}

	tray->menu_pointer_focus = new_focused_menu;

	return true;
}

bool dbusmenu_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	struct swaybar_seat *seat = data;
	struct swaybar_tray *tray = seat->bar->tray;
	if (!(tray && tray->menu)) {
		return false;
	}

	tray->menu_pointer_focus = NULL;

	return true;
}

static bool dbusmenu_pointer_button_left_process_item(struct swaybar_dbusmenu *dbusmenu,
		struct swaybar_dbusmenu_menu_item *item, struct swaybar_seat *seat) {
	struct swaybar_sni *sni = dbusmenu->sni;
	struct swaybar_tray *tray = sni->tray;
	int scale = dbusmenu->output->scale;

	if (is_in_hotspot(&item->hotspot, seat->pointer.x * scale,
			seat->pointer.y * scale)) {
		if (!item->enabled || item->is_separator) {
			return false;
		}

		sway_log(SWAY_DEBUG, "%s%s menu clicked id %d", sni->service, sni->menu,
				item->id);

		sd_bus_call_method_async(tray->bus, NULL, sni->service, sni->menu,
				menu_interface, "Event", NULL, NULL, "isvu", item->id, "clicked", "y", 0, 
				time(NULL));

		if (item->submenu) {
			open_menu_id(dbusmenu, item->id);
		} else {
			// The user clicked an menu item other than a submenu. That means 
			// the user made it's choise. Close the tray menu.
			swaybar_dbusmenu_destroy(tray->menu);
		}
		return true;
	}

	return false;
}

static bool dbusmenu_pointer_button_left(struct swaybar_dbusmenu *dbusmenu,
		struct swaybar_seat *seat) {
	struct swaybar_dbusmenu_menu *focused_menu 
		= dbusmenu->sni->tray->menu_pointer_focus;

	if (!focused_menu) {
		return true;
	}

	for (int i = 0; i < focused_menu->items->length; ++i) {
		struct swaybar_dbusmenu_menu_item *item = focused_menu->items->items[i];
		if (dbusmenu_pointer_button_left_process_item(dbusmenu, item, seat)) {
			return true;
		}
	}

	return true;
}

bool dbusmenu_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time_, uint32_t button, uint32_t state) {
	struct swaybar_seat *seat = data;
	struct swaybar_tray *tray = seat->bar->tray;
	if (!(tray && tray->menu)) {
		return false;
	}

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		// intentionally left blank
		return true;
	} else if (!tray->menu_pointer_focus) {
		swaybar_dbusmenu_destroy(tray->menu);
		return true;
	} else if (button == BTN_LEFT) {
		return dbusmenu_pointer_button_left(tray->menu, seat);
	}

	return false;
}
