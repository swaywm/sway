#include <json-c/json.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <libinput.h>
#include "sway/container.h"
#include "sway/input.h"
#include "sway/ipc-json.h"
#include "util.h"

static json_object *ipc_json_create_rect(swayc_t *c) {
	json_object *rect = json_object_new_object();

	json_object_object_add(rect, "x", json_object_new_int((int32_t)c->x));
	json_object_object_add(rect, "y", json_object_new_int((int32_t)c->y));

	struct wlc_size size;
	if (c->type == C_OUTPUT) {
		size = *wlc_output_get_resolution(c->handle);
	} else {
		size.w = c->width;
		size.h = c->height;
	}

	json_object_object_add(rect, "width", json_object_new_int((int32_t)size.w));
	json_object_object_add(rect, "height", json_object_new_int((int32_t)size.h));

	return rect;
}

static json_object *ipc_json_create_rect_from_geometry(struct wlc_geometry g) {
	json_object *rect = json_object_new_object();

	json_object_object_add(rect, "x", json_object_new_int(g.origin.x));
	json_object_object_add(rect, "y", json_object_new_int(g.origin.y));
	json_object_object_add(rect, "width", json_object_new_int(g.size.w));
	json_object_object_add(rect, "height", json_object_new_int(g.size.h));

	return rect;
}

static const char *ipc_json_border_description(swayc_t *c) {
	const char *border;

	switch (c->border_type) {
	case B_PIXEL:
		border = "1pixel";
		break;

	case B_NORMAL:
		border = "normal";
		break;

	case B_NONE: // fallthrough
	default:
		border = "none";
		break;
	}

	return border;
}

static const char *ipc_json_layout_description(enum swayc_layouts l) {
	const char *layout;

	switch (l) {
	case L_VERT:
		layout = "splitv";
		break;

	case L_HORIZ:
		layout = "splith";
		break;

	case L_TABBED:
		layout = "tabbed";
		break;

	case L_STACKED:
		layout = "stacked";
		break;

	case L_FLOATING:
		layout = "floating";
		break;

	case L_NONE: // fallthrough
	case L_LAYOUTS: // fallthrough; this should never happen, I'm just trying to silence compiler warnings
	default:
		layout = "null";
		break;
	}

	return layout;
}

static float ipc_json_child_percentage(swayc_t *c) {
	float percent = 0;
	swayc_t *parent = c->parent;

	if (parent) {
		switch (parent->layout) {
		case L_VERT:
			percent = c->height / parent->height;
			break;

		case L_HORIZ:
			percent = c->width / parent->width;
			break;

		case L_STACKED: // fallthrough
		case L_TABBED: // fallthrough
			percent = 1.0;
			break;

		default:
			break;
		}
	}

	return percent;
}

static void ipc_json_describe_output(swayc_t *output, json_object *object) {
	uint32_t scale = wlc_output_get_scale(output->handle);
	json_object_object_add(object, "active", json_object_new_boolean(true));
	json_object_object_add(object, "primary", json_object_new_boolean(false));
	json_object_object_add(object, "layout", json_object_new_string("output"));
	json_object_object_add(object, "type", json_object_new_string("output"));
	json_object_object_add(object, "current_workspace",
		(output->focused) ? json_object_new_string(output->focused->name) : NULL);
	json_object_object_add(object, "scale", json_object_new_int(scale));
}

static void ipc_json_describe_workspace(swayc_t *workspace, json_object *object) {
	int num = (isdigit(workspace->name[0])) ? atoi(workspace->name) : -1;
	const char *layout = ipc_json_layout_description(workspace->workspace_layout);

	json_object_object_add(object, "num", json_object_new_int(num));
	json_object_object_add(object, "output", (workspace->parent) ? json_object_new_string(workspace->parent->name) : NULL);
	json_object_object_add(object, "type", json_object_new_string("workspace"));
	json_object_object_add(object, "layout", (strcmp(layout, "null") == 0) ? NULL : json_object_new_string(layout));
}

