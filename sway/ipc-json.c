#include <json-c/json.h>
#include <stdio.h>
#include <ctype.h>
#include "log.h"
#include "sway/config.h"
#include "sway/ipc-json.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"
#include "sway/output.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include "wlr-layer-shell-unstable-v1-protocol.h"

static const char *ipc_json_layout_description(enum sway_container_layout l) {
	switch (l) {
	case L_VERT:
		return "splitv";
	case L_HORIZ:
		return "splith";
	case L_TABBED:
		return "tabbed";
	case L_STACKED:
		return "stacked";
	case L_FLOATING:
		return "floating";
	case L_NONE:
		break;
	}
	return "none";
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
	json_object_object_add(version, "loaded_config_file_name", json_object_new_string(config->current_config_path));

	return version;
}

static json_object *ipc_json_create_rect(struct sway_container *c) {
	json_object *rect = json_object_new_object();

	json_object_object_add(rect, "x", json_object_new_int((int32_t)c->x));
	json_object_object_add(rect, "y", json_object_new_int((int32_t)c->y));
	json_object_object_add(rect, "width", json_object_new_int((int32_t)c->width));
	json_object_object_add(rect, "height", json_object_new_int((int32_t)c->height));

	return rect;
}

static void ipc_json_describe_root(struct sway_container *root, json_object *object) {
	json_object_object_add(object, "type", json_object_new_string("root"));
	json_object_object_add(object, "layout", json_object_new_string("splith"));
}

static const char *ipc_json_get_output_transform(enum wl_output_transform transform) {
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		return "normal";
	case WL_OUTPUT_TRANSFORM_90:
		return "90";
	case WL_OUTPUT_TRANSFORM_180:
		return "180";
	case WL_OUTPUT_TRANSFORM_270:
		return "270";
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		return "flipped";
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		return "flipped-90";
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		return "flipped-180";
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		return "flipped-270";
	}
	return NULL;
}

static void ipc_json_describe_output(struct sway_container *container,
		json_object *object) {
	struct wlr_output *wlr_output = container->sway_output->wlr_output;
	json_object_object_add(object, "type",
			json_object_new_string("output"));
	json_object_object_add(object, "active",
			json_object_new_boolean(true));
	json_object_object_add(object, "primary",
			json_object_new_boolean(false));
	json_object_object_add(object, "layout",
			json_object_new_string("output"));
	json_object_object_add(object, "make",
			json_object_new_string(wlr_output->make));
	json_object_object_add(object, "model",
			json_object_new_string(wlr_output->model));
	json_object_object_add(object, "serial",
			json_object_new_string(wlr_output->serial));
	json_object_object_add(object, "scale",
			json_object_new_double(wlr_output->scale));
	json_object_object_add(object, "refresh",
			json_object_new_int(wlr_output->refresh));
	json_object_object_add(object, "transform",
		json_object_new_string(
			ipc_json_get_output_transform(wlr_output->transform)));

	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	const char *ws = NULL;
	if (seat) {
		struct sway_container *focus =
			seat_get_focus_inactive(seat, container);
		if (focus && focus->type != C_WORKSPACE) {
			focus = container_parent(focus, C_WORKSPACE);
		}
		if (focus) {
			ws = focus->name;
		}
	}
	json_object_object_add(object, "current_workspace",
			json_object_new_string(ws));

	json_object *modes_array = json_object_new_array();
	struct wlr_output_mode *mode;
	wl_list_for_each(mode, &wlr_output->modes, link) {
		json_object *mode_object = json_object_new_object();
		json_object_object_add(mode_object, "width",
			json_object_new_int(mode->width));
		json_object_object_add(mode_object, "height",
			json_object_new_int(mode->height));
		json_object_object_add(mode_object, "refresh",
			json_object_new_int(mode->refresh));
		json_object_array_add(modes_array, mode_object);
	}

	json_object_object_add(object, "modes", modes_array);
	json_object_object_add(object, "layout", json_object_new_string("output"));
}

