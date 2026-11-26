#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/box.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include "config.h"
#include "log.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

#if WLR_HAS_DRM_BACKEND
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#endif

bool output_match_name_or_id(struct sway_output *output,
		const char *name_or_id) {
	if (strcmp(name_or_id, "*") == 0) {
		return true;
	}

	char identifier[128];
	output_get_identifier(identifier, sizeof(identifier), output);
	return strcasecmp(identifier, name_or_id) == 0
		|| strcasecmp(output->wlr_output->name, name_or_id) == 0;
}

struct sway_output *output_by_name_or_id(const char *name_or_id) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (output_match_name_or_id(output, name_or_id)) {
			return output;
		}
	}
	return NULL;
}

struct sway_output *all_output_by_name_or_id(const char *name_or_id) {
	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output_match_name_or_id(output, name_or_id)) {
			return output;
		}
	}
	return NULL;
}


struct sway_workspace *output_get_active_workspace(struct sway_output *output) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_node *focus = seat_get_active_tiling_child(seat, &output->node);
	if (!focus) {
		if (!output->workspaces->length) {
			return NULL;
		}
		return output->workspaces->items[0];
	}
	return focus->sway_workspace;
}

struct send_frame_done_data {
	struct timespec when;
	int msec_until_refresh;
	struct sway_output *output;
};

struct buffer_timer {
	struct wl_listener destroy;
	struct wl_event_source *frame_done_timer;
};

static int handle_buffer_timer(void *data) {
	struct wlr_scene_surface *scene_surface = data;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_surface_send_frame_done(scene_surface, &now);
	return 0;
}

static void handle_buffer_timer_destroy(struct wl_listener *listener,
		void *data) {
	struct buffer_timer *timer = wl_container_of(listener, timer, destroy);

	wl_list_remove(&timer->destroy.link);
	wl_event_source_remove(timer->frame_done_timer);
	free(timer);
}

static struct buffer_timer *buffer_timer_get_or_create(struct wlr_scene_surface *scene_surface) {
	struct wlr_scene_buffer *buffer = scene_surface->buffer;

	struct buffer_timer *timer =
		scene_descriptor_try_get(&buffer->node, SWAY_SCENE_DESC_BUFFER_TIMER);
	if (timer) {
		return timer;
	}

	timer = calloc(1, sizeof(struct buffer_timer));
	if (!timer) {
		return NULL;
	}

	timer->frame_done_timer = wl_event_loop_add_timer(server.wl_event_loop,
		handle_buffer_timer, scene_surface);
	if (!timer->frame_done_timer) {
		free(timer);
		return NULL;
	}

	scene_descriptor_assign(&buffer->node, SWAY_SCENE_DESC_BUFFER_TIMER, timer);

	timer->destroy.notify = handle_buffer_timer_destroy;
	wl_signal_add(&buffer->node.events.destroy, &timer->destroy);

	return timer;
}

static void send_frame_done_iterator(struct wlr_scene_buffer *buffer,
		int x, int y, void *user_data) {
	struct send_frame_done_data *data = user_data;
	struct sway_output *output = data->output;
	int view_max_render_time = 0;

	if (buffer->primary_output != data->output->scene_output) {
		return;
	}

	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);
	if (scene_surface == NULL) {
		return;
	}

	struct wlr_scene_node *current = &buffer->node;
	while (true) {
		struct sway_view *view = scene_descriptor_try_get(current,
			SWAY_SCENE_DESC_VIEW);
		if (view) {
			view_max_render_time = view->max_render_time;
			break;
		}

		if (!current->parent) {
			break;
		}

		current = &current->parent->node;
	}

	int delay = data->msec_until_refresh - output->max_render_time
			- view_max_render_time;

	struct buffer_timer *timer = NULL;

	if (output->max_render_time != 0 && view_max_render_time != 0 && delay > 0) {
		timer = buffer_timer_get_or_create(scene_surface);
	}

	if (timer) {
		wl_event_source_timer_update(timer->frame_done_timer, delay);
	} else {
		wlr_scene_surface_send_frame_done(scene_surface, &data->when);
	}
}

