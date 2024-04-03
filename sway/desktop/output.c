#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/backend/headless.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_compositor.h>
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
	struct wlr_scene_buffer *buffer = data;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_buffer_send_frame_done(buffer, &now);
	return 0;
}

static void handle_buffer_timer_destroy(struct wl_listener *listener,
		void *data) {
	struct buffer_timer *timer = wl_container_of(listener, timer, destroy);

	wl_list_remove(&timer->destroy.link);
	wl_event_source_remove(timer->frame_done_timer);
	free(timer);
}

static struct buffer_timer *buffer_timer_get_or_create(struct wlr_scene_buffer *buffer) {
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
		handle_buffer_timer, buffer);
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
		timer = buffer_timer_get_or_create(buffer);
	}

	if (timer) {
		wl_event_source_timer_update(timer->frame_done_timer, delay);
	} else {
		wlr_scene_buffer_send_frame_done(buffer, &data->when);
	}
}

static enum wlr_scale_filter_mode get_scale_filter(struct sway_output *output,
		struct wlr_scene_buffer *buffer) {
	// if we are scaling down, we should always choose linear
	if (buffer->dst_width > 0 && buffer->dst_height > 0 && (
			buffer->dst_width < buffer->buffer_width ||
			buffer->dst_height < buffer->buffer_height)) {
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

static void output_configure_scene(struct sway_output *output,
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

		// hack: don't call the scene setter because that will damage all outputs
		// We don't want to damage outputs that aren't our current output that
		// we're configuring
		buffer->filter_mode = get_scale_filter(output, buffer);

		wlr_scene_buffer_set_opacity(buffer, opacity);
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *node;
		wl_list_for_each(node, &tree->children, link) {
			output_configure_scene(output, node, opacity);
		}
	}
}

static int output_repaint_timer_handler(void *data) {
	struct sway_output *output = data;

	if (!output->enabled) {
		return 0;
	}

	output->wlr_output->frame_pending = false;

	output_configure_scene(output, &root->root_scene->tree.node, 1.0f);

	if (output->gamma_lut_changed) {
		struct wlr_output_state pending;
		wlr_output_state_init(&pending);
		if (!wlr_scene_output_build_state(output->scene_output, &pending, NULL)) {
			return 0;
		}

		output->gamma_lut_changed = false;
		struct wlr_gamma_control_v1 *gamma_control =
			wlr_gamma_control_manager_v1_get_control(
			server.gamma_control_manager_v1, output->wlr_output);
		if (!wlr_gamma_control_v1_apply(gamma_control, &pending)) {
			wlr_output_state_finish(&pending);
			return 0;
		}

		if (!wlr_output_commit_state(output->wlr_output, &pending)) {
			wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
			wlr_output_state_finish(&pending);
			return 0;
		}

		wlr_output_state_finish(&pending);
		return 0;
	}

	wlr_scene_output_commit(output->scene_output, NULL);
	return 0;
}

static void handle_frame(struct wl_listener *listener, void *user_data) {
	struct sway_output *output =
		wl_container_of(listener, output, frame);
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

static void update_output_manager_config(struct sway_server *server) {
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output == root->fallback_output) {
			continue;
		}
		struct wlr_output_configuration_head_v1 *config_head =
			wlr_output_configuration_head_v1_create(config, output->wlr_output);
		struct wlr_box output_box;
		wlr_output_layout_get_box(root->output_layout,
			output->wlr_output, &output_box);
		// We mark the output enabled when it's switched off but not disabled
		config_head->state.enabled = !wlr_box_empty(&output_box);
		config_head->state.x = output_box.x;
		config_head->state.y = output_box.y;
	}

	wlr_output_manager_v1_set_configuration(server->output_manager_v1, config);

	ipc_event_output();
}

static void begin_destroy(struct sway_output *output) {
	struct sway_server *server = output->server;

	if (output->enabled) {
		output_disable(output);
	}

	output_begin_destroy(output);

	wl_list_remove(&output->link);

	wl_list_remove(&output->layout_destroy.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->commit.link);
	wl_list_remove(&output->present.link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);

	wlr_scene_output_destroy(output->scene_output);
	output->scene_output = NULL;
	output->wlr_output->data = NULL;
	output->wlr_output = NULL;

	transaction_commit_dirty();

	update_output_manager_config(server);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, destroy);
	begin_destroy(output);
}