// window is in the scratchpad ? changed : none
static const char *ipc_json_get_scratchpad_state(swayc_t *c) {
	int i;
	for (i = 0; i < scratchpad->length; i++) {
		if (scratchpad->items[i] == c) {
			return "changed";
		}
	}
	return "none"; // we ignore the fresh value
}

static void ipc_json_describe_view(swayc_t *c, json_object *object) {
	json_object *props = json_object_new_object();
	json_object_object_add(object, "type", json_object_new_string((c->is_floating) ? "floating_con" : "con"));

	wlc_handle parent = wlc_view_get_parent(c->handle);
	json_object_object_add(object, "scratchpad_state",
		json_object_new_string(ipc_json_get_scratchpad_state(c)));

	json_object_object_add(object, "name", (c->name) ? json_object_new_string(c->name) : NULL);

	json_object_object_add(props, "class", c->class ? json_object_new_string(c->class) :
		c->app_id ? json_object_new_string(c->app_id) : NULL);
	json_object_object_add(props, "instance", c->instance ? json_object_new_string(c->instance) :
		c->app_id ? json_object_new_string(c->app_id) : NULL);
	json_object_object_add(props, "title", (c->name) ? json_object_new_string(c->name) : NULL);
	json_object_object_add(props, "transient_for", parent ? json_object_new_int(parent) : NULL);
	json_object_object_add(object, "window_properties", props);

	json_object_object_add(object, "fullscreen_mode",
		json_object_new_int(swayc_is_fullscreen(c) ? 1 : 0));
	json_object_object_add(object, "sticky", json_object_new_boolean(c->sticky));
	json_object_object_add(object, "floating", json_object_new_string(
		c->is_floating ? "auto_on" : "auto_off")); // we can't state the cause

	json_object_object_add(object, "app_id", c->app_id ? json_object_new_string(c->app_id) : NULL);

	if (c->parent) {
		const char *layout = (c->parent->type == C_CONTAINER) ?
			ipc_json_layout_description(c->parent->layout) : "none";
		const char *last_layout = (c->parent->type == C_CONTAINER) ?
			ipc_json_layout_description(c->parent->prev_layout) : "none";
		json_object_object_add(object, "layout",
			(strcmp(layout, "null") == 0) ? NULL : json_object_new_string(layout));
		json_object_object_add(object, "last_split_layout",
			(strcmp(last_layout, "null") == 0) ? NULL : json_object_new_string(last_layout));
		json_object_object_add(object, "workspace_layout",
			json_object_new_string(ipc_json_layout_description(swayc_parent_by_type(c, C_WORKSPACE)->workspace_layout)));
	}
}

static void ipc_json_describe_root(swayc_t *c, json_object *object) {
	json_object_object_add(object, "type", json_object_new_string("root"));
	json_object_object_add(object, "layout", json_object_new_string("splith"));
}

json_object *ipc_json_describe_container(swayc_t *c) {
	float percent = ipc_json_child_percentage(c);

	if (!(sway_assert(c, "Container must not be null."))) {
		return NULL;
	}

	json_object *object = json_object_new_object();

	json_object_object_add(object, "id", json_object_new_int((int)c->id));
	json_object_object_add(object, "name", (c->name) ? json_object_new_string(c->name) : NULL);
	json_object_object_add(object, "rect", ipc_json_create_rect(c));
	json_object_object_add(object, "visible", json_object_new_boolean(c->visible));
	json_object_object_add(object, "focused", json_object_new_boolean(c == current_focus));

	json_object_object_add(object, "border", json_object_new_string(ipc_json_border_description(c)));
	json_object_object_add(object, "window_rect", ipc_json_create_rect_from_geometry(c->actual_geometry));
	json_object_object_add(object, "deco_rect", ipc_json_create_rect_from_geometry(c->title_bar_geometry));
	json_object_object_add(object, "geometry", ipc_json_create_rect_from_geometry(c->cached_geometry));
	json_object_object_add(object, "percent", (percent > 0) ? json_object_new_double(percent) : NULL);
	json_object_object_add(object, "window", json_object_new_int(c->handle)); // for the sake of i3 compat
	// TODO: make urgency actually work once Sway supports it
	json_object_object_add(object, "urgent", json_object_new_boolean(false));
	json_object_object_add(object, "current_border_width", json_object_new_int(c->border_thickness));

	switch (c->type) {
	case C_ROOT:
		ipc_json_describe_root(c, object);
		break;

	case C_OUTPUT:
		ipc_json_describe_output(c, object);
		break;

	case C_CONTAINER: // fallthrough
	case C_VIEW:
		ipc_json_describe_view(c, object);
		break;

	case C_WORKSPACE:
		ipc_json_describe_workspace(c, object);
		break;

	case C_TYPES: // fallthrough; this should never happen, I'm just trying to silence compiler warnings
	default:
		break;
	}

	return object;
}