static enum wlr_scale_filter_mode get_scale_filter(struct sway_output *output,
		struct wlr_scene_buffer *buffer) {
	// if we are scaling down, we should always choose linear
	if (buffer->dst_width > 0 && buffer->dst_height > 0 && (
			buffer->dst_width < buffer->WLR_PRIVATE.buffer_width ||
			buffer->dst_height < buffer->WLR_PRIVATE.buffer_height)) {
		return WLR_SCALE_FILTER_BILINEAR;
	}

	switch (output->scale_filter) {
	case SCALE_FILTER_LINEAR:
		return WLR_SCALE_FILTER_BILINEAR;
	case SCALE_FILTER_NEAREST:
		return WLR_SCALE_FILTER_NEAREST;
	default:
		abort(); // unreachable
	}
}

void output_configure_scene(struct sway_output *output,
		struct wlr_scene_node *node, float opacity) {
	if (!node->enabled) {
		return;
	}

	struct sway_container *con =
		scene_descriptor_try_get(node, SWAY_SCENE_DESC_CONTAINER);
	if (con) {
		opacity = con->alpha;
	}

	if (node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(buffer);

		if (surface) {
			const struct wlr_alpha_modifier_surface_v1_state *alpha_modifier_state =
				wlr_alpha_modifier_v1_get_surface_state(surface->surface);
			if (alpha_modifier_state != NULL) {
				opacity *= (float)alpha_modifier_state->multiplier;
			}
		}

		// hack: don't call the scene setter because that will damage all outputs
		// We don't want to damage outputs that aren't our current output that
		// we're configuring
		if (output) {
			buffer->filter_mode = get_scale_filter(output, buffer);
		}

		wlr_scene_buffer_set_opacity(buffer, opacity);
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *node;
		wl_list_for_each(node, &tree->children, link) {
			output_configure_scene(output, node, opacity);
		}
	}
}

static bool output_can_tear(struct sway_output *output) {
	struct sway_workspace *workspace = output->current.active_workspace;
	if (!workspace) {
		return false;
	}

	struct sway_container *fullscreen_con = root->fullscreen_global;
	if (!fullscreen_con) {
		fullscreen_con = workspace->current.fullscreen;
	}
	if (fullscreen_con && fullscreen_con->view) {
		return (output->allow_tearing && view_can_tear(fullscreen_con->view));
	}

	return false;
}

static int output_repaint_timer_handler(void *data) {
	struct sway_output *output = data;

	output->wlr_output->frame_pending = false;
	if (!output->wlr_output->enabled || !output->scene_output) {
		// The output may have started mirroring since the timer was armed
		return 0;
	}

	output_configure_scene(output, &root->root_scene->tree.node, 1.0f);

	struct wlr_scene_output_state_options opts = {
		.color_transform = output->color_transform,
	};

	struct wlr_scene_output *scene_output = output->scene_output;
	if (!wlr_scene_output_needs_frame(scene_output)) {
		return 0;
	}

	struct wlr_output_state pending;
	wlr_output_state_init(&pending);
	if (!wlr_scene_output_build_state(output->scene_output, &pending, &opts)) {
		wlr_output_state_finish(&pending);
		return 0;
	}

	if (output_can_tear(output)) {
		pending.tearing_page_flip = true;

		if (!wlr_output_test_state(output->wlr_output, &pending)) {
			sway_log(SWAY_DEBUG, "Output test failed on '%s', retrying without tearing page-flip",
				output->wlr_output->name);
			pending.tearing_page_flip = false;
		}
	}

	if (!wlr_output_commit_state(output->wlr_output, &pending)) {
		sway_log(SWAY_ERROR, "Page-flip failed on output %s", output->wlr_output->name);
	}
	wlr_output_state_finish(&pending);
	return 0;
}

