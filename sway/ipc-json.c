#include <json-c/json.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include "container.h"
#include "util.h"
#include "ipc-json.h"

static json_object *ipc_json_create_rect(swayc_t *c) {
	json_object *rect = json_object_new_object();

	json_object_object_add(rect, "x", json_object_new_int((int32_t)c->x));
	json_object_object_add(rect, "y", json_object_new_int((int32_t)c->y));
	json_object_object_add(rect, "width", json_object_new_int((int32_t)c->width));
	json_object_object_add(rect, "height", json_object_new_int((int32_t)c->height));

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
	json_object_object_add(object, "active", json_object_new_boolean(true));
	json_object_object_add(object, "primary", json_object_new_boolean(false));
	json_object_object_add(object, "layout", json_object_new_string("output"));
	json_object_object_add(object, "type", json_object_new_string("output"));
	json_object_object_add(object, "current_workspace",
		(output->focused) ? json_object_new_string(output->focused->name) : NULL);
}

static void ipc_json_describe_workspace(swayc_t *workspace, json_object *object) {
	int num = (isdigit(workspace->name[0])) ? atoi(workspace->name) : -1;
	bool focused = root_container.focused == workspace->parent && workspace->parent->focused == workspace;
	const char *layout = ipc_json_layout_description(workspace->layout);

	json_object_object_add(object, "num", json_object_new_int(num));
	json_object_object_add(object, "focused", json_object_new_boolean(focused));
	json_object_object_add(object, "output", (workspace->parent) ? json_object_new_string(workspace->parent->name) : NULL);
	json_object_object_add(object, "urgent", json_object_new_boolean(false));
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
	float percent = ipc_json_child_percentage(c);
	const char *layout = (c->parent->type == C_CONTAINER) ?
		ipc_json_layout_description(c->parent->layout) : "none";
	const char *last_layout = (c->parent->type == C_CONTAINER) ?
		ipc_json_layout_description(c->parent->prev_layout) : "none";
	wlc_handle parent = wlc_view_get_parent(c->handle);

	json_object_object_add(object, "id", json_object_new_int(c->handle));
	json_object_object_add(object, "type", json_object_new_string((c->is_floating) ? "floating_con" : "con"));

	json_object_object_add(object, "scratchpad_state",
		json_object_new_string(ipc_json_get_scratchpad_state(c)));
	json_object_object_add(object, "percent", (percent > 0) ? json_object_new_double(percent) : NULL);
	// TODO: make urgency actually work once Sway supports it
	json_object_object_add(object, "urgent", json_object_new_boolean(false));
	json_object_object_add(object, "focused", json_object_new_boolean(c->is_focused));

	json_object_object_add(object, "layout",
		(strcmp(layout, "null") == 0) ? NULL : json_object_new_string(layout));
	json_object_object_add(object, "last_split_layout",
		(strcmp(last_layout, "null") == 0) ? NULL : json_object_new_string(last_layout));
	json_object_object_add(object, "workspace_layout",
		json_object_new_string(ipc_json_layout_description(swayc_parent_by_type(c, C_WORKSPACE)->layout)));

	json_object_object_add(object, "border", json_object_new_string(ipc_json_border_description(c)));
	json_object_object_add(object, "current_border_width", json_object_new_int(c->border_thickness));

	json_object_object_add(object, "rect", ipc_json_create_rect(c));
	json_object_object_add(object, "deco_rect", ipc_json_create_rect_from_geometry(c->title_bar_geometry));
	json_object_object_add(object, "geometry", ipc_json_create_rect_from_geometry(c->cached_geometry));
	json_object_object_add(object, "window_rect", ipc_json_create_rect_from_geometry(c->actual_geometry));

	json_object_object_add(object, "name", (c->name) ? json_object_new_string(c->name) : NULL);

	json_object_object_add(object, "window", json_object_new_int(c->handle)); // for the sake of i3 compat
	json_object_object_add(props, "class", c->class ? json_object_new_string(c->class) :
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
	// we do not include children, floating, unmanaged etc. as views have none
}

json_object *ipc_json_describe_container(swayc_t *c) {
	if (!(sway_assert(c, "Container must not be null."))) {
		return NULL;
	}

	json_object *object = json_object_new_object();

	json_object_object_add(object, "id", json_object_new_int((intptr_t)&c));
	json_object_object_add(object, "name", (c->name) ? json_object_new_string(c->name) : NULL);
	json_object_object_add(object, "rect", ipc_json_create_rect(c));
	json_object_object_add(object, "visible", json_object_new_boolean(c->visible));

	switch (c->type) {
	case C_ROOT:
		json_object_object_add(object, "type", json_object_new_string("root"));
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

json_object *ipc_json_get_version() {
	json_object *version = json_object_new_object();

#if defined SWAY_GIT_VERSION && defined SWAY_GIT_BRANCH && defined SWAY_VERSION_DATE
	char *full_version = calloc(strlen(SWAY_GIT_VERSION) + strlen(SWAY_GIT_BRANCH) + strlen(SWAY_VERSION_DATE) + 20, 1);
	strcat(full_version, SWAY_GIT_VERSION);
	strcat(full_version, " (");
	strcat(full_version, SWAY_VERSION_DATE);
	strcat(full_version, ", branch \"");
	strcat(full_version, SWAY_GIT_BRANCH);
	strcat(full_version, "\")");

	json_object_object_add(version, "human_readable", json_object_new_string(full_version));
	json_object_object_add(version, "variant", json_object_new_string("sway"));
	// Todo once we actually release a version
	json_object_object_add(version, "major", json_object_new_int(0));
	json_object_object_add(version, "minor", json_object_new_int(0));
	json_object_object_add(version, "patch", json_object_new_int(1));
	free(full_version);
#else
	json_object_object_add(version, "human_readable", json_object_new_string("version not found"));
	json_object_object_add(version, "major", json_object_new_int(0));
	json_object_object_add(version, "minor", json_object_new_int(0));
	json_object_object_add(version, "patch", json_object_new_int(0));
#endif

	return version;
}

json_object *ipc_json_describe_bar_config(struct bar_config *bar) {
	if (!sway_assert(bar, "Bar must not be NULL")) {
		return NULL;
	}

	json_object *json = json_object_new_object();
	json_object_object_add(json, "id", json_object_new_string(bar->id));
	json_object_object_add(json, "tray_output", NULL);
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

	json_object_object_add(colors, "binding_mode_border", json_object_new_string(bar->colors.binding_mode_border));
	json_object_object_add(colors, "binding_mode_bg", json_object_new_string(bar->colors.binding_mode_bg));
	json_object_object_add(colors, "binding_mode_text", json_object_new_string(bar->colors.binding_mode_text));

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

	if (c->type != C_VIEW) {
		json_object *floating = json_object_new_array();
		if (c->floating && c->floating->length > 0) {
			for (i = 0; i < c->floating->length; ++i) {
				json_object_array_add(floating, ipc_json_describe_container_recursive(c->floating->items[i]));
			}
		}
		json_object_object_add(object, "floating_nodes", floating);

		json_object *children = json_object_new_array();
		if (c->children && c->children->length > 0) {
			for (i = 0; i < c->children->length; ++i) {
				json_object_array_add(children, ipc_json_describe_container_recursive(c->children->items[i]));
			}
		}
		json_object_object_add(object, "nodes", children);

		json_object *unmanaged = json_object_new_array();
		if (c->unmanaged && c->unmanaged->length > 0) {
			for (i = 0; i < c->unmanaged->length; ++i) {
				json_object_array_add(unmanaged, ipc_json_describe_container_recursive(c->unmanaged->items[i]));
			}
		}
		json_object_object_add(object, "unmanaged_nodes", unmanaged);
	}

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