json_object *ipc_json_describe_input(struct libinput_device *device) {
	char* identifier = libinput_dev_unique_id(device);
	int vendor = libinput_device_get_id_vendor(device);
	int product = libinput_device_get_id_product(device);
	const char *name = libinput_device_get_name(device);
	double width = -1, height = -1;
	int has_size = libinput_device_get_size(device, &width, &height);

	json_object *device_object = json_object_new_object();
	json_object_object_add(device_object,"identifier",
			identifier ? json_object_new_string(identifier) : NULL);
	json_object_object_add(device_object,
			"vendor", json_object_new_int(vendor));
	json_object_object_add(device_object,
			"product", json_object_new_int(product));
	json_object_object_add(device_object,
			"name", json_object_new_string(name));
	if (has_size == 0) {
		json_object *size_object = json_object_new_object();
		json_object_object_add(size_object,
				"width", json_object_new_double(width));
		json_object_object_add(size_object,
				"height", json_object_new_double(height));
	} else {
		json_object_object_add(device_object, "size", NULL);
	}

	struct {
		enum libinput_device_capability cap;
		const char *name;
		// If anyone feels like implementing device-specific IPC output,
		// be my guest
		json_object *(*describe)(struct libinput_device *);
	} caps[] = {
		{ LIBINPUT_DEVICE_CAP_KEYBOARD, "keyboard", NULL },
		{ LIBINPUT_DEVICE_CAP_POINTER, "pointer", NULL },
		{ LIBINPUT_DEVICE_CAP_TOUCH, "touch", NULL },
		{ LIBINPUT_DEVICE_CAP_TABLET_TOOL, "tablet_tool", NULL },
		{ LIBINPUT_DEVICE_CAP_TABLET_PAD, "tablet_pad", NULL },
		{ LIBINPUT_DEVICE_CAP_GESTURE, "gesture", NULL },
#ifdef LIBINPUT_DEVICE_CAP_SWITCH // libinput 1.7.0+
		{ LIBINPUT_DEVICE_CAP_SWITCH, "switch", NULL },
#endif		
	};

	json_object *_caps = json_object_new_array();
	for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i) {
		if (libinput_device_has_capability(device, caps[i].cap)) {
			json_object_array_add(_caps, json_object_new_string(caps[i].name));
			if (caps[i].describe) {
				json_object *desc = caps[i].describe(device);
				json_object_object_add(device_object, caps[i].name, desc);
			}
		}
	}
	json_object_object_add(device_object, "capabilities", _caps);

	free(identifier);
	return device_object;
}

json_object *ipc_json_get_version() {
	int major = 0, minor = 0, patch = 0;
	json_object *version = json_object_new_object();

	sscanf(SWAY_VERSION, "%u.%u.%u", &major, &minor, &patch);

	json_object_object_add(version, "human_readable", json_object_new_string(SWAY_VERSION));
	json_object_object_add(version, "variant", json_object_new_string("sway"));
	json_object_object_add(version, "major", json_object_new_int(major));
	json_object_object_add(version, "minor", json_object_new_int(minor));
	json_object_object_add(version, "patch", json_object_new_int(patch));

	return version;
}

