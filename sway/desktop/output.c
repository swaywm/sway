#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/render.h>
#include <wlr/render/matrix.h>
#include "log.h"
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/view.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"

static void output_frame_view(swayc_t *view, void *data) {
	struct sway_output *output = data;
	struct wlr_output *wlr_output = output->wlr_output;
	struct sway_view *sway_view = view->sway_view;
	struct wlr_surface *surface = sway_view->surface;
	if (!wlr_surface_has_buffer(surface)) {
		return;
	}
	// TODO
	// - Deal with wlr_output_layout
	int width = sway_view->surface->current->width;
	int height = sway_view->surface->current->height;
	int render_width = width * wlr_output->scale;
	int render_height = height * wlr_output->scale;
	double ox = view->x, oy = view->y;
	// TODO
	//wlr_output_layout_output_coords(desktop->layout, wlr_output, &ox, &oy);
	ox *= wlr_output->scale;
	oy *= wlr_output->scale;
	// TODO
	//if (wlr_output_layout_intersects(desktop->layout, wlr_output,
	//		lx, ly, lx + render_width, ly + render_height)) {
	//		return;
	//}

	// if the shell specifies window geometry, make the top left corner of the
	// window in the top left corner of the container to avoid arbitrarily
	// sized gaps based on the attached buffer size
	int window_offset_x = 0;
	int window_offset_y = 0;

	if (view->sway_view->type == SWAY_XDG_SHELL_V6_VIEW) {
		window_offset_x = view->sway_view->wlr_xdg_surface_v6->geometry->x;
		window_offset_y = view->sway_view->wlr_xdg_surface_v6->geometry->y;
	}

	// TODO
	double rotation = 0;
	float matrix[16];

	float translate_origin[16];
	wlr_matrix_translate(&translate_origin,
		(int)ox + render_width / 2 - window_offset_x,
		(int)oy + render_height / 2 - window_offset_y,
		0);

	float rotate[16];
	wlr_matrix_rotate(&rotate, rotation);

	float translate_center[16];
	wlr_matrix_translate(&translate_center,
		-render_width / 2,
		-render_height / 2, 0);

	float scale[16];
	wlr_matrix_scale(&scale, render_width, render_height, 1);

	float transform[16];
	wlr_matrix_mul(&translate_origin, &rotate, &transform);
	wlr_matrix_mul(&transform, &translate_center, &transform);
	wlr_matrix_mul(&transform, &scale, &transform);
	wlr_matrix_mul(&wlr_output->transform_matrix, &transform, &matrix);

	wlr_render_with_matrix(output->server->renderer, surface->texture, &matrix);

	// TODO: move into wlroots
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	wlr_surface_send_frame_done(surface, &now);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
	struct sway_output *soutput = wl_container_of(listener, soutput, frame);
	struct wlr_output *wlr_output = data;
	struct sway_server *server = soutput->server;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	wlr_output_make_current(wlr_output);
	wlr_renderer_begin(server->renderer, wlr_output);

	swayc_descendants_of_type(
			&root_container, C_VIEW, output_frame_view, soutput);

	wlr_renderer_end(server->renderer);
	wlr_output_swap_buffers(wlr_output);

	soutput->last_frame = now;
}

void output_add_notify(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, output_add);
	struct wlr_output *wlr_output = data;
	wlr_log(L_DEBUG, "New output %p: %s", wlr_output, wlr_output->name);

	struct sway_output *output = calloc(1, sizeof(struct sway_output));
	if (!output) {
		return;
	}
	output->wlr_output = wlr_output;
	output->server = server;

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	output->swayc = new_output(output);
	if (!output->swayc) {
		free(output);
		return;
	}

	sway_input_manager_configure_xcursor(input_manager);

	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
}

void output_remove_notify(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, output_remove);
	struct wlr_output *wlr_output = data;
	wlr_log(L_DEBUG, "Output %p %s removed", wlr_output, wlr_output->name);

	swayc_t *output_container = NULL;
	for (int i = 0 ; i < root_container.children->length; ++i) {
		swayc_t *child = root_container.children->items[i];
		if (child->type == C_OUTPUT &&
				child->sway_output->wlr_output == wlr_output) {
			output_container = child;
			break;
		}
	}
	if (!output_container) {
		return;
	}

	destroy_output(output_container);
}
