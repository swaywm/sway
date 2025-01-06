#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/config.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <xf86drm.h>
#include "config.h"
#include "list.h"
#include "log.h"
#include "sway/config.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/input/cursor.h"
#include "sway/tree/root.h"

#if WLR_HAS_XWAYLAND
#include <wlr/xwayland/shell.h>
#include "sway/xwayland.h"
#endif

#if WLR_HAS_DRM_BACKEND
#include <wlr/types/wlr_drm_lease_v1.h>
#endif

#define SWAY_XDG_SHELL_VERSION 5
#define SWAY_LAYER_SHELL_VERSION 4
#define SWAY_FOREIGN_TOPLEVEL_LIST_VERSION 1
#define SWAY_PRESENTATION_VERSION 2

bool allow_unsupported_gpu = false;

#if WLR_HAS_DRM_BACKEND
static void handle_drm_lease_request(struct wl_listener *listener, void *data) {
	/* We only offer non-desktop outputs, but in the future we might want to do
	 * more logic here. */

	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);
	if (!lease) {
		sway_log(SWAY_ERROR, "Failed to grant lease request");
		wlr_drm_lease_request_v1_reject(req);
	}
}
#endif

static bool is_privileged(const struct wl_global *global) {
#if WLR_HAS_DRM_BACKEND
	if (server.drm_lease_manager != NULL) {
		struct wlr_drm_lease_device_v1 *drm_lease_dev;
		wl_list_for_each(drm_lease_dev, &server.drm_lease_manager->devices, link) {
			if (drm_lease_dev->global == global) {
				return true;
			}
		}
	}
#endif

	return
		global == server.output_manager_v1->global ||
		global == server.output_power_manager_v1->global ||
		global == server.input_method->global ||
		global == server.foreign_toplevel_list->global ||
		global == server.foreign_toplevel_manager->global ||
		global == server.data_control_manager_v1->global ||
		global == server.screencopy_manager_v1->global ||
		global == server.export_dmabuf_manager_v1->global ||
		global == server.security_context_manager_v1->global ||
		global == server.gamma_control_manager_v1->global ||
		global == server.layer_shell->global ||
		global == server.session_lock.manager->global ||
		global == server.input->keyboard_shortcuts_inhibit->global ||
		global == server.input->virtual_keyboard->global ||
		global == server.input->virtual_pointer->global ||
		global == server.input->transient_seat_manager->global ||
		global == server.xdg_output_manager_v1->global;
}

static bool filter_global(const struct wl_client *client,
		const struct wl_global *global, void *data) {
#if WLR_HAS_XWAYLAND
	struct wlr_xwayland *xwayland = server.xwayland.wlr_xwayland;
	if (xwayland && global == xwayland->shell_v1->global) {
		return xwayland->server != NULL && client == xwayland->server->client;
	}
#endif

	// Restrict usage of privileged protocols to unsandboxed clients
	// TODO: add a way for users to configure an allow-list
	const struct wlr_security_context_v1_state *security_context =
		wlr_security_context_manager_v1_lookup_client(
		server.security_context_manager_v1, (struct wl_client *)client);
	if (is_privileged(global)) {
		return security_context == NULL;
	}

	return true;
}

static void detect_proprietary(struct wlr_backend *backend, void *data) {
	int drm_fd = wlr_backend_get_drm_fd(backend);
	if (drm_fd < 0) {
		return;
	}

	drmVersion *version = drmGetVersion(drm_fd);
	if (version == NULL) {
		sway_log(SWAY_ERROR, "drmGetVersion() failed");
		return;
	}

	bool is_unsupported = false;
	if (strcmp(version->name, "nvidia-drm") == 0) {
		is_unsupported = true;
		sway_log(SWAY_ERROR, "!!! Proprietary Nvidia drivers are in use !!!");
		if (!allow_unsupported_gpu) {
			sway_log(SWAY_ERROR, "Use Nouveau instead");
		}
	}

	if (strcmp(version->name, "evdi") == 0) {
		is_unsupported = true;
		sway_log(SWAY_ERROR, "!!! Proprietary DisplayLink drivers are in use !!!");
	}

	if (!allow_unsupported_gpu && is_unsupported) {
		sway_log(SWAY_ERROR,
			"Proprietary drivers are NOT supported. To launch sway anyway, "
			"launch with --unsupported-gpu and DO NOT report issues.");
		exit(EXIT_FAILURE);
	}

	drmFreeVersion(version);
}

