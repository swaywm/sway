#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "config.h"
#include "list.h"
#include "sway/desktop/idle_inhibit_v1.h"
#if HAVE_XWAYLAND
#include "sway/xwayland.h"
#endif

struct sway_transaction;

struct sway_session_lock {
	struct wlr_session_lock_v1 *lock;
	struct wlr_surface *focused;
	bool abandoned;

	struct wl_list outputs; // struct sway_session_lock_output

	// invalid if the session is abandoned
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
};

struct sway_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	const char *socket;

	struct wlr_backend *backend;
	struct wlr_session *session;
	// secondary headless backend used for creating virtual outputs on-the-fly
	struct wlr_backend *headless_backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;

	struct wlr_compositor *compositor;

	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;

	struct wlr_data_device_manager *data_device_manager;

	struct sway_input_manager *input;

	struct wl_listener new_output;
	struct wl_listener output_layout_change;

	struct wlr_idle_notifier_v1 *idle_notifier_v1;
	struct sway_idle_inhibit_manager_v1 idle_inhibit_manager_v1;

	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener layer_shell_surface;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener xdg_shell_toplevel;

	struct wlr_tablet_manager_v2 *tablet_v2;

#if HAVE_XWAYLAND
	struct sway_xwayland xwayland;
	struct wl_listener xwayland_surface;
	struct wl_listener xwayland_ready;
#endif

	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;

	struct wlr_server_decoration_manager *server_decoration_manager;
	struct wl_listener server_decoration;
	struct wl_list decorations; // sway_server_decoration::link

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
	struct wl_listener xdg_decoration;
	struct wl_list xdg_decorations; // sway_xdg_decoration::link

	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct wl_listener drm_lease_request;

	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wl_listener pointer_constraint;

	struct wlr_output_manager_v1 *output_manager_v1;
	struct wl_listener output_manager_apply;
	struct wl_listener output_manager_test;

	struct wlr_gamma_control_manager_v1 *gamma_control_manager_v1;
	struct wl_listener gamma_control_set_gamma;

	struct {
		struct sway_session_lock *lock;
		struct wlr_session_lock_manager_v1 *manager;

		struct wl_listener new_lock;
		struct wl_listener manager_destroy;
	} session_lock;

	struct wlr_output_power_manager_v1 *output_power_manager_v1;
	struct wl_listener output_power_manager_set_mode;
	struct wlr_input_method_manager_v2 *input_method;
	struct wlr_text_input_manager_v3 *text_input;
	struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wlr_content_type_manager_v1 *content_type_manager_v1;
	struct wlr_data_control_manager_v1 *data_control_manager_v1;
	struct wlr_screencopy_manager_v1 *screencopy_manager_v1;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager_v1;
	struct wlr_security_context_manager_v1 *security_context_manager_v1;

	struct wlr_xdg_activation_v1 *xdg_activation_v1;
	struct wl_listener xdg_activation_v1_request_activate;
	struct wl_listener xdg_activation_v1_new_token;

	struct wl_listener request_set_cursor_shape;

	struct wl_list pending_launcher_ctxs; // launcher_ctx::link

	// The timeout for transactions, after which a transaction is applied
	// regardless of readiness.
	size_t txn_timeout_ms;

	// Stores a transaction after it has been committed, but is waiting for
	// views to ack the new dimensions before being applied. A queued
	// transaction is frozen and must not have new instructions added to it.
	struct sway_transaction *queued_transaction;

	// Stores a pending transaction that will be committed once the existing
	// queued transaction is applied and freed. The pending transaction can be
	// updated with new instructions as needed.
	struct sway_transaction *pending_transaction;

	// Stores the nodes that have been marked as "dirty" and will be put into
	// the pending transaction.
	list_t *dirty_nodes;
};

extern struct sway_server server;

struct sway_debug {
	bool noatomic;         // Ignore atomic layout updates
	bool txn_timings;      // Log verbose messages about transactions
	bool txn_wait;         // Always wait for the timeout before applying
	bool legacy_wl_drm;    // Enable the legacy wl_drm interface
};

extern struct sway_debug debug;

extern bool allow_unsupported_gpu;

bool server_init(struct sway_server *server);
void server_fini(struct sway_server *server);
bool server_start(struct sway_server *server);
void server_run(struct sway_server *server);

void restore_nofile_limit(void);

void handle_new_output(struct wl_listener *listener, void *data);

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data);
void handle_layer_shell_surface(struct wl_listener *listener, void *data);
void sway_session_lock_init(void);
void sway_session_lock_add_output(struct sway_session_lock *lock,
	struct sway_output *output);
bool sway_session_lock_has_surface(struct sway_session_lock *lock,
	struct wlr_surface *surface);
void handle_xdg_shell_toplevel(struct wl_listener *listener, void *data);
#if HAVE_XWAYLAND
void handle_xwayland_surface(struct wl_listener *listener, void *data);
#endif
void handle_server_decoration(struct wl_listener *listener, void *data);
void handle_xdg_decoration(struct wl_listener *listener, void *data);
void handle_pointer_constraint(struct wl_listener *listener, void *data);
void xdg_activation_v1_handle_request_activate(struct wl_listener *listener,
	void *data);
void xdg_activation_v1_handle_new_token(struct wl_listener *listener,
	void *data);

void set_rr_scheduling(void);

#endif
