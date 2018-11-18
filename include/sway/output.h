#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include "config.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"

struct sway_server;
struct sway_container;

struct sway_output_state {
	list_t *workspaces;
	struct sway_workspace *active_workspace;
};

struct sway_output {
	struct sway_node node;
	struct wlr_output *wlr_output;
	struct sway_server *server;

	struct wl_list layers[4]; // sway_layer_surface::link
	struct wlr_box usable_area;

	struct timespec last_frame;
	struct wlr_output_damage *damage;

	int lx, ly;
	int width, height;

	bool enabled;
	list_t *workspaces;

	struct sway_output_state current;

	struct wl_listener destroy;
	struct wl_listener mode;
	struct wl_listener transform;
	struct wl_listener scale;
	struct wl_listener present;
	struct wl_listener damage_destroy;
	struct wl_listener damage_frame;

	struct wl_list link;

	pid_t bg_pid;

	struct {
		struct wl_signal destroy;
	} events;
};

struct sway_output *output_create(struct wlr_output *wlr_output);

void output_destroy(struct sway_output *output);

void output_begin_destroy(struct sway_output *output);

struct sway_output *output_from_wlr_output(struct wlr_output *output);

struct sway_output *output_get_in_direction(struct sway_output *reference,
		enum wlr_direction direction);

void output_add_workspace(struct sway_output *output,
		struct sway_workspace *workspace);

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

struct sway_output *output_by_name(const char *name);

struct sway_output *output_by_identifier(const char *identifier);

void output_sort_workspaces(struct sway_output *output);

struct output_config *output_find_config(struct sway_output *output);

void output_enable(struct sway_output *output, struct output_config *oc);

void output_disable(struct sway_output *output);

bool output_has_opaque_overlay_layer_surface(struct sway_output *output);

struct sway_workspace *output_get_active_workspace(struct sway_output *output);

void output_render(struct sway_output *output, struct timespec *when,
	pixman_region32_t *damage);

void output_surface_for_each_surface(struct sway_output *output,
		struct wlr_surface *surface, double ox, double oy,
		sway_surface_iterator_func_t iterator, void *user_data);

void output_view_for_each_surface(struct sway_output *output,
	struct sway_view *view, sway_surface_iterator_func_t iterator,
	void *user_data);

void output_view_for_each_popup(struct sway_output *output,
		struct sway_view *view, sway_surface_iterator_func_t iterator,
		void *user_data);

void output_layer_for_each_surface(struct sway_output *output,
	struct wl_list *layer_surfaces, sway_surface_iterator_func_t iterator,
	void *user_data);

#if HAVE_XWAYLAND
void output_unmanaged_for_each_surface(struct sway_output *output,
	struct wl_list *unmanaged, sway_surface_iterator_func_t iterator,
	void *user_data);
#endif

void output_drag_icons_for_each_surface(struct sway_output *output,
	struct wl_list *drag_icons, sway_surface_iterator_func_t iterator,
	void *user_data);

void output_for_each_workspace(struct sway_output *output,
		void (*f)(struct sway_workspace *ws, void *data), void *data);

void output_for_each_container(struct sway_output *output,
		void (*f)(struct sway_container *con, void *data), void *data);

struct sway_workspace *output_find_workspace(struct sway_output *output,
		bool (*test)(struct sway_workspace *ws, void *data), void *data);

struct sway_container *output_find_container(struct sway_output *output,
		bool (*test)(struct sway_container *con, void *data), void *data);

void output_get_box(struct sway_output *output, struct wlr_box *box);

enum sway_container_layout output_get_default_layout(
		struct sway_output *output);

void output_add_listeners(struct sway_output *output);

#endif