static void handle_renderer_lost(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, renderer_lost);

	sway_log(SWAY_INFO, "Re-creating renderer after GPU reset");

	struct wlr_renderer *renderer = wlr_renderer_autocreate(server->backend);
	if (renderer == NULL) {
		sway_log(SWAY_ERROR, "Unable to create renderer");
		return;
	}

	struct wlr_allocator *allocator =
		wlr_allocator_autocreate(server->backend, renderer);
	if (allocator == NULL) {
		sway_log(SWAY_ERROR, "Unable to create allocator");
		wlr_renderer_destroy(renderer);
		return;
	}

	struct wlr_renderer *old_renderer = server->renderer;
	struct wlr_allocator *old_allocator = server->allocator;
	server->renderer = renderer;
	server->allocator = allocator;

	wl_list_remove(&server->renderer_lost.link);
	wl_signal_add(&server->renderer->events.lost, &server->renderer_lost);

	wlr_compositor_set_renderer(server->compositor, renderer);

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		wlr_output_init_render(output->wlr_output,
			server->allocator, server->renderer);
	}

	wlr_allocator_destroy(old_allocator);
	wlr_renderer_destroy(old_renderer);
}

bool server_init(struct sway_server *server) {
	sway_log(SWAY_DEBUG, "Initializing Wayland server");
	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

	wl_display_set_global_filter(server->wl_display, filter_global, NULL);

	root = root_create(server->wl_display);

	server->backend = wlr_backend_autocreate(server->wl_event_loop, &server->session);
	if (!server->backend) {
		sway_log(SWAY_ERROR, "Unable to create backend");
		return false;
	}

	wlr_multi_for_each_backend(server->backend, detect_proprietary, NULL);

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		sway_log(SWAY_ERROR, "Failed to create renderer");
		return false;
	}

	server->renderer_lost.notify = handle_renderer_lost;
	wl_signal_add(&server->renderer->events.lost, &server->renderer_lost);

	wlr_renderer_init_wl_shm(server->renderer, server->wl_display);

	if (wlr_renderer_get_texture_formats(server->renderer, WLR_BUFFER_CAP_DMABUF) != NULL) {
		server->linux_dmabuf_v1 = wlr_linux_dmabuf_v1_create_with_renderer(
			server->wl_display, 4, server->renderer);
		if (debug.legacy_wl_drm) {
			wlr_drm_create(server->wl_display, server->renderer);
		}
	}
	if (wlr_renderer_get_drm_fd(server->renderer) >= 0 &&
			server->renderer->features.timeline &&
			server->backend->features.timeline) {
		wlr_linux_drm_syncobj_manager_v1_create(server->wl_display, 1,
			wlr_renderer_get_drm_fd(server->renderer));
	}

	server->allocator = wlr_allocator_autocreate(server->backend,
		server->renderer);
	if (!server->allocator) {
		sway_log(SWAY_ERROR, "Failed to create allocator");
		return false;
	}

	server->compositor = wlr_compositor_create(server->wl_display, 6,
		server->renderer);

	wlr_subcompositor_create(server->wl_display);

	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	server->gamma_control_manager_v1 =
		wlr_gamma_control_manager_v1_create(server->wl_display);
	wlr_scene_set_gamma_control_manager_v1(root->root_scene,
		server->gamma_control_manager_v1);

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	server->xdg_output_manager_v1 =
		wlr_xdg_output_manager_v1_create(server->wl_display, root->output_layout);

	server->idle_notifier_v1 = wlr_idle_notifier_v1_create(server->wl_display);
	sway_idle_inhibit_manager_v1_init();

	server->layer_shell = wlr_layer_shell_v1_create(server->wl_display,
		SWAY_LAYER_SHELL_VERSION);
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->layer_shell_surface);
	server->layer_shell_surface.notify = handle_layer_shell_surface;

	server->xdg_shell = wlr_xdg_shell_create(server->wl_display,
		SWAY_XDG_SHELL_VERSION);
	wl_signal_add(&server->xdg_shell->events.new_toplevel,
		&server->xdg_shell_toplevel);
	server->xdg_shell_toplevel.notify = handle_xdg_shell_toplevel;

	server->tablet_v2 = wlr_tablet_v2_create(server->wl_display);

	server->server_decoration_manager =
		wlr_server_decoration_manager_create(server->wl_display);
	wlr_server_decoration_manager_set_default_mode(
		server->server_decoration_manager,
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	wl_signal_add(&server->server_decoration_manager->events.new_decoration,
		&server->server_decoration);
	server->server_decoration.notify = handle_server_decoration;
	wl_list_init(&server->decorations);

	server->xdg_decoration_manager =
		wlr_xdg_decoration_manager_v1_create(server->wl_display);
	wl_signal_add(
			&server->xdg_decoration_manager->events.new_toplevel_decoration,
			&server->xdg_decoration);
	server->xdg_decoration.notify = handle_xdg_decoration;
	wl_list_init(&server->xdg_decorations);

	server->relative_pointer_manager =
		wlr_relative_pointer_manager_v1_create(server->wl_display);

	server->pointer_constraints =
		wlr_pointer_constraints_v1_create(server->wl_display);
	server->pointer_constraint.notify = handle_pointer_constraint;
	wl_signal_add(&server->pointer_constraints->events.new_constraint,
		&server->pointer_constraint);

	wlr_presentation_create(server->wl_display, server->backend, SWAY_PRESENTATION_VERSION);
	wlr_alpha_modifier_v1_create(server->wl_display);

	server->output_manager_v1 =
		wlr_output_manager_v1_create(server->wl_display);
	server->output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&server->output_manager_v1->events.apply,
		&server->output_manager_apply);
	server->output_manager_test.notify = handle_output_manager_test;
	wl_signal_add(&server->output_manager_v1->events.test,
		&server->output_manager_test);

	server->output_power_manager_v1 =
		wlr_output_power_manager_v1_create(server->wl_display);
	server->output_power_manager_set_mode.notify =
		handle_output_power_manager_set_mode;
	wl_signal_add(&server->output_power_manager_v1->events.set_mode,
		&server->output_power_manager_set_mode);
	server->input_method = wlr_input_method_manager_v2_create(server->wl_display);
	server->text_input = wlr_text_input_manager_v3_create(server->wl_display);
	server->foreign_toplevel_list =
		wlr_ext_foreign_toplevel_list_v1_create(server->wl_display, SWAY_FOREIGN_TOPLEVEL_LIST_VERSION);
	server->foreign_toplevel_manager =
		wlr_foreign_toplevel_manager_v1_create(server->wl_display);

	sway_session_lock_init();

