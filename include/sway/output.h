#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include "config.h"
#include "sway/tree/view.h"

struct sway_server;
struct sway_container;

struct sway_output {
	struct wlr_output *wlr_output;
	struct sway_container *swayc;
	struct sway_server *server;

	struct wl_list layers[4]; // sway_layer_surface::link
	struct wlr_box usable_area;

	struct timespec last_frame;
	struct wlr_output_damage *damage;

	struct wl_listener destroy;
	struct wl_listener mode;
	struct wl_listener transform;
	struct wl_listener scale;

	struct wl_listener damage_destroy;
	struct wl_listener damage_frame;

	struct wl_list link;

	pid_t bg_pid;

	struct {
		struct wl_signal destroy;
	} events;
};

/**
 * Contains a surface's root geometry information. For instance, when rendering
 * a popup, this will contain the parent view's position and size.
 */
struct root_geometry {
	double x, y;
	int width, height;
	float rotation;
};

typedef void (*sway_surface_iterator_func_t)(struct sway_output *output,
	struct wlr_surface *surface, struct wlr_box *box, float rotation,
	void *user_data);

void output_damage_whole(struct sway_output *output);

void output_damage_surface(struct sway_output *output, double ox, double oy,
	struct wlr_surface *surface, bool whole);

void output_damage_from_view(struct sway_output *output,
	struct sway_view *view);

void output_damage_box(struct sway_output *output, struct wlr_box *box);

void output_damage_whole_container(struct sway_output *output,
	struct sway_container *con);

struct sway_container *output_by_name(const char *name);

void output_enable(struct sway_output *output);

bool output_has_opaque_overlay_layer_surface(struct sway_output *output);

struct sway_container *output_get_active_workspace(struct sway_output *output);

void output_render(struct sway_output *output, struct timespec *when,
	pixman_region32_t *damage);

bool output_get_surface_box(struct root_geometry *geo,
	struct sway_output *output, struct wlr_surface *surface, int sx, int sy,
	struct wlr_box *surface_box);

void output_surface_for_each_surface(struct wlr_surface *surface,
	double ox, double oy, struct root_geometry *geo,
	wlr_surface_iterator_func_t iterator, void *user_data);

void output_surface_for_each_surface2(struct sway_output *output,
	struct wlr_surface *surface, double ox, double oy, float rotation,
	sway_surface_iterator_func_t iterator, void *user_data);

void output_view_for_each_surface(struct sway_view *view,
	struct sway_output *output, struct root_geometry *geo,
	wlr_surface_iterator_func_t iterator, void *user_data);

void output_layer_for_each_surface(struct wl_list *layer_surfaces,
	struct root_geometry *geo, wlr_surface_iterator_func_t iterator,
	void *user_data);

#ifdef HAVE_XWAYLAND
void output_unmanaged_for_each_surface(struct sway_output *output,
	struct wl_list *unmanaged, sway_surface_iterator_func_t iterator,
	void *user_data);
#endif

void output_drag_icons_for_each_surface(struct sway_output *output,
	struct wl_list *drag_icons, sway_surface_iterator_func_t iterator,
	void *user_data);

#endif