bool output_mirror_build_state(struct sway_output *output,
		struct sway_output *source, struct wlr_output_state *state,
		struct wlr_swapchain *swapchain,
		struct wlr_color_transform *color_transform) {
	struct wlr_output *wlr_output = output->wlr_output;
	bool enabled = wlr_output->enabled;
	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		enabled = state->enabled;
	}
	if (!enabled) {
		return true;
	}

	if (!swapchain) {
		if (!wlr_output_configure_primary_swapchain(wlr_output, state,
				&wlr_output->swapchain)) {
			return false;
		}
		swapchain = wlr_output->swapchain;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
	if (!buffer) {
		return false;
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		wlr_output->renderer, buffer, &(struct wlr_buffer_pass_options){
			.color_transform = color_transform,
		});
	if (!pass) {
		wlr_buffer_unlock(buffer);
		return false;
	}

	// Black wherever the picture doesn't reach
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = { .width = buffer->width, .height = buffer->height },
		.color = { .r = 0, .g = 0, .b = 0, .a = 1 },
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});

	// The stored frame is only valid for the source it came from
	struct wlr_texture *texture = NULL;
	if (output->mirror_source == source && output->mirror.buffer != NULL) {
		texture = wlr_texture_from_buffer(wlr_output->renderer,
			output->mirror.buffer);
	}

	if (texture != NULL && output->mirror.width > 0 &&
			output->mirror.height > 0) {
		enum wl_output_transform transform = wlr_output->transform;
		if (state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
			transform = state->transform;
		}

		// Fit the source's logical size into the mirror's transformed size
		// with the same factor on both axes, and centre it. Output scales
		// don't change the picture, only the aspect ratios matter.
		int width = buffer->width, height = buffer->height;
		if (transform & WL_OUTPUT_TRANSFORM_90) {
			width = buffer->height;
			height = buffer->width;
		}
		double fit = fmin((double)width / output->mirror.width,
			(double)height / output->mirror.height);
		struct wlr_box box = {
			.width = fmin(round(output->mirror.width * fit), width),
			.height = fmin(round(output->mirror.height * fit), height),
		};
		box.x = (width - box.width) / 2;
		box.y = (height - box.height) / 2;

		struct wlr_box dst_box;
		wlr_box_transform(&dst_box, &box,
			wlr_output_transform_invert(transform), width, height);

		// The source's buffer holds its content with the source's transform
		// applied: undo it, then apply the mirror's
		struct wlr_render_texture_options options = {
			.texture = texture,
			.dst_box = dst_box,
			.transform = wlr_output_transform_compose(
				wlr_output_transform_invert(output->mirror.transform),
				transform),
			.filter_mode = WLR_SCALE_FILTER_BILINEAR,
			.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
		};
		if (wlr_output->renderer->features.timeline) {
			options.wait_timeline = output->mirror.wait_timeline;
			options.wait_point = output->mirror.wait_point;
		}
		wlr_render_pass_add_texture(pass, &options);
	}

	bool ok = wlr_render_pass_submit(pass);
	wlr_texture_destroy(texture);
	if (ok) {
		wlr_output_state_set_buffer(state, buffer);
	}
	wlr_buffer_unlock(buffer);
	return ok;
}

static void mirror_repaint(struct sway_output *output) {
	output->mirror.needs_frame = false;

	struct wlr_output_state pending;
	wlr_output_state_init(&pending);
	if (output_mirror_build_state(output, output->mirror_source, &pending,
			NULL, output->color_transform)) {
		if (!wlr_output_commit_state(output->wlr_output, &pending)) {
			sway_log(SWAY_ERROR, "Page-flip failed on mirror output %s",
				output->wlr_output->name);
		}
	}
	wlr_output_state_finish(&pending);
}

static void mirror_clear_frame(struct sway_output *output) {
	wlr_buffer_unlock(output->mirror.buffer);
	output->mirror.buffer = NULL;
	if (output->mirror.wait_timeline != NULL) {
		wlr_drm_syncobj_timeline_unref(output->mirror.wait_timeline);
		output->mirror.wait_timeline = NULL;
	}
}