static void handle_layout_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, layout_destroy);
	begin_destroy(output);
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, commit);
	struct wlr_output_event_commit *event = data;

	if (!output->enabled) {
		return;
	}

	if (event->state->committed & (
			WLR_OUTPUT_STATE_MODE |
			WLR_OUTPUT_STATE_TRANSFORM |
			WLR_OUTPUT_STATE_SCALE)) {
		arrange_layers(output);
		arrange_output(output);
		transaction_commit_dirty();

		update_output_manager_config(output->server);
	}

	// Next time the output is enabled, try to re-apply the gamma LUT
	if ((event->state->committed & WLR_OUTPUT_STATE_ENABLED) && !output->wlr_output->enabled) {
		output->gamma_lut_changed = true;
	}
}

static void handle_present(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, present);
	struct wlr_output_event_present *output_event = data;

	if (!output->enabled || !output_event->presented) {
		return;
	}

	output->last_presentation = *output_event->when;
	output->refresh_nsec = output_event->refresh;
}

static void handle_request_state(struct wl_listener *listener, void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
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
	wl_signal_add(&wlr_output->events.commit, &output->commit);
	output->commit.notify = handle_commit;
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

	apply_all_output_configs();

	transaction_commit_dirty();

	update_output_manager_config(server);
}

void handle_output_layout_change(struct wl_listener *listener,
		void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, output_layout_change);
	update_output_manager_config(server);
}

void handle_gamma_control_set_gamma(struct wl_listener *listener, void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, gamma_control_set_gamma);
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;

	struct sway_output *output = event->output->data;

	if(!output) {
		return;
	}

	output->gamma_lut_changed = true;
	wlr_output_schedule_frame(output->wlr_output);
}

static struct output_config *output_config_for_config_head(
		struct wlr_output_configuration_head_v1 *config_head,
		struct sway_output *output) {
	struct output_config *oc = new_output_config(output->wlr_output->name);
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
		struct wlr_output_configuration_v1 *config, bool test_only) {
	size_t configs_len = wl_list_length(&root->all_outputs);
	struct matched_output_config *configs = calloc(configs_len, sizeof(*configs));
	if (!configs) {
		return;
	}

	int config_idx = 0;
	struct sway_output *sway_output;
	wl_list_for_each(sway_output, &root->all_outputs, link) {
		if (sway_output == root->fallback_output) {
			configs_len--;
			continue;
		}

		struct matched_output_config *cfg = &configs[config_idx++];
		cfg->output = sway_output;

		struct wlr_output_configuration_head_v1 *config_head;
		wl_list_for_each(config_head, &config->heads, link) {
			if (config_head->state.output == sway_output->wlr_output) {
				cfg->config = output_config_for_config_head(config_head, sway_output);
				break;
			}
		}
		if (!cfg->config) {
			cfg->config = find_output_config(sway_output);
		}
	}

	bool ok = apply_output_configs(configs, configs_len, test_only);
	for (size_t idx = 0; idx < configs_len; idx++) {
		struct matched_output_config *cfg = &configs[idx];

		// Only store new configs for successful non-test commits. Old configs,
		// test-only and failed commits just get freed.
		bool store_config = false;
		if (!test_only && ok) {
			struct wlr_output_configuration_head_v1 *config_head;
			wl_list_for_each(config_head, &config->heads, link) {
				if (config_head->state.output == sway_output->wlr_output) {
					store_config = true;
					break;
				}
			}
		}
		if (store_config) {
			store_output_config(cfg->config);
		} else {
			free_output_config(cfg->config);
		}
	}
	free(configs);

	if (ok) {
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);

	if (!test_only) {
		update_output_manager_config(server);
	}
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
	switch (event->mode) {
	case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
		oc->power = 0;
		break;
	case ZWLR_OUTPUT_POWER_V1_MODE_ON:
		oc->power = 1;
		break;
	}
	store_output_config(oc);
	apply_all_output_configs();
}
