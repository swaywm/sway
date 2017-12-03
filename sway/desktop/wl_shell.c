#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_wl_shell.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/server.h"
#include "sway/view.h"
#include "log.h"

static bool assert_wl_shell(struct sway_view *view) {
	return sway_assert(view->type == SWAY_WL_SHELL_VIEW,
		"Expecting wl_shell view!");
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (!assert_wl_shell(view)) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_wl_shell_surface->title;
	case VIEW_PROP_APP_ID:
		return view->wlr_wl_shell_surface->class;
	default:
		return NULL;
	}
}

