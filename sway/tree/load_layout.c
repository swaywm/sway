#include <ctype.h>
#include <errno.h>
#include <json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/criteria.h"
#include "sway/desktop/transaction.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/load_layout.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static char *slurp_file(const char *path, char **error_out) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		*error_out = format_str("append_layout: cannot open %s: %s",
				path, strerror(errno));
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		*error_out = format_str("append_layout: seek failed on %s", path);
		fclose(f);
		return NULL;
	}
	long size = ftell(f);
	if (size < 0) {
		*error_out = format_str("append_layout: ftell failed on %s", path);
		fclose(f);
		return NULL;
	}
	rewind(f);
	char *buf = malloc(size + 1);
	if (!buf) {
		*error_out = format_str("append_layout: out of memory");
		fclose(f);
		return NULL;
	}
	size_t n = fread(buf, 1, size, f);
	fclose(f);
	if ((long)n != size) {
		free(buf);
		*error_out = format_str("append_layout: short read on %s", path);
		return NULL;
	}
	buf[size] = '\0';
	return buf;
}

// i3-save-tree emits a sequence of top-level objects separated by `}\n{`
// rather than wrapping them in an array. Wrap into a strict JSON array.
// Same string-literal caveat as i3's own loader.
static char *preprocess_i3_concat(char *buf) {
	const char *p = buf;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '[' || *p != '{') {
		return buf;
	}

	size_t in_len = strlen(buf);
	size_t cap = in_len + 16;
	char *out = malloc(cap + 1);
	if (!out) {
		return buf;
	}
	size_t pos = 0;
	out[pos++] = '[';

	for (size_t i = 0; i < in_len; i++) {
		out[pos++] = buf[i];
		if (pos + 4 >= cap) {
			cap = cap * 2 + 16;
			char *grown = realloc(out, cap + 1);
			if (!grown) {
				free(out);
				return buf;
			}
			out = grown;
		}
		if (buf[i] == '}') {
			size_t j = i + 1;
			while (j < in_len && isspace((unsigned char)buf[j])) {
				j++;
			}
			if (j < in_len && buf[j] == '{') {
				out[pos++] = ',';
			}
		}
	}
	out[pos++] = ']';
	out[pos] = '\0';
	free(buf);
	return out;
}

static enum sway_container_layout parse_layout_name(const char *s) {
	if (!s) {
		return L_NONE;
	}
	if (strcasecmp(s, "splith") == 0) {
		return L_HORIZ;
	}
	if (strcasecmp(s, "splitv") == 0) {
		return L_VERT;
	}
	if (strcasecmp(s, "tabbed") == 0) {
		return L_TABBED;
	}
	// i3-save-tree emits "stacked", sway's `layout` command spells it
	// "stacking"; accept both.
	if (strcasecmp(s, "stacked") == 0 || strcasecmp(s, "stacking") == 0) {
		return L_STACKED;
	}
	return L_NONE;
}

static enum sway_container_border parse_border_name(const char *s) {
	if (!s) {
		return B_NORMAL;
	}
	if (strcasecmp(s, "none") == 0) {
		return B_NONE;
	}
	if (strcasecmp(s, "pixel") == 0) {
		return B_PIXEL;
	}
	if (strcasecmp(s, "csd") == 0) {
		return B_CSD;
	}
	return B_NORMAL;
}

// Append a key="value" fragment to a malloc'd, null-terminated buffer. The
// value is the raw regex from the swallows entry; we trust json-c to give us
// a NUL-terminated string and we do NOT escape internal quotes, i3-save-tree
// already escapes them in its output and hand-written layouts must follow the
// same rule.
static bool append_key_value(char **buf, const char *key, const char *value) {
	size_t old = *buf ? strlen(*buf) : 0;
	size_t add = strlen(key) + strlen(value) + 5; // ` k="v"`
	char *grown = realloc(*buf, old + add + 1);
	if (!grown) {
		return false;
	}
	*buf = grown;
	int written = snprintf(grown + old, add + 1, "%s%s=\"%s\"",
			old ? " " : "", key, value);
	if (written < 0) {
		return false;
	}
	return true;
}