#if WLR_HAS_DRM_BACKEND
	server->drm_lease_manager=
		wlr_drm_lease_v1_manager_create(server->wl_display, server->backend);
	if (server->drm_lease_manager) {
		server->drm_lease_request.notify = handle_drm_lease_request;
		wl_signal_add(&server->drm_lease_manager->events.request,
				&server->drm_lease_request);
	} else {
		sway_log(SWAY_DEBUG, "Failed to create wlr_drm_lease_device_v1");
		sway_log(SWAY_INFO, "VR will not be available");
	}
#endif

	server->export_dmabuf_manager_v1 = wlr_export_dmabuf_manager_v1_create(server->wl_display);
	server->screencopy_manager_v1 = wlr_screencopy_manager_v1_create(server->wl_display);
	server->data_control_manager_v1 = wlr_data_control_manager_v1_create(server->wl_display);
	server->security_context_manager_v1 = wlr_security_context_manager_v1_create(server->wl_display);
	wlr_viewporter_create(server->wl_display);
	wlr_single_pixel_buffer_manager_v1_create(server->wl_display);
	server->content_type_manager_v1 =
		wlr_content_type_manager_v1_create(server->wl_display, 1);
	wlr_fractional_scale_manager_v1_create(server->wl_display, 1);

	server->tearing_control_v1 =
		wlr_tearing_control_manager_v1_create(server->wl_display, 1);
	server->tearing_control_new_object.notify = handle_new_tearing_hint;
	wl_signal_add(&server->tearing_control_v1->events.new_object,
		&server->tearing_control_new_object);
	wl_list_init(&server->tearing_controllers);

	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(server->wl_display);
	wlr_xdg_foreign_v1_create(server->wl_display, foreign_registry);
	wlr_xdg_foreign_v2_create(server->wl_display, foreign_registry);

	server->xdg_activation_v1 = wlr_xdg_activation_v1_create(server->wl_display);
	server->xdg_activation_v1_request_activate.notify =
		xdg_activation_v1_handle_request_activate;
	wl_signal_add(&server->xdg_activation_v1->events.request_activate,
		&server->xdg_activation_v1_request_activate);
	server->xdg_activation_v1_new_token.notify =
		xdg_activation_v1_handle_new_token;
	wl_signal_add(&server->xdg_activation_v1->events.new_token,
		&server->xdg_activation_v1_new_token);

	struct wlr_cursor_shape_manager_v1 *cursor_shape_manager =
		wlr_cursor_shape_manager_v1_create(server->wl_display, 1);
	server->request_set_cursor_shape.notify = handle_request_set_cursor_shape;
	wl_signal_add(&cursor_shape_manager->events.request_set_shape, &server->request_set_cursor_shape);

	wl_list_init(&server->pending_launcher_ctxs);

	// Avoid using "wayland-0" as display socket
	char name_candidate[16];
	for (unsigned int i = 1; i <= 32; ++i) {
		snprintf(name_candidate, sizeof(name_candidate), "wayland-%u", i);
		if (wl_display_add_socket(server->wl_display, name_candidate) >= 0) {
			server->socket = strdup(name_candidate);
			break;
		}
	}

	if (!server->socket) {
		sway_log(SWAY_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(server->backend);
		return false;
	}

	server->headless_backend = wlr_headless_backend_create(server->wl_event_loop);
	if (!server->headless_backend) {
		sway_log(SWAY_ERROR, "Failed to create secondary headless backend");
		wlr_backend_destroy(server->backend);
		return false;
	} else {
		wlr_multi_backend_add(server->backend, server->headless_backend);
	}

	struct wlr_output *wlr_output =
			wlr_headless_add_output(server->headless_backend, 800, 600);
	wlr_output_set_name(wlr_output, "FALLBACK");
	root->fallback_output = output_create(wlr_output);

	// This may have been set already via -Dtxn-timeout
	if (!server->txn_timeout_ms) {
		server->txn_timeout_ms = 200;
	}

	server->dirty_nodes = create_list();

	server->input = input_manager_create(server);
	input_manager_get_default_seat(); // create seat0

	return true;
}

