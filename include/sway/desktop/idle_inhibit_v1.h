#ifndef _SWAY_DESKTOP_IDLE_INHIBIT_V1_H
#define _SWAY_DESKTOP_IDLE_INHIBIT_V1_H
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle.h>
#include "sway/server.h"

struct sway_idle_inhibit_manager_v1 {
	struct wlr_idle_inhibit_manager_v1 *wlr_manager;
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;

	struct wlr_idle *idle;
};

struct sway_idle_inhibitor_v1 {
	struct sway_idle_inhibit_manager_v1 *manager;
	struct sway_view *view;

	struct wl_list link;
	struct wl_listener destroy;
};

void idle_inhibit_v1_check_active(
	struct sway_idle_inhibit_manager_v1 *manager);

struct sway_idle_inhibit_manager_v1 *sway_idle_inhibit_manager_v1_create(
	struct wl_display *wl_display, struct wlr_idle *idle);
#endif