json_object *ipc_json_describe_disabled_output(struct sway_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;

	json_object *object = json_object_new_object();

	json_object_object_add(object, "type", json_object_new_string("output"));
	json_object_object_add(object, "name",
			json_object_new_string(wlr_output->name));
	json_object_object_add(object, "active", json_object_new_boolean(false));
	json_object_object_add(object, "make",
			json_object_new_string(wlr_output->make));
	json_object_object_add(object, "model",
			json_object_new_string(wlr_output->model));
	json_object_object_add(object, "serial",
			json_object_new_string(wlr_output->serial));
	json_object_object_add(object, "modes", json_object_new_array());

	return object;
}

static void ipc_json_describe_workspace(struct sway_container *workspace,
		json_object *object) {
	int num = isdigit(workspace->name[0]) ? atoi(workspace->name) : -1;

	json_object_object_add(object, "num", json_object_new_int(num));
	json_object_object_add(object, "output", workspace->parent ?
			json_object_new_string(workspace->parent->name) : NULL);
	json_object_object_add(object, "type", json_object_new_string("workspace"));
	json_object_object_add(object, "urgent",
			json_object_new_boolean(workspace_is_urgent(workspace)));
	json_object_object_add(object, "representation", workspace->formatted_title ?
			json_object_new_string(workspace->formatted_title) : NULL);

	const char *layout = ipc_json_layout_description(workspace->layout);
	json_object_object_add(object, "layout", json_object_new_string(layout));

	// Floating
	json_object *floating_array = json_object_new_array();
	struct sway_container *floating = workspace->sway_workspace->floating;
	for (int i = 0; i < floating->children->length; ++i) {
		struct sway_container *floater = floating->children->items[i];
		json_object_array_add(floating_array, ipc_json_describe_container_recursive(floater));
	}
	json_object_object_add(object, "floating_nodes", floating_array);
}

static void ipc_json_describe_view(struct sway_container *c, json_object *object) {
	json_object_object_add(object, "name",
			c->name ? json_object_new_string(c->name) : NULL);
	json_object_object_add(object, "type", json_object_new_string("con"));

	if (c->parent) {
		json_object_object_add(object, "layout",
			json_object_new_string(ipc_json_layout_description(c->layout)));
	}

	json_object_object_add(object, "urgent",
			json_object_new_boolean(view_is_urgent(c->sway_view)));
}

static void focus_inactive_children_iterator(struct sway_container *c, void *data) {
	json_object *focus = data;
	json_object_array_add(focus, json_object_new_int(c->id));
}

json_object *ipc_json_describe_container(struct sway_container *c) {
	if (!(sway_assert(c, "Container must not be null."))) {
		return NULL;
	}

	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	bool focused = seat_get_focus(seat) == c;

	json_object *object = json_object_new_object();

	json_object_object_add(object, "id", json_object_new_int((int)c->id));
	json_object_object_add(object, "name",
			c->name ? json_object_new_string(c->name) : NULL);
	json_object_object_add(object, "rect", ipc_json_create_rect(c));
	json_object_object_add(object, "focused",
			json_object_new_boolean(focused));

	json_object *focus = json_object_new_array();
	seat_focus_inactive_children_for_each(seat, c,
		focus_inactive_children_iterator, focus);
	json_object_object_add(object, "focus", focus);

	switch (c->type) {
	case C_ROOT:
		ipc_json_describe_root(c, object);
		break;
	case C_OUTPUT:
		ipc_json_describe_output(c, object);
		break;
	case C_CONTAINER:
	case C_VIEW:
		ipc_json_describe_view(c, object);
		break;
	case C_WORKSPACE:
		ipc_json_describe_workspace(c, object);
		break;
	case C_TYPES:
	default:
		break;
	}

	return object;
}

