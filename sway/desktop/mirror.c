#include "log.h"
#include "sway/mirror.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

/**
 * BEGIN helper functions
 */

static bool test_con_id(struct sway_container *container, void *data) {
	size_t *con_id = data;
	return container->node.id == *con_id;
}

static struct wlr_output *container_output(struct sway_container *container) {
	struct sway_workspace *workspace = container->current.workspace;
	if (workspace) {
		struct sway_output *output = workspace->current.output;
		if (output) {
			return output->wlr_output;
		}
	}
	return NULL;
}

/**
 * Stop rendering on output, arranging root as thought the output has been
 * disabled or unplugged.
 */
void vacate_output(struct sway_output *output) {

	// arranges root
	if (output->enabled) {
		output_disable(output);
	}

	// idempotent
	wlr_output_layout_remove(root->output_layout, output->wlr_output);
}

/**
 * Reclaim an output, arranging root as though it is a newly enabled output.
 *
 * Any "pending" changes that were blocked in apply_output_config during the
 * mirror session will be applied.
 */
void reclaim_output(struct sway_output *output) {

	struct output_config *oc = find_output_config(output);

	// calls output_enable
	apply_output_config(oc, output);

	free_output_config(oc);
}

/**
 * END helper functions
 */

/**
 * BEGIN sway_mirror handler functions
 */

static void handle_ready_entire(struct wl_listener *listener, void *data) {
	struct sway_mirror *mirror = wl_container_of(listener, mirror, ready);
	struct wlr_output *output = data;

	if (output != mirror->params.output_src) {
		return;
	}

	struct wlr_box box_output = { 0 };
	wlr_output_transformed_resolution(output, &box_output.width, &box_output.height);

	wlr_mirror_v1_request_box(mirror->wlr_mirror_v1, output, box_output);
}

static void handle_ready_box(struct wl_listener *listener, void *data) {
	struct sway_mirror *mirror = wl_container_of(listener, mirror, ready);
	struct wlr_output *output = data;

	if (output != mirror->params.output_src) {
		return;
	}

	wlr_mirror_v1_request_box(mirror->wlr_mirror_v1, output, mirror->params.box);
}

static void handle_ready_container(struct wl_listener *listener, void *data) {
	struct sway_mirror *mirror = wl_container_of(listener, mirror, ready);
	struct wlr_output *output = data;

	// does the container still exist?
	struct sway_container *container = root_find_container(test_con_id, &mirror->params.con_id);
	if (!container) {
		sway_log(SWAY_DEBUG, "Mirror container %ld destroyed, stopping", mirror->params.con_id);
		wlr_mirror_v1_destroy(mirror->wlr_mirror_v1);
		return;
	}

	// is the container visible?
	struct sway_view *view = container->view;
	if (!view_is_visible(view)) {
		wlr_mirror_v1_request_blank(mirror->wlr_mirror_v1);
		return;
	}

	// is the container on this output?
	if (output != container_output(container)) {
		return;
	}

	// is the container's last rendered output this output?
	if (output != view->last_output->wlr_output) {
		return;
	}

	// intersection with output should always be last_destination
	struct wlr_box box_output = { 0 };
	wlr_output_transformed_resolution(output, &box_output.width, &box_output.height);
	struct wlr_box box_src_intersected;
	wlr_box_intersection(&box_src_intersected, &view->last_destination, &box_output);

	wlr_mirror_v1_request_box(mirror->wlr_mirror_v1, output, box_src_intersected);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_mirror *mirror = wl_container_of(listener, mirror, destroy);

	sway_log(SWAY_DEBUG, "Mirror destroy dst '%s'", mirror->params.wlr_params.output_dst->name);

	wl_list_remove(&mirror->ready.link);
	wl_list_remove(&mirror->destroy.link);

	struct sway_output *output_dst = output_from_wlr_output(mirror->params.wlr_params.output_dst);
	if (output_dst) {
		reclaim_output(output_dst);
	}

	wl_list_remove(&mirror->link);

	wl_array_release(&mirror->params.wlr_params.output_srcs);
	free(mirror);
}

/**
 * END sway_mirror handler functions
 */

/**
 * BEGIN public functions
 */