json_object *ipc_json_describe_bar_config(struct bar_config *bar) {
	if (!sway_assert(bar, "Bar must not be NULL")) {
		return NULL;
	}

	json_object *json = json_object_new_object();
	json_object_object_add(json, "id", json_object_new_string(bar->id));
#ifdef ENABLE_TRAY
	if (bar->tray_output) {
		json_object_object_add(json, "tray_output", json_object_new_string(bar->tray_output));
	} else {
		json_object_object_add(json, "tray_output", NULL);
	}
	if (bar->icon_theme) {
		json_object_object_add(json, "icon_theme", json_object_new_string(bar->icon_theme));
	} else {
		json_object_object_add(json, "icon_theme", NULL);
	}
	json_object_object_add(json, "tray_padding", json_object_new_int(bar->tray_padding));
	json_object_object_add(json, "activate_button", json_object_new_int(bar->activate_button));
	json_object_object_add(json, "context_button", json_object_new_int(bar->context_button));
	json_object_object_add(json, "secondary_button", json_object_new_int(bar->secondary_button));
#endif
	json_object_object_add(json, "mode", json_object_new_string(bar->mode));
	json_object_object_add(json, "hidden_state", json_object_new_string(bar->hidden_state));
	json_object_object_add(json, "modifier", json_object_new_string(get_modifier_name_by_mask(bar->modifier)));
	switch (bar->position) {
	case DESKTOP_SHELL_PANEL_POSITION_TOP:
		json_object_object_add(json, "position", json_object_new_string("top"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
		json_object_object_add(json, "position", json_object_new_string("bottom"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_LEFT:
		json_object_object_add(json, "position", json_object_new_string("left"));
		break;
	case DESKTOP_SHELL_PANEL_POSITION_RIGHT:
		json_object_object_add(json, "position", json_object_new_string("right"));
		break;
	}
	json_object_object_add(json, "status_command", json_object_new_string(bar->status_command));
	json_object_object_add(json, "font", json_object_new_string((bar->font) ? bar->font : config->font));
	if (bar->separator_symbol) {
		json_object_object_add(json, "separator_symbol", json_object_new_string(bar->separator_symbol));
	}
	json_object_object_add(json, "bar_height", json_object_new_int(bar->height));
	json_object_object_add(json, "wrap_scroll", json_object_new_boolean(bar->wrap_scroll));
	json_object_object_add(json, "workspace_buttons", json_object_new_boolean(bar->workspace_buttons));
	json_object_object_add(json, "strip_workspace_numbers", json_object_new_boolean(bar->strip_workspace_numbers));
	json_object_object_add(json, "binding_mode_indicator", json_object_new_boolean(bar->binding_mode_indicator));
	json_object_object_add(json, "verbose", json_object_new_boolean(bar->verbose));
	json_object_object_add(json, "pango_markup", json_object_new_boolean(bar->pango_markup));

	json_object *colors = json_object_new_object();
	json_object_object_add(colors, "background", json_object_new_string(bar->colors.background));
	json_object_object_add(colors, "statusline", json_object_new_string(bar->colors.statusline));
	json_object_object_add(colors, "separator", json_object_new_string(bar->colors.separator));

	if (bar->colors.focused_background) {
		json_object_object_add(colors, "focused_background", json_object_new_string(bar->colors.focused_background));
	} else {
		json_object_object_add(colors, "focused_background", json_object_new_string(bar->colors.background));
	}

	if (bar->colors.focused_statusline) {
		json_object_object_add(colors, "focused_statusline", json_object_new_string(bar->colors.focused_statusline));
	} else {
		json_object_object_add(colors, "focused_statusline", json_object_new_string(bar->colors.statusline));
	}

	if (bar->colors.focused_separator) {
		json_object_object_add(colors, "focused_separator", json_object_new_string(bar->colors.focused_separator));
	} else {
		json_object_object_add(colors, "focused_separator", json_object_new_string(bar->colors.separator));
	}

	json_object_object_add(colors, "focused_workspace_border", json_object_new_string(bar->colors.focused_workspace_border));
	json_object_object_add(colors, "focused_workspace_bg", json_object_new_string(bar->colors.focused_workspace_bg));
	json_object_object_add(colors, "focused_workspace_text", json_object_new_string(bar->colors.focused_workspace_text));

	json_object_object_add(colors, "inactive_workspace_border", json_object_new_string(bar->colors.inactive_workspace_border));
	json_object_object_add(colors, "inactive_workspace_bg", json_object_new_string(bar->colors.inactive_workspace_bg));
	json_object_object_add(colors, "inactive_workspace_text", json_object_new_string(bar->colors.inactive_workspace_text));

	json_object_object_add(colors, "active_workspace_border", json_object_new_string(bar->colors.active_workspace_border));
	json_object_object_add(colors, "active_workspace_bg", json_object_new_string(bar->colors.active_workspace_bg));
	json_object_object_add(colors, "active_workspace_text", json_object_new_string(bar->colors.active_workspace_text));

	json_object_object_add(colors, "urgent_workspace_border", json_object_new_string(bar->colors.urgent_workspace_border));
	json_object_object_add(colors, "urgent_workspace_bg", json_object_new_string(bar->colors.urgent_workspace_bg));
	json_object_object_add(colors, "urgent_workspace_text", json_object_new_string(bar->colors.urgent_workspace_text));

	if (bar->colors.binding_mode_border) {
		json_object_object_add(colors, "binding_mode_border", json_object_new_string(bar->colors.binding_mode_border));
	} else {
		json_object_object_add(colors, "binding_mode_border", json_object_new_string(bar->colors.urgent_workspace_border));
	}

	if (bar->colors.binding_mode_bg) {
		json_object_object_add(colors, "binding_mode_bg", json_object_new_string(bar->colors.binding_mode_bg));
	} else {
		json_object_object_add(colors, "binding_mode_bg", json_object_new_string(bar->colors.urgent_workspace_bg));
	}

	if (bar->colors.binding_mode_text) {
		json_object_object_add(colors, "binding_mode_text", json_object_new_string(bar->colors.binding_mode_text));
	} else {
		json_object_object_add(colors, "binding_mode_text", json_object_new_string(bar->colors.urgent_workspace_text));
	}

	json_object_object_add(json, "colors", colors);

	// Add outputs if defined
	if (bar->outputs && bar->outputs->length > 0) {
		json_object *outputs = json_object_new_array();
		int i;
		for (i = 0; i < bar->outputs->length; ++i) {
			const char *name = bar->outputs->items[i];
			json_object_array_add(outputs, json_object_new_string(name));
		}
		json_object_object_add(json, "outputs", outputs);
	}

	return json;
}

json_object *ipc_json_describe_container_recursive(swayc_t *c) {
	json_object *object = ipc_json_describe_container(c);
	int i;

	json_object *floating = json_object_new_array();
	if (c->type != C_VIEW && c->floating) {
		for (i = 0; i < c->floating->length; ++i) {
			swayc_t *item = c->floating->items[i];
			json_object_array_add(floating, ipc_json_describe_container_recursive(item));
		}
	}
	json_object_object_add(object, "floating_nodes", floating);

	json_object *children = json_object_new_array();
	if (c->type != C_VIEW && c->children) {
		for (i = 0; i < c->children->length; ++i) {
			json_object_array_add(children, ipc_json_describe_container_recursive(c->children->items[i]));
		}
	}
	json_object_object_add(object, "nodes", children);

	json_object *focus = json_object_new_array();
	if (c->type != C_VIEW) {
		if (c->focused) {
			json_object_array_add(focus, json_object_new_double(c->focused->id));
		}
		if (c->floating) {
			for (i = 0; i < c->floating->length; ++i) {
				swayc_t *item = c->floating->items[i];
				if (item == c->focused) {
					continue;
				}

				json_object_array_add(focus, json_object_new_double(item->id));
			}
		}
		if (c->children) {
			for (i = 0; i < c->children->length; ++i) {
				swayc_t *item = c->children->items[i];
				if (item == c->focused) {
					continue;
				}

				json_object_array_add(focus, json_object_new_double(item->id));
			}
		}
	}
	json_object_object_add(object, "focus", focus);

	if (c->type == C_ROOT) {
		json_object *scratchpad_json = json_object_new_array();
		if (scratchpad->length > 0) {
			for (i = 0; i < scratchpad->length; ++i) {
				json_object_array_add(scratchpad_json, ipc_json_describe_container_recursive(scratchpad->items[i]));
			}
		}
		json_object_object_add(object, "scratchpad", scratchpad_json);
	}

	return object;
}
