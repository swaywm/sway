#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
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

	struct {
		struct wlr_scene_tree *tiling;
		struct wlr_scene_tree *fullscreen;
	} layers;

	// when a container is fullscreen, in case the fullscreen surface is
	// translucent (can see behind) we must make sure that the background is a
	// solid color in order to conform to the wayland protocol. This rect
	// ensures that when looking through a surface, all that will be seen
	// is black.
	struct wlr_scene_rect *fullscreen_background;

	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct sway_server *server;
	struct wl_list link;

	struct wl_list shell_layers[4]; // sway_layer_surface::link
	struct wlr_box usable_area;

	struct timespec last_frame;
	struct wlr_damage_ring damage_ring;

	int lx, ly; // layout coords
	int width, height; // transformed buffer size
	enum wl_output_subpixel detected_subpixel;
	enum scale_filter_mode scale_filter;

	bool enabling, enabled;
	list_t *workspaces;

	struct sway_output_state current;

	struct wl_listener layout_destroy;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener present;
	struct wl_listener damage;
	struct wl_listener frame;
	struct wl_listener needs_frame;
	struct wl_listener request_state;

	struct {
		struct wl_signal disable;
	} events;

	struct timespec last_presentation;
	uint32_t refresh_nsec;
	int max_render_time; // In milliseconds
	struct wl_event_source *repaint_timer;
	bool gamma_lut_changed;
};

struct sway_output_non_desktop {
	struct wlr_output *wlr_output;

	struct wl_listener destroy;
};

struct render_context {
	struct sway_output *output;
	struct wlr_renderer *renderer;
	const pixman_region32_t *output_damage;

	struct wlr_render_pass *pass;
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
	struct sway_view *view, struct wlr_surface *surface, struct wlr_box *box,
	void *user_data);

void output_damage_whole(struct sway_output *output);

void output_damage_surface(struct sway_output *output, double ox, double oy,
	struct wlr_surface *surface, bool whole);

void output_damage_from_view(struct sway_output *output,
	struct sway_view *view);

void output_damage_box(struct sway_output *output, struct wlr_box *box);

void output_damage_whole_container(struct sway_output *output,
	struct sway_container *con);

bool output_match_name_or_id(struct sway_output *output,
	const char *name_or_id);

// this ONLY includes the enabled outputs
struct sway_output *output_by_name_or_id(const char *name_or_id);

// this includes all the outputs, including disabled ones
struct sway_output *all_output_by_name_or_id(const char *name_or_id);

void output_sort_workspaces(struct sway_output *output);

void output_enable(struct sway_output *output);

void output_disable(struct sway_output *output);

bool output_has_opaque_overlay_layer_surface(struct sway_output *output);

struct sway_workspace *output_get_active_workspace(struct sway_output *output);

void output_render(struct render_context *ctx);

void output_surface_for_each_surface(struct sway_output *output,
		struct wlr_surface *surface, double ox, double oy,
		sway_surface_iterator_func_t iterator, void *user_data);

void output_view_for_each_surface(struct sway_output *output,
	struct sway_view *view, sway_surface_iterator_func_t iterator,
	void *user_data);

void output_view_for_each_popup_surface(struct sway_output *output,
		struct sway_view *view, sway_surface_iterator_func_t iterator,
		void *user_data);

void output_layer_for_each_surface(struct sway_output *output,
	struct wl_list *layer_surfaces, sway_surface_iterator_func_t iterator,
	void *user_data);

void output_layer_for_each_toplevel_surface(struct sway_output *output,
	struct wl_list *layer_surfaces, sway_surface_iterator_func_t iterator,
	void *user_data);

void output_layer_for_each_popup_surface(struct sway_output *output,
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

void render_rect(struct render_context *ctx, const struct wlr_box *_box,
		float color[static 4]);

void premultiply_alpha(float color[4], float opacity);

void scale_box(struct wlr_box *box, float scale);

enum wlr_direction opposite_direction(enum wlr_direction d);

void handle_output_layout_change(struct wl_listener *listener, void *data);

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data);

void handle_output_manager_apply(struct wl_listener *listener, void *data);

void handle_output_manager_test(struct wl_listener *listener, void *data);

void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);

struct sway_output_non_desktop *output_non_desktop_create(struct wlr_output *wlr_output);

#endif
