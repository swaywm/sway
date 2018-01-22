#include "sway/view.h"

const char *view_get_title(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_TITLE);
	}
	return NULL;
}

const char *view_get_app_id(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_APP_ID);
	}
	return NULL;
}

const char *view_get_class(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_CLASS);
	}
	return NULL;
}

const char *view_get_instance(struct sway_view *view) {
	if (view->iface.get_prop) {
		return view->iface.get_prop(view, VIEW_PROP_INSTANCE);
	}
	return NULL;
}

void view_set_size(struct sway_view *view, int width, int height) {
	if (view->iface.set_size) {
		view->iface.set_size(view, width, height);
	}
}

void view_set_position(struct sway_view *view, double ox, double oy) {
	if (view->iface.set_position) {
		view->iface.set_position(view, ox, oy);
	}
}

void view_set_activated(struct sway_view *view, bool activated) {
	if (view->iface.set_activated) {
		view->iface.set_activated(view, activated);
	}
}

void view_close(struct sway_view *view) {
	if (view->iface.close) {
		view->iface.close(view);
	}
}