bool mirror_create(struct sway_mirror_params *params) {
	if (!params || !params->wlr_params.output_dst) {
		sway_log(SWAY_ERROR, "Missing params or params->wlr_params.output_dst");
		return false;
	}

	struct sway_output *output_dst = output_from_wlr_output(params->wlr_params.output_dst);

	struct sway_mirror *mirror = calloc(1, sizeof(struct sway_mirror));

	memcpy(&mirror->params, params, sizeof(struct sway_mirror_params));
	struct wlr_mirror_v1_params *wlr_params = &mirror->params.wlr_params;
	wl_array_init(&wlr_params->output_srcs);

	switch (mirror->params.flavour) {
		case SWAY_MIRROR_FLAVOUR_ENTIRE:
			sway_log(SWAY_DEBUG, "Mirror creating dst '%s' entire", output_dst->wlr_output->name);
			mirror->ready.notify = handle_ready_entire;
			break;
		case SWAY_MIRROR_FLAVOUR_BOX:
			sway_log(SWAY_DEBUG, "Mirror creating dst '%s' box %d,%d %dx%d",
					output_dst->wlr_output->name,
					mirror->params.box.x, mirror->params.box.y,
					mirror->params.box.width, mirror->params.box.height);
			mirror->ready.notify = handle_ready_box;
			break;
		case SWAY_MIRROR_FLAVOUR_CONTAINER:
			sway_log(SWAY_DEBUG, "Mirror creating dst '%s' container con_id %ld",
					output_dst->wlr_output->name, mirror->params.con_id);
			mirror->ready.notify = handle_ready_container;
			break;
		default:
			sway_log(SWAY_ERROR, "Invalid sway_mirror_flavour.");
			goto error_params;
			break;
	}

	if (params->flavour == SWAY_MIRROR_FLAVOUR_CONTAINER) {
		// listen for ready for all enabled srcs except the dst
		for (int i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			if (output->wlr_output != wlr_params->output_dst) {
				struct wlr_output **output_src_ptr =
					wl_array_add(&wlr_params->output_srcs, sizeof(struct output_src_ptr*));
				*output_src_ptr = output->wlr_output;
			}
		}
	} else {
		// listen for ready on just the specified src
		struct wlr_output **output_src_ptr =
			wl_array_add(&wlr_params->output_srcs, sizeof(struct output_src_ptr*));
		*output_src_ptr = mirror->params.output_src;
	}

	// start the session
	mirror->wlr_mirror_v1 = wlr_mirror_v1_create(&mirror->params.wlr_params);
	if (!mirror->wlr_mirror_v1) {
		goto error_create;
	}

	vacate_output(output_dst);

	// ready events from all srcs
	wl_signal_add(&mirror->wlr_mirror_v1->events.ready, &mirror->ready);

	// mirror session end
	wl_signal_add(&mirror->wlr_mirror_v1->events.destroy, &mirror->destroy);
	mirror->destroy.notify = handle_destroy;

	// add to the global server list
	wl_list_insert(&server.mirrors, &mirror->link);

	return true;

error_params:
error_create:
	wl_array_release(&mirror->params.wlr_params.output_srcs);
	free(mirror);
	return false;
}

void mirror_destroy(struct sway_mirror *mirror) {
	if (!mirror) {
		return;
	}
	sway_log(SWAY_DEBUG, "Mirror destroying dst '%s'", mirror->params.wlr_params.output_dst->name);

	wlr_mirror_v1_destroy(mirror->wlr_mirror_v1);
}

void mirror_destroy_all() {
	struct sway_mirror *mirror, *next;
	wl_list_for_each_safe(mirror, next, &server.mirrors, link) {
		mirror_destroy(mirror);
	}
}

bool mirror_output_is_mirror_dst(struct sway_output *output) {
	return output && output->wlr_output && output->wlr_output->mirror_dst;
}

bool mirror_layout_box_within_output(struct wlr_box *box, struct wlr_output *output) {
	// translate origin to local output
	double x = box->x;
	double y = box->y;
	wlr_output_layout_output_coords(root->output_layout, output, &x, &y);
	box->x = round(x);
	box->y = round(y);

	// scale to local output
	scale_box(box, output->scale);

	// local output's box
	struct wlr_box box_output = { 0 };
	wlr_output_transformed_resolution(output, &box_output.width, &box_output.height);

	// box must be within the output
	struct wlr_box box_intersected = { 0 };
	wlr_box_intersection(&box_intersected, &box_output, box);
	if (memcmp(box, &box_intersected, sizeof(struct wlr_box)) != 0) {
		return false;
	}

	return true;
}

/**
 * END public functions
 */

