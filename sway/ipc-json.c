#include <json-c/json.h>
#include <stdio.h>
#include <ctype.h>
#include "log.h"
#include "sway/config.h"
#include "sway/ipc-json.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
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
	case L_NONE:
		break;
	}
	return "none";
}

static const char *ipc_json_orientation_description(enum sway_container_layout l) {
	if (l == L_VERT) {
		return "vertical";
	}

	if (l == L_HORIZ) {
		return "horizontal";
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

static json_object *ipc_json_create_rect(struct wlr_box *box) {
	json_object *rect = json_object_new_object();

	json_object_object_add(rect, "x", json_object_new_int(box->x));
	json_object_object_add(rect, "y", json_object_new_int(box->y));
	json_object_object_add(rect, "width", json_object_new_int(box->width));
	json_object_object_add(rect, "height", json_object_new_int(box->height));

	return rect;
}

static json_object *ipc_json_create_empty_rect(void) {
	struct wlr_box empty = {0, 0, 0, 0};

	return ipc_json_create_rect(&empty);
}

static void ipc_json_describe_root(struct sway_root *root, json_object *object) {
	json_object_object_add(object, "type", json_object_new_string("root"));
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

static void ipc_json_describe_output(struct sway_output *output,
		json_object *object) {
	struct wlr_output *wlr_output = output->wlr_output;
	json_object_object_add(object, "type", json_object_new_string("output"));
	json_object_object_add(object, "active", json_object_new_boolean(true));
	json_object_object_add(object, "primary", json_object_new_boolean(false));
	json_object_object_add(object, "layout", json_object_new_string("output"));
	json_object_object_add(object, "orientation",
			json_object_new_string(ipc_json_orientation_description(L_NONE)));
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

	struct sway_workspace *ws = output_get_active_workspace(output);
	json_object_object_add(object, "current_workspace",
			json_object_new_string(ws->name));

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

	struct sway_node *parent = node_get_parent(&output->node);
	struct wlr_box parent_box = {0, 0, 0, 0};

	if (parent != NULL) {
		node_get_box(parent, &parent_box);
	}

	if (parent_box.width != 0 && parent_box.height != 0) {
		double percent = ((double)output->width / parent_box.width)
				* ((double)output->height / parent_box.height);
		json_object_object_add(object, "percent", json_object_new_double(percent));
	}
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

	json_object_object_add(object, "percent", NULL);

	return object;
}

static void ipc_json_describe_workspace(struct sway_workspace *workspace,
		json_object *object) {
	int num = isdigit(workspace->name[0]) ? atoi(workspace->name) : -1;

	json_object_object_add(object, "num", json_object_new_int(num));
	json_object_object_add(object, "output", workspace->output ?
			json_object_new_string(workspace->output->wlr_output->name) : NULL);
	json_object_object_add(object, "type", json_object_new_string("workspace"));
	json_object_object_add(object, "urgent",
			json_object_new_boolean(workspace->urgent));
	json_object_object_add(object, "representation", workspace->representation ?
			json_object_new_string(workspace->representation) : NULL);

	const char *layout = ipc_json_layout_description(workspace->layout);
	json_object_object_add(object, "layout", json_object_new_string(layout));

	const char *orientation = ipc_json_orientation_description(workspace->layout);
	json_object_object_add(object, "orientation", json_object_new_string(orientation));

	// Floating
	json_object *floating_array = json_object_new_array();
	for (int i = 0; i < workspace->floating->length; ++i) {
		struct sway_container *floater = workspace->floating->items[i];
		json_object_array_add(floating_array,
				ipc_json_describe_node_recursive(&floater->node));
	}
	json_object_object_add(object, "floating_nodes", floating_array);
}

static const char *describe_container_border(enum sway_container_border border) {
	switch (border) {
	case B_NONE:
		return "none";
	case B_PIXEL:
		return "pixel";
	case B_NORMAL:
		return "normal";
	}
	return "unknown";
}

static void ipc_json_describe_view(struct sway_container *c, json_object *object) {
	const char *app_id = view_get_app_id(c->view);
	json_object_object_add(object, "app_id",
			app_id ? json_object_new_string(app_id) : NULL);

	const char *class = view_get_class(c->view);
	json_object_object_add(object, "class",
			class ? json_object_new_string(class) : NULL);

	json_object *marks = json_object_new_array();
	list_t *view_marks = c->view->marks;
	for (int i = 0; i < view_marks->length; ++i) {
		json_object_array_add(marks, json_object_new_string(view_marks->items[i]));
	}

	json_object_object_add(object, "marks", marks);

	struct wlr_box window_box = {
		c->view->x - c->x,
		(c->current.border == B_PIXEL) ? c->current.border_thickness : 0,
		c->view->width,
		c->view->height
	};

	json_object_object_add(object, "window_rect", ipc_json_create_rect(&window_box));

	struct wlr_box deco_box = {0, 0, 0, 0};

	if (c->current.border == B_NORMAL) {
		deco_box.width = c->width;
		deco_box.height = c->view->y - c->y;
	}

	json_object_object_add(object, "deco_rect", ipc_json_create_rect(&deco_box));

	struct wlr_box geometry = {0, 0, c->view->natural_width, c->view->natural_height};
	json_object_object_add(object, "geometry", ipc_json_create_rect(&geometry));

#ifdef HAVE_XWAYLAND
	if (c->view->type == SWAY_VIEW_XWAYLAND) {
		json_object_object_add(object, "window",
				json_object_new_int(view_get_x11_window_id(c->view)));
	}
#endif
}

static void ipc_json_describe_container(struct sway_container *c, json_object *object) {
	json_object_object_add(object, "name",
			c->title ? json_object_new_string(c->title) : NULL);
	json_object_object_add(object, "type", json_object_new_string("con"));

	json_object_object_add(object, "layout",
		json_object_new_string(ipc_json_layout_description(c->layout)));

	json_object_object_add(object, "orientation",
			json_object_new_string(ipc_json_orientation_description(c->layout)));

	bool urgent = c->view ?
		view_is_urgent(c->view) : container_has_urgent_child(c);
	json_object_object_add(object, "urgent", json_object_new_boolean(urgent));
	json_object_object_add(object, "sticky", json_object_new_boolean(c->is_sticky));

	struct sway_node *parent = node_get_parent(&c->node);
	struct wlr_box parent_box = {0, 0, 0, 0};

	if (parent != NULL) {
		node_get_box(parent, &parent_box);
	}

	if (parent_box.width != 0 && parent_box.height != 0) {
		double percent = ((double)c->width / parent_box.width)
				* ((double)c->height / parent_box.height);
		json_object_object_add(object, "percent", json_object_new_double(percent));
	}

	json_object_object_add(object, "border",
			json_object_new_string(describe_container_border(c->current.border)));
	json_object_object_add(object, "current_border_width",
			json_object_new_int(c->current.border_thickness));
	json_object_object_add(object, "floating_nodes", json_object_new_array());

	if (c->view) {
		ipc_json_describe_view(c, object);
	}
}

struct focus_inactive_data {
	struct sway_node *node;
	json_object *object;
};

static void focus_inactive_children_iterator(struct sway_node *node,
		void *_data) {
	struct focus_inactive_data *data = _data;
	json_object *focus = data->object;
	if (data->node == &root->node) {
		struct sway_output *output = node_get_output(node);
		if (output == NULL) {
			return;
		}
		size_t id = output->node.id;
		int len = json_object_array_length(focus);
		for (int i = 0; i < len; ++i) {
			if ((size_t) json_object_get_int(json_object_array_get_idx(focus, i)) == id) {
				return;
			}
		}
		node = &output->node;
	} else if (node_get_parent(node) != data->node) {
		return;
	}
	json_object_array_add(focus, json_object_new_int(node->id));
}

json_object *ipc_json_describe_node(struct sway_node *node) {
	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	bool focused = seat_get_focus(seat) == node;

	json_object *object = json_object_new_object();
	char *name = node_get_name(node);

	struct wlr_box box;
	node_get_box(node, &box);
	json_object_object_add(object, "id", json_object_new_int((int)node->id));
	json_object_object_add(object, "name",
			name ? json_object_new_string(name) : NULL);
	json_object_object_add(object, "rect", ipc_json_create_rect(&box));
	json_object_object_add(object, "focused", json_object_new_boolean(focused));

	json_object *focus = json_object_new_array();
	struct focus_inactive_data data = {
		.node = node,
		.object = focus,
	};
	seat_for_each_node(seat, focus_inactive_children_iterator, &data);
	json_object_object_add(object, "focus", focus);

	// set default values to be compatible with i3
	json_object_object_add(object, "border",
			json_object_new_string(describe_container_border(B_NONE)));
	json_object_object_add(object, "current_border_width", json_object_new_int(0));
	json_object_object_add(object, "layout",
			json_object_new_string(ipc_json_layout_description(L_HORIZ)));
	json_object_object_add(object, "orientation",
			json_object_new_string(ipc_json_orientation_description(L_HORIZ)));
	json_object_object_add(object, "percent", NULL);
	json_object_object_add(object, "window_rect", ipc_json_create_empty_rect());
	json_object_object_add(object, "deco_rect", ipc_json_create_empty_rect());
	json_object_object_add(object, "geometry", ipc_json_create_empty_rect());
	json_object_object_add(object, "window", NULL);
	json_object_object_add(object, "urgent", json_object_new_boolean(false));
	json_object_object_add(object, "floating_nodes", json_object_new_array());
	json_object_object_add(object, "sticky", json_object_new_boolean(false));

	switch (node->type) {
	case N_ROOT:
		ipc_json_describe_root(root, object);
		break;
	case N_OUTPUT:
		ipc_json_describe_output(node->sway_output, object);
		break;
	case N_CONTAINER:
		ipc_json_describe_container(node->sway_container, object);
		break;
	case N_WORKSPACE:
		ipc_json_describe_workspace(node->sway_workspace, object);
		break;
	}

	return object;
}

json_object *ipc_json_describe_node_recursive(struct sway_node *node) {
	json_object *object = ipc_json_describe_node(node);
	int i;

	json_object *children = json_object_new_array();
	switch (node->type) {
	case N_ROOT:
		for (i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			json_object_array_add(children,
					ipc_json_describe_node_recursive(&output->node));
		}
		break;
	case N_OUTPUT:
		for (i = 0; i < node->sway_output->workspaces->length; ++i) {
			struct sway_workspace *ws = node->sway_output->workspaces->items[i];
			json_object_array_add(children,
					ipc_json_describe_node_recursive(&ws->node));
		}
		break;
	case N_WORKSPACE:
		for (i = 0; i < node->sway_workspace->tiling->length; ++i) {
			struct sway_container *con = node->sway_workspace->tiling->items[i];
			json_object_array_add(children,
					ipc_json_describe_node_recursive(&con->node));
		}
		break;
	case N_CONTAINER:
		if (node->sway_container->children) {
			for (i = 0; i < node->sway_container->children->length; ++i) {
				struct sway_container *child =
					node->sway_container->children->items[i];
				json_object_array_add(children,
						ipc_json_describe_node_recursive(&child->node));
			}
		}
		break;
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
	struct sway_node *focus = seat_get_focus(seat);

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
