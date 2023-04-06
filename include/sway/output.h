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
		struct wlr_scene_tree *shell_background;
		struct wlr_scene_tree *shell_bottom;
		struct wlr_scene_tree *tiling;
		struct wlr_scene_tree *fullscreen;
		struct wlr_scene_tree *shell_top;
		struct wlr_scene_tree *shell_overlay;
		struct wlr_scene_tree *session_lock;
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

	struct wlr_box usable_area;

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
	struct wl_listener frame;
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

bool output_match_name_or_id(struct sway_output *output,
	const char *name_or_id);

// this ONLY includes the enabled outputs
struct sway_output *output_by_name_or_id(const char *name_or_id);

// this includes all the outputs, including disabled ones
struct sway_output *all_output_by_name_or_id(const char *name_or_id);

void output_sort_workspaces(struct sway_output *output);

void output_enable(struct sway_output *output);

void output_disable(struct sway_output *output);

struct sway_workspace *output_get_active_workspace(struct sway_output *output);

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

enum wlr_direction opposite_direction(enum wlr_direction d);

void handle_output_layout_change(struct wl_listener *listener, void *data);

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data);

void handle_output_manager_apply(struct wl_listener *listener, void *data);

void handle_output_manager_test(struct wl_listener *listener, void *data);

void handle_output_power_manager_set_mode(struct wl_listener *listener,
	void *data);

struct sway_output_non_desktop *output_non_desktop_create(struct wlr_output *wlr_output);

#endif