static void handle_mirror_source_commit(struct wl_listener *listener,
		void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, mirror.source_commit);
	struct wlr_output_event_commit *event = data;
	const struct wlr_output_state *state = event->state;
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
		// The source shows nothing while it is powered off, and neither
		// does the mirror
		mirror_clear_frame(output);
		output->mirror.needs_frame = true;
		wlr_output_schedule_frame(output->wlr_output);
		return;
	}
	if (!(state->committed & WLR_OUTPUT_STATE_BUFFER) || !state->buffer) {
		return;
	}

	// Keep the source's latest frame. The source's swapchain doesn't reuse
	// the buffer while it is locked.
	mirror_clear_frame(output);
	output->mirror.buffer = wlr_buffer_lock(state->buffer);
	if ((state->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) &&
			state->wait_timeline != NULL) {
		output->mirror.wait_timeline =
			wlr_drm_syncobj_timeline_ref(state->wait_timeline);
		output->mirror.wait_point = state->wait_point;
	}
	output->mirror.transform = event->output->transform;
	wlr_output_effective_resolution(event->output, &output->mirror.width,
		&output->mirror.height);

	output->mirror.needs_frame = true;
	wlr_output_schedule_frame(output->wlr_output);
}

void output_start_mirroring(struct sway_output *output,
		struct sway_output *source) {
	// A mirror shows its source's frames, not the scene
	wlr_scene_output_destroy(output->scene_output);
	output->scene_output = NULL;

	if (output->mirror_source == source) {
		return;
	}
	output_stop_mirroring(output);

	output->mirror_source = source;
	output->mirror.source_commit.notify = handle_mirror_source_commit;
	wl_signal_add(&source->wlr_output->events.commit,
		&output->mirror.source_commit);

	// Every frame of the source has to be composited by the renderer, with
	// the cursor in it: a buffer scanned out directly may be a client buffer
	// that doesn't cover the whole output, and hardware cursors aren't part
	// of the buffer.
	wlr_output_lock_attach_render(source->wlr_output, true);
	wlr_output_lock_software_cursors(source->wlr_output, true);

	// Ask the source for a new frame, which the mirror then shows
	wlr_output_update_needs_frame(source->wlr_output);
	output->mirror.needs_frame = true;
	wlr_output_schedule_frame(output->wlr_output);
}

void output_stop_mirroring(struct sway_output *output) {
	struct sway_output *source = output->mirror_source;
	if (!source) {
		return;
	}
	wlr_output_lock_attach_render(source->wlr_output, false);
	wlr_output_lock_software_cursors(source->wlr_output, false);
	wl_list_remove(&output->mirror.source_commit.link);
	mirror_clear_frame(output);
	output->mirror_source = NULL;
	output->mirror.needs_frame = false;
}