// app_id is a sway extension over i3's swallows schema; machine is ignored.
static struct criteria *build_swallow_criteria(struct json_object *entry,
		char **error_out) {
	if (!json_object_is_type(entry, json_type_object)) {
		*error_out = format_str("append_layout: swallows entry is not an object");
		return NULL;
	}
	static const char *keys[] = {
		"class", "instance", "title", "window_role", "window_type", "app_id",
		NULL,
	};
	char *body = NULL;
	for (int i = 0; keys[i]; i++) {
		struct json_object *v;
		if (!json_object_object_get_ex(entry, keys[i], &v)) {
			continue;
		}
		if (!json_object_is_type(v, json_type_string)) {
			free(body);
			*error_out = format_str("append_layout: swallows.%s is not a string",
					keys[i]);
			return NULL;
		}
		if (!append_key_value(&body, keys[i], json_object_get_string(v))) {
			free(body);
			*error_out = format_str("append_layout: out of memory");
			return NULL;
		}
	}
	struct json_object *machine;
	if (json_object_object_get_ex(entry, "machine", &machine)) {
		sway_log(SWAY_DEBUG,
				"append_layout: ignoring 'machine' key in swallows entry");
	}
	if (!body) {
		free(body);
		*error_out = format_str("append_layout: empty swallows entry");
		return NULL;
	}
	size_t body_len = strlen(body);
	char *raw = malloc(body_len + 3);
	if (!raw) {
		free(body);
		*error_out = format_str("append_layout: out of memory");
		return NULL;
	}
	raw[0] = '[';
	memcpy(raw + 1, body, body_len);
	raw[body_len + 1] = ']';
	raw[body_len + 2] = '\0';
	free(body);

	char *parse_err = NULL;
	struct criteria *c = criteria_parse(raw, &parse_err);
	free(raw);
	if (!c) {
		*error_out = format_str("append_layout: invalid swallows pattern: %s",
				parse_err ? parse_err : "(no detail)");
		free(parse_err);
		return NULL;
	}
	return c;
}

static list_t *parse_swallows(struct json_object *arr, char **error_out) {
	if (!json_object_is_type(arr, json_type_array)) {
		*error_out = format_str("append_layout: swallows is not an array");
		return NULL;
	}
	list_t *out = create_list();
	size_t n = json_object_array_length(arr);
	for (size_t i = 0; i < n; i++) {
		struct json_object *entry = json_object_array_get_idx(arr, i);
		struct criteria *c = build_swallow_criteria(entry, error_out);
		if (!c) {
			for (int j = 0; j < out->length; j++) {
				criteria_destroy(out->items[j]);
			}
			list_free(out);
			return NULL;
		}
		list_add(out, c);
	}
	return out;
}

// Tear down a parsed subtree that we never attached to a workspace.
static void free_transient_subtree(struct sway_container *con) {
	if (!con) {
		return;
	}
	if (con->pending.children) {
		while (con->pending.children->length) {
			struct sway_container *child =
					con->pending.children->items[0];
			free_transient_subtree(child);
		}
	}
	container_begin_destroy(con);
}

static struct sway_container *build_node(struct json_object *obj,
		char **error_out);

static struct sway_container *build_node(struct json_object *obj,
		char **error_out) {
	if (!json_object_is_type(obj, json_type_object)) {
		*error_out = format_str("append_layout: node is not an object");
		return NULL;
	}

	struct sway_container *c = container_create(NULL);
	if (!c) {
		*error_out = format_str("append_layout: container_create failed");
		return NULL;
	}

	struct json_object *layout_v;
	if (json_object_object_get_ex(obj, "layout", &layout_v) &&
			json_object_is_type(layout_v, json_type_string)) {
		c->pending.layout = parse_layout_name(json_object_get_string(layout_v));
	}

	struct json_object *border_v;
	if (json_object_object_get_ex(obj, "border", &border_v) &&
			json_object_is_type(border_v, json_type_string)) {
		c->pending.border = parse_border_name(json_object_get_string(border_v));
	} else {
		c->pending.border = B_NORMAL;
	}

	struct json_object *bw_v;
	if (json_object_object_get_ex(obj, "current_border_width", &bw_v) &&
			json_object_is_type(bw_v, json_type_int)) {
		c->pending.border_thickness = json_object_get_int(bw_v);
	}

	struct json_object *percent_v;
	if (json_object_object_get_ex(obj, "percent", &percent_v)) {
		// Normalised by the next arrange pass.
		double p = json_object_get_double(percent_v);
		c->width_fraction = p;
		c->height_fraction = p;
	}

	struct json_object *name_v;
	if (json_object_object_get_ex(obj, "name", &name_v) &&
			json_object_is_type(name_v, json_type_string)) {
		const char *s = json_object_get_string(name_v);
		free(c->title);
		c->title = strdup(s ? s : "");
	}

	struct json_object *marks_v;
	if (json_object_object_get_ex(obj, "marks", &marks_v) &&
			json_object_is_type(marks_v, json_type_array)) {
		size_t n = json_object_array_length(marks_v);
		for (size_t i = 0; i < n; i++) {
			struct json_object *m = json_object_array_get_idx(marks_v, i);
			if (json_object_is_type(m, json_type_string)) {
				container_add_mark(c, (char *)json_object_get_string(m));
			}
		}
	}

	struct json_object *floating_v;
	if (json_object_object_get_ex(obj, "floating_nodes", &floating_v) &&
			json_object_is_type(floating_v, json_type_array) &&
			json_object_array_length(floating_v) > 0) {
		sway_log(SWAY_DEBUG, "append_layout: skipping floating_nodes "
				"(tiling-only support in this release)");
	}

	struct json_object *nodes_v;
	bool has_children = json_object_object_get_ex(obj, "nodes", &nodes_v) &&
			json_object_is_type(nodes_v, json_type_array) &&
			json_object_array_length(nodes_v) > 0;

	struct json_object *swallows_v;
	bool has_swallows = json_object_object_get_ex(obj, "swallows", &swallows_v);

	if (has_children) {
		// i3 ignores swallows on non-leaves; mirror that.
		size_t n = json_object_array_length(nodes_v);
		for (size_t i = 0; i < n; i++) {
			struct json_object *child_obj =
					json_object_array_get_idx(nodes_v, i);
			struct sway_container *child = build_node(child_obj, error_out);
			if (!child) {
				free_transient_subtree(c);
				return NULL;
			}
			container_add_child(c, child);
		}
	} else if (has_swallows) {
		list_t *sw = parse_swallows(swallows_v, error_out);
		if (!sw) {
			free_transient_subtree(c);
			return NULL;
		}
		c->is_placeholder = true;
		c->swallows = sw;
		// Retained so IPC can echo it verbatim for round-trip.
		c->swallows_json = json_object_get(swallows_v);
	} else {
		// Leaf without swallows is an empty split with no children. i3 does
		// not produce these; treat as an error to avoid silently leaving
		// orphaned containers.
		*error_out = format_str("append_layout: node has neither nodes nor "
				"swallows");
		free_transient_subtree(c);
		return NULL;
	}

	return c;
}