void server_fini(struct sway_server *server) {
	// TODO: free sway-specific resources
#if WLR_HAS_XWAYLAND
	wlr_xwayland_destroy(server->xwayland.wlr_xwayland);
#endif
	wl_display_destroy_clients(server->wl_display);
	wlr_backend_destroy(server->backend);
	wl_display_destroy(server->wl_display);
	list_free(server->dirty_nodes);
}

bool server_start(struct sway_server *server) {
#if WLR_HAS_XWAYLAND
	if (config->xwayland != XWAYLAND_MODE_DISABLED) {
		sway_log(SWAY_DEBUG, "Initializing Xwayland (lazy=%d)",
				config->xwayland == XWAYLAND_MODE_LAZY);
		server->xwayland.wlr_xwayland =
			wlr_xwayland_create(server->wl_display, server->compositor,
					config->xwayland == XWAYLAND_MODE_LAZY);
		if (!server->xwayland.wlr_xwayland) {
			sway_log(SWAY_ERROR, "Failed to start Xwayland");
			unsetenv("DISPLAY");
		} else {
			wl_signal_add(&server->xwayland.wlr_xwayland->events.new_surface,
				&server->xwayland_surface);
			server->xwayland_surface.notify = handle_xwayland_surface;
			wl_signal_add(&server->xwayland.wlr_xwayland->events.ready,
				&server->xwayland_ready);
			server->xwayland_ready.notify = handle_xwayland_ready;

			setenv("DISPLAY", server->xwayland.wlr_xwayland->display_name, true);

			/* xcursor configured by the default seat */
		}
	}
#endif

	if (config->primary_selection) {
		wlr_primary_selection_v1_device_manager_create(server->wl_display);
	}

	sway_log(SWAY_INFO, "Starting backend on wayland display '%s'",
			server->socket);
	if (!wlr_backend_start(server->backend)) {
		sway_log(SWAY_ERROR, "Failed to start backend");
		wlr_backend_destroy(server->backend);
		return false;
	}

	return true;
}

void server_run(struct sway_server *server) {
	sway_log(SWAY_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	wl_display_run(server->wl_display);
}