json_object *ipc_json_describe_container_recursive(struct sway_container *c) {
	json_object *object = ipc_json_describe_container(c);
	int i;

	json_object *children = json_object_new_array();
	if (c->type != C_VIEW && c->children) {
		for (i = 0; i < c->children->length; ++i) {
			json_object_array_add(children, ipc_json_describe_container_recursive(c->children->items[i]));
		}
	}
	json_object_object_add(object, "nodes", children);

	return object;
}

static const char *describe_device_type(struct sway_input_device *device) {
	switch (device->wlr_device->type) {
	case WLR_INPUT_DEVICE_POINTER:
		return "pointer";
	case WLR_INPUT_DEVICE_KEYBOARD:
		return "keyboard";
	case WLR_INPUT_DEVICE_TOUCH:
		return "touch";
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		return "tablet_tool";
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return "tablet_pad";
	}
	return "unknown";
}

json_object *ipc_json_describe_input(struct sway_input_device *device) {
	if (!(sway_assert(device, "Device must not be null"))) {
		return NULL;
	}

	json_object *object = json_object_new_object();

	json_object_object_add(object, "identifier",
		json_object_new_string(device->identifier));
	json_object_object_add(object, "name",
		json_object_new_string(device->wlr_device->name));
	json_object_object_add(object, "vendor",
		json_object_new_int(device->wlr_device->vendor));
	json_object_object_add(object, "product",
		json_object_new_int(device->wlr_device->product));
	json_object_object_add(object, "type",
		json_object_new_string(describe_device_type(device)));

	return object;
}

json_object *ipc_json_describe_seat(struct sway_seat *seat) {
	if (!(sway_assert(seat, "Seat must not be null"))) {
		return NULL;
	}

	json_object *object = json_object_new_object();
	struct sway_container *focus = seat_get_focus(seat);

	json_object_object_add(object, "name",
		json_object_new_string(seat->wlr_seat->name));
	json_object_object_add(object, "capabilities",
		json_object_new_int(seat->wlr_seat->capabilities));
	json_object_object_add(object, "focus",
		json_object_new_int(focus ? focus->id : 0));

	json_object *devices = json_object_new_array();
	struct sway_seat_device *device = NULL;
	wl_list_for_each(device, &seat->devices, link) {
		json_object_array_add(devices, ipc_json_describe_input(device->input_device));
	}
	json_object_object_add(object, "devices", devices);

	return object;
}