static void handle_frame(struct wl_listener *listener, void *user_data) {
	struct sway_output *output =
		wl_container_of(listener, output, frame);
	if (output->mirror_source) {
		if (output->mirror.needs_frame && output->wlr_output->enabled) {
			mirror_repaint(output);
		}
		return;
	}
	if (!output->enabled || !output->wlr_output->enabled) {
		return;
	}

	// Compute predicted milliseconds until the next refresh. It's used for
	// delaying both output rendering and surface frame callbacks.
	int msec_until_refresh = 0;

	if (output->max_render_time != 0) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		const long NSEC_IN_SECONDS = 1000000000;
		struct timespec predicted_refresh = output->last_presentation;
		predicted_refresh.tv_nsec += output->refresh_nsec % NSEC_IN_SECONDS;
		predicted_refresh.tv_sec += output->refresh_nsec / NSEC_IN_SECONDS;
		if (predicted_refresh.tv_nsec >= NSEC_IN_SECONDS) {
			predicted_refresh.tv_sec += 1;
			predicted_refresh.tv_nsec -= NSEC_IN_SECONDS;
		}

		// If the predicted refresh time is before the current time then
		// there's no point in delaying.
		//
		// We only check tv_sec because if the predicted refresh time is less
		// than a second before the current time, then msec_until_refresh will
		// end up slightly below zero, which will effectively disable the delay
		// without potential disastrous negative overflows that could occur if
		// tv_sec was not checked.
		if (predicted_refresh.tv_sec >= now.tv_sec) {
			long nsec_until_refresh
				= (predicted_refresh.tv_sec - now.tv_sec) * NSEC_IN_SECONDS
					+ (predicted_refresh.tv_nsec - now.tv_nsec);

			// We want msec_until_refresh to be conservative, that is, floored.
			// If we have 7.9 msec until refresh, we better compute the delay
			// as if we had only 7 msec, so that we don't accidentally delay
			// more than necessary and miss a frame.
			msec_until_refresh = nsec_until_refresh / 1000000;
		}
	}

	int delay = msec_until_refresh - output->max_render_time;

	// If the delay is less than 1 millisecond (which is the least we can wait)
	// then just render right away.
	if (delay < 1) {
		output_repaint_timer_handler(output);
	} else {
		output->wlr_output->frame_pending = true;
		wl_event_source_timer_update(output->repaint_timer, delay);
	}

	// Send frame done to all visible surfaces
	struct send_frame_done_data data = {0};
	clock_gettime(CLOCK_MONOTONIC, &data.when);
	data.msec_until_refresh = msec_until_refresh;
	data.output = output;
	wlr_scene_output_for_each_buffer(output->scene_output, send_frame_done_iterator, &data);
}

void update_output_manager_config(struct sway_server *server) {
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output == root->fallback_output) {
			continue;
		}
		struct wlr_output_configuration_head_v1 *config_head =
			wlr_output_configuration_head_v1_create(config, output->wlr_output);
		// A mirror is shown at the position of its source, which is the
		// area of the layout it shows
		struct wlr_output *layout_output = output->mirror_source ?
			output->mirror_source->wlr_output : output->wlr_output;
		struct wlr_box output_box;
		wlr_output_layout_get_box(root->output_layout, layout_output,
			&output_box);
		// We mark the output enabled when it's switched off but not disabled
		config_head->state.enabled = !wlr_box_empty(&output_box);
		config_head->state.x = output_box.x;
		config_head->state.y = output_box.y;
	}

	wlr_output_manager_v1_set_configuration(server->output_manager_v1, config);

	ipc_event_output();
}

static int timer_modeset_handle(void *data) {
	struct sway_server *server = data;
	wl_event_source_remove(server->delayed_modeset);
	server->delayed_modeset = NULL;

	apply_stored_output_configs();
	return 0;
}

void request_modeset(void) {
	if (server.delayed_modeset == NULL) {
		server.delayed_modeset = wl_event_loop_add_timer(server.wl_event_loop,
			timer_modeset_handle, &server);
		wl_event_source_timer_update(server.delayed_modeset, 10);
	}
}

bool modeset_is_pending(void) {
	return server.delayed_modeset != NULL;
}

void force_modeset(void) {
	if (server.delayed_modeset != NULL) {
		wl_event_source_remove(server.delayed_modeset);
		server.delayed_modeset = NULL;
	}
	apply_stored_output_configs();
}

static void begin_destroy(struct sway_output *output) {
	// Mirrors of this output stop mirroring; the modeset requested below
	// turns them into normal outputs.
	struct sway_output *other;
	wl_list_for_each(other, &root->all_outputs, link) {
		if (other->mirror_source == output) {
			output_stop_mirroring(other);
		}
	}
	output_stop_mirroring(output);

	wl_list_remove(&output->layout_destroy.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->present.link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);

	// Remove the scene_output first to ensure that the scene does not emit
	// events for this output.
	wlr_scene_output_destroy(output->scene_output);
	output->scene_output = NULL;

	if (output->enabled) {
		output_disable(output);
	}
	output_begin_destroy(output);
	wl_list_remove(&output->link);

	output->wlr_output->data = NULL;
	output->wlr_output = NULL;

	wl_event_source_remove(output->repaint_timer);
	output->repaint_timer = NULL;

	request_modeset();
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, destroy);
	begin_destroy(output);
}