bool load_layout_from_file(struct sway_workspace *ws, const char *path,
		char **error_out) {
	if (!ws) {
		*error_out = format_str("append_layout: no target workspace");
		return false;
	}

	char *buf = slurp_file(path, error_out);
	if (!buf) {
		return false;
	}
	buf = preprocess_i3_concat(buf);

	struct json_tokener *tok = json_tokener_new();
	if (!tok) {
		free(buf);
		*error_out = format_str("append_layout: json_tokener_new failed");
		return false;
	}
	struct json_object *root_obj = json_tokener_parse_ex(tok, buf, strlen(buf));
	enum json_tokener_error err = json_tokener_get_error(tok);
	json_tokener_free(tok);
	free(buf);
	if (!root_obj || err != json_tokener_success) {
		*error_out = format_str("append_layout: json parse error: %s",
				json_tokener_error_desc(err));
		if (root_obj) {
			json_object_put(root_obj);
		}
		return false;
	}

	// All-or-nothing: build the whole tree before attaching anything.
	list_t *children = create_list();
	bool ok = true;
	if (json_object_is_type(root_obj, json_type_array)) {
		size_t n = json_object_array_length(root_obj);
		for (size_t i = 0; i < n; i++) {
			struct json_object *entry =
					json_object_array_get_idx(root_obj, i);
			struct sway_container *child = build_node(entry, error_out);
			if (!child) {
				ok = false;
				break;
			}
			list_add(children, child);
		}
	} else if (json_object_is_type(root_obj, json_type_object)) {
		struct sway_container *child = build_node(root_obj, error_out);
		if (!child) {
			ok = false;
		} else {
			list_add(children, child);
		}
	} else {
		*error_out = format_str("append_layout: unexpected JSON root type");
		ok = false;
	}
	json_object_put(root_obj);

	if (!ok) {
		for (int i = 0; i < children->length; i++) {
			free_transient_subtree(children->items[i]);
		}
		list_free(children);
		return false;
	}

	// workspace_add_tiling would wrap each child with container_split when
	// default_layout is set, which mutates the parsed tree.
	for (int i = 0; i < children->length; i++) {
		workspace_insert_tiling_direct(ws, children->items[i],
				ws->tiling->length);
	}
	list_free(children);

	arrange_workspace(ws);
	transaction_commit_dirty();
	return true;
}

static bool placeholder_matches_view(struct sway_container *placeholder,
		struct sway_view *view) {
	if (!placeholder->is_placeholder || !placeholder->swallows) {
		return false;
	}
	for (int i = 0; i < placeholder->swallows->length; i++) {
		struct criteria *c = placeholder->swallows->items[i];
		if (criteria_matches_view_unmapped(c, view)) {
			return true;
		}
	}
	return false;
}

static struct sway_container *search_swallow(struct sway_container *con,
		struct sway_view *view) {
	if (placeholder_matches_view(con, view)) {
		return con;
	}
	if (con->pending.children) {
		for (int i = 0; i < con->pending.children->length; i++) {
			struct sway_container *child = con->pending.children->items[i];
			struct sway_container *match = search_swallow(child, view);
			if (match) {
				return match;
			}
		}
	}
	return NULL;
}

struct sway_container *find_swallow_match(struct sway_view *view) {
	if (!view) {
		return NULL;
	}
	for (int o = 0; o < root->outputs->length; o++) {
		struct sway_output *output = root->outputs->items[o];
		for (int w = 0; w < output->workspaces->length; w++) {
			struct sway_workspace *ws = output->workspaces->items[w];
			for (int t = 0; t < ws->tiling->length; t++) {
				struct sway_container *con = ws->tiling->items[t];
				struct sway_container *match = search_swallow(con, view);
				if (match) {
					return match;
				}
			}
		}
	}
	return NULL;
}