json_object *ipc_json_describe_bar_config(struct bar_config *bar) {
	if (!sway_assert(bar, "Bar must not be NULL")) {
		return NULL;
	}

	json_object *json = json_object_new_object();
	json_object_object_add(json, "id", json_object_new_string(bar->id));
	json_object_object_add(json, "mode", json_object_new_string(bar->mode));
	json_object_object_add(json, "hidden_state",
			json_object_new_string(bar->hidden_state));
	json_object_object_add(json, "position",
			json_object_new_string(bar->position));
	json_object_object_add(json, "status_command",
			json_object_new_string(bar->status_command));
	json_object_object_add(json, "font",
			json_object_new_string((bar->font) ? bar->font : config->font));
	if (bar->separator_symbol) {
		json_object_object_add(json, "separator_symbol",
				json_object_new_string(bar->separator_symbol));
	}
	json_object_object_add(json, "bar_height",
			json_object_new_int(bar->height));
	json_object_object_add(json, "wrap_scroll",
			json_object_new_boolean(bar->wrap_scroll));
	json_object_object_add(json, "workspace_buttons",
			json_object_new_boolean(bar->workspace_buttons));
	json_object_object_add(json, "strip_workspace_numbers",
			json_object_new_boolean(bar->strip_workspace_numbers));
	json_object_object_add(json, "binding_mode_indicator",
			json_object_new_boolean(bar->binding_mode_indicator));
	json_object_object_add(json, "verbose",
			json_object_new_boolean(bar->verbose));
	json_object_object_add(json, "pango_markup",
			json_object_new_boolean(bar->pango_markup));

	json_object *colors = json_object_new_object();
	json_object_object_add(colors, "background",
			json_object_new_string(bar->colors.background));
	json_object_object_add(colors, "statusline",
			json_object_new_string(bar->colors.statusline));
	json_object_object_add(colors, "separator",
			json_object_new_string(bar->colors.separator));

	if (bar->colors.focused_background) {
		json_object_object_add(colors, "focused_background",
				json_object_new_string(bar->colors.focused_background));
	} else {
		json_object_object_add(colors, "focused_background",
				json_object_new_string(bar->colors.background));
	}

	if (bar->colors.focused_statusline) {
		json_object_object_add(colors, "focused_statusline",
				json_object_new_string(bar->colors.focused_statusline));
	} else {
		json_object_object_add(colors, "focused_statusline",
				json_object_new_string(bar->colors.statusline));
	}

	if (bar->colors.focused_separator) {
		json_object_object_add(colors, "focused_separator",
				json_object_new_string(bar->colors.focused_separator));
	} else {
		json_object_object_add(colors, "focused_separator",
				json_object_new_string(bar->colors.separator));
	}

	json_object_object_add(colors, "focused_workspace_border",
			json_object_new_string(bar->colors.focused_workspace_border));
	json_object_object_add(colors, "focused_workspace_bg",
			json_object_new_string(bar->colors.focused_workspace_bg));
	json_object_object_add(colors, "focused_workspace_text",
			json_object_new_string(bar->colors.focused_workspace_text));

	json_object_object_add(colors, "inactive_workspace_border",
			json_object_new_string(bar->colors.inactive_workspace_border));
	json_object_object_add(colors, "inactive_workspace_bg",
			json_object_new_string(bar->colors.inactive_workspace_bg));
	json_object_object_add(colors, "inactive_workspace_text",
			json_object_new_string(bar->colors.inactive_workspace_text));

	json_object_object_add(colors, "active_workspace_border",
			json_object_new_string(bar->colors.active_workspace_border));
	json_object_object_add(colors, "active_workspace_bg",
			json_object_new_string(bar->colors.active_workspace_bg));
	json_object_object_add(colors, "active_workspace_text",
			json_object_new_string(bar->colors.active_workspace_text));

	json_object_object_add(colors, "urgent_workspace_border",
			json_object_new_string(bar->colors.urgent_workspace_border));
	json_object_object_add(colors, "urgent_workspace_bg",
			json_object_new_string(bar->colors.urgent_workspace_bg));
	json_object_object_add(colors, "urgent_workspace_text",
			json_object_new_string(bar->colors.urgent_workspace_text));

	if (bar->colors.binding_mode_border) {
		json_object_object_add(colors, "binding_mode_border",
				json_object_new_string(bar->colors.binding_mode_border));
	} else {
		json_object_object_add(colors, "binding_mode_border",
				json_object_new_string(bar->colors.urgent_workspace_border));
	}

	if (bar->colors.binding_mode_bg) {
		json_object_object_add(colors, "binding_mode_bg",
				json_object_new_string(bar->colors.binding_mode_bg));
	} else {
		json_object_object_add(colors, "binding_mode_bg",
				json_object_new_string(bar->colors.urgent_workspace_bg));
	}

	if (bar->colors.binding_mode_text) {
		json_object_object_add(colors, "binding_mode_text",
				json_object_new_string(bar->colors.binding_mode_text));
	} else {
		json_object_object_add(colors, "binding_mode_text",
				json_object_new_string(bar->colors.urgent_workspace_text));
	}

	json_object_object_add(json, "colors", colors);

	// Add outputs if defined
	if (bar->outputs && bar->outputs->length > 0) {
		json_object *outputs = json_object_new_array();
		for (int i = 0; i < bar->outputs->length; ++i) {
			const char *name = bar->outputs->items[i];
			json_object_array_add(outputs, json_object_new_string(name));
		}
		json_object_object_add(json, "outputs", outputs);
	}
	return json;
}