static void handle_layout_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, layout_destroy);
	begin_destroy(output);
}

static void handle_present(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, present);
	struct wlr_output_event_present *output_event = data;

	if (!output->enabled || !output_event->presented) {
		return;
	}

	output->last_presentation = output_event->when;
	output->refresh_nsec = output_event->refresh;
}

static void handle_request_state(struct wl_listener *listener, void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	const struct wlr_output_state *state = event->state;

	// Store the requested changes so that the active configuration is
	// consistent with the current state, and to avoid duplicate logic to apply
	// the changes.
	struct output_config *oc = new_output_config(output->wlr_output->name);
	if (!oc) {
		sway_log(SWAY_ERROR, "Allocation failed");
		return;
	}

	int committed = state->committed;
	if (committed & WLR_OUTPUT_STATE_MODE) {
		if (state->mode != NULL) {
			oc->width = state->mode->width;
			oc->height = state->mode->height;
			oc->refresh_rate = state->mode->refresh / 1000.f;
		} else {
			oc->width = state->custom_mode.width;
			oc->height = state->custom_mode.height;
			oc->refresh_rate = state->custom_mode.refresh / 1000.f;
		}
		committed &= ~WLR_OUTPUT_STATE_MODE;
	}
	if (committed & WLR_OUTPUT_STATE_SCALE) {
		oc->scale = state->scale;
		committed &= ~WLR_OUTPUT_STATE_SCALE;
	}
	if (committed & WLR_OUTPUT_STATE_TRANSFORM) {
		oc->transform = state->transform;
		committed &= ~WLR_OUTPUT_STATE_TRANSFORM;
	}

	// We do not expect or support any other changes here
	assert(committed == 0);
	store_output_config(oc);

	force_modeset();
}

static unsigned int last_headless_num = 0;

void handle_new_output(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if (wlr_output == root->fallback_output->wlr_output) {
		return;
	}

	if (wlr_output_is_headless(wlr_output)) {
		char name[64];
		snprintf(name, sizeof(name), "HEADLESS-%u", ++last_headless_num);
		wlr_output_set_name(wlr_output, name);
	}

	sway_log(SWAY_DEBUG, "New output %p: %s (non-desktop: %d)",
			wlr_output, wlr_output->name, wlr_output->non_desktop);

	if (wlr_output->non_desktop) {
		sway_log(SWAY_DEBUG, "Not configuring non-desktop output");
		struct sway_output_non_desktop *non_desktop = output_non_desktop_create(wlr_output);
#if WLR_HAS_DRM_BACKEND
		if (server->drm_lease_manager) {
			wlr_drm_lease_v1_manager_offer_output(server->drm_lease_manager,
					wlr_output);
		}
#endif
		list_add(root->non_desktop_outputs, non_desktop);
		return;
	}

	if (!wlr_output_init_render(wlr_output, server->allocator,
			server->renderer)) {
		sway_log(SWAY_ERROR, "Failed to init output render");
		return;
	}

	// Create the scene output here so we're not accidentally creating one for
	// the fallback output
	struct wlr_scene_output *scene_output =
		wlr_scene_output_create(root->root_scene, wlr_output);
	if (!scene_output) {
		sway_log(SWAY_ERROR, "Failed to create a scene output");
		return;
	}

	struct sway_output *output = output_create(wlr_output);
	if (!output) {
		sway_log(SWAY_ERROR, "Failed to create a sway output");
		wlr_scene_output_destroy(scene_output);
		return;
	}

	output->server = server;
	output->scene_output = scene_output;

	wl_signal_add(&root->output_layout->events.destroy, &output->layout_destroy);
	output->layout_destroy.notify = handle_layout_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_output->events.present, &output->present);
	output->present.notify = handle_present;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	output->frame.notify = handle_frame;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
	output->request_state.notify = handle_request_state;

	output->repaint_timer = wl_event_loop_add_timer(server->wl_event_loop,
		output_repaint_timer_handler, output);

	if (server->session_lock.lock) {
		sway_session_lock_add_output(server->session_lock.lock, output);
	}

	request_modeset();
}

static struct output_config *output_config_for_config_head(
		struct wlr_output_configuration_head_v1 *config_head) {
	struct output_config *oc = new_output_config(config_head->state.output->name);
	if (!oc) {
		return NULL;
	}

	oc->enabled = config_head->state.enabled;
	if (!oc->enabled) {
		return oc;
	}

	if (config_head->state.mode != NULL) {
		struct wlr_output_mode *mode = config_head->state.mode;
		oc->width = mode->width;
		oc->height = mode->height;
		oc->refresh_rate = mode->refresh / 1000.f;
	} else {
		oc->width = config_head->state.custom_mode.width;
		oc->height = config_head->state.custom_mode.height;
		oc->refresh_rate =
			config_head->state.custom_mode.refresh / 1000.f;
	}
	oc->x = config_head->state.x;
	oc->y = config_head->state.y;
	oc->transform = config_head->state.transform;
	oc->scale = config_head->state.scale;
	oc->adaptive_sync = config_head->state.adaptive_sync_enabled;
	return oc;
}

static void output_manager_apply(struct sway_server *server,
		struct wlr_output_configuration_v1 *cfg, bool test_only) {
	bool ok = false;
	size_t configs_len = config->output_configs->length + wl_list_length(&cfg->heads);
	struct output_config **configs = calloc(configs_len, sizeof(*configs));
	if (!configs) {
		sway_log(SWAY_ERROR, "Allocation failed");
		goto error;
	}
	size_t start_new_configs = config->output_configs->length;
	for (size_t idx = 0; idx < start_new_configs; idx++) {
		configs[idx] = config->output_configs->items[idx];
	}

	size_t config_idx = start_new_configs;
	struct wlr_output_configuration_head_v1 *config_head;
	wl_list_for_each(config_head, &cfg->heads, link) {
		// Generate the configuration and store it as a temporary
		// config. We keep a record of it so we can remove it later.
		struct output_config *oc = output_config_for_config_head(config_head);
		if (!oc) {
			sway_log(SWAY_ERROR, "Allocation failed");
			goto error_config;
		}
		configs[config_idx++] = oc;
	}

	// Try to commit without degrade to off enabled. Note that this will fail
	// if any output configured for enablement fails to be enabled, even if it
	// was not part of the config heads we were asked to configure.
	ok = apply_output_configs(configs, configs_len, test_only, false);

error_config:
	for (size_t idx = start_new_configs; idx < configs_len; idx++) {
		struct output_config *cfg = configs[idx];
		if (!test_only && ok) {
			store_output_config(cfg);
		} else {
			free_output_config(cfg);
		}
	}
	free(configs);

error:
	if (ok) {
		wlr_output_configuration_v1_send_succeeded(cfg);
		if (server->delayed_modeset != NULL) {
			wl_event_source_remove(server->delayed_modeset);
			server->delayed_modeset = NULL;
		}
	} else {
		wlr_output_configuration_v1_send_failed(cfg);
	}
	wlr_output_configuration_v1_destroy(cfg);
}

void handle_output_manager_apply(struct wl_listener *listener, void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	output_manager_apply(server, config, false);
}

void handle_output_manager_test(struct wl_listener *listener, void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, output_manager_test);
	struct wlr_output_configuration_v1 *config = data;

	output_manager_apply(server, config, true);
}

void handle_output_power_manager_set_mode(struct wl_listener *listener,
		void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct sway_output *output = event->output->data;

	struct output_config *oc = new_output_config(output->wlr_output->name);
	if (!oc) {
		sway_log(SWAY_ERROR, "Allocation failed");
		return;
	}

	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		oc->power = 0;
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		oc->power = 1;
		break;
	}
	store_output_config(oc);
	request_modeset();
}
