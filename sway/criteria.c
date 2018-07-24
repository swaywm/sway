#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pcre.h>
#include "sway/criteria.h"
#include "sway/tree/container.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

bool criteria_is_empty(struct criteria *criteria) {
	return !criteria->title
		&& !criteria->shell
		&& !criteria->app_id
		&& !criteria->class
		&& !criteria->instance
		&& !criteria->con_mark
		&& !criteria->con_id
    #ifdef HAVE_XWAYLAND
		&& !criteria->id
    #endif
		&& !criteria->window_role
		&& !criteria->window_type
		&& !criteria->floating
		&& !criteria->tiling
		&& !criteria->urgent
		&& !criteria->workspace;
}

void criteria_destroy(struct criteria *criteria) {
	pcre_free(criteria->title);
	pcre_free(criteria->shell);
	pcre_free(criteria->app_id);
	pcre_free(criteria->class);
	pcre_free(criteria->instance);
	pcre_free(criteria->con_mark);
	pcre_free(criteria->window_role);
	free(criteria->workspace);
	free(criteria->cmdlist);
	free(criteria->raw);
	free(criteria);
}

static int regex_cmp(const char *item, const pcre *regex) {
	return pcre_exec(regex, NULL, item, strlen(item), 0, 0, NULL, 0);
}

static int cmp_urgent(const void *_a, const void *_b) {
	struct sway_view *a = *(void **)_a;
	struct sway_view *b = *(void **)_b;

	if (a->urgent.tv_sec < b->urgent.tv_sec) {
		return -1;
	} else if (a->urgent.tv_sec > b->urgent.tv_sec) {
		return 1;
	}
	if (a->urgent.tv_nsec < b->urgent.tv_nsec) {
		return -1;
	} else if (a->urgent.tv_nsec > b->urgent.tv_nsec) {
		return 1;
	}
	return 0;
}

static void find_urgent_iterator(struct sway_container *swayc, void *data) {
	if (swayc->type != C_VIEW || !view_is_urgent(swayc->sway_view)) {
		return;
	}
	list_t *urgent_views = data;
	list_add(urgent_views, swayc->sway_view);
}

static bool criteria_matches_view(struct criteria *criteria,
		struct sway_view *view) {
	if (criteria->title) {
		const char *title = view_get_title(view);
		if (!title || regex_cmp(title, criteria->title) != 0) {
			return false;
		}
	}

	if (criteria->shell) {
		const char *shell = view_get_shell(view);
		if (!shell || regex_cmp(shell, criteria->shell) != 0) {
			return false;
		}
	}

	if (criteria->app_id) {
		const char *app_id = view_get_app_id(view);
		if (!app_id || regex_cmp(app_id, criteria->app_id) != 0) {
			return false;
		}
	}

	if (criteria->class) {
		const char *class = view_get_class(view);
		if (!class || regex_cmp(class, criteria->class) != 0) {
			return false;
		}
	}

	if (criteria->instance) {
		const char *instance = view_get_instance(view);
		if (!instance || regex_cmp(instance, criteria->instance) != 0) {
			return false;
		}
	}

	if (criteria->con_mark) {
		bool exists = false;
		for (int i = 0; i < view->marks->length; ++i) {
			if (regex_cmp(view->marks->items[i], criteria->con_mark) == 0) {
				exists = true;
				break;
			}
		}
		if (!exists) {
			return false;
		}
	}

	if (criteria->con_id) { // Internal ID
		if (!view->swayc || view->swayc->id != criteria->con_id) {
			return false;
		}
	}

  #ifdef HAVE_XWAYLAND
	if (criteria->id) { // X11 window ID
		uint32_t x11_window_id = view_get_x11_window_id(view);
		if (!x11_window_id || x11_window_id != criteria->id) {
			return false;
		}
	}
  #endif

	if (criteria->window_role) {
		// TODO
	}

	if (criteria->window_type) {
		uint32_t type = view_get_window_type(view);
		if (!type || type != criteria->window_type) {
			return false;
		}
	}

	if (criteria->floating) {
		if (!container_is_floating(view->swayc)) {
			return false;
		}
	}

	if (criteria->tiling) {
		if (container_is_floating(view->swayc)) {
			return false;
		}
	}

	if (criteria->urgent) {
		if (!view_is_urgent(view)) {
			return false;
		}
		list_t *urgent_views = create_list();
		container_for_each_descendant_dfs(&root_container,
				find_urgent_iterator, urgent_views);
		list_stable_sort(urgent_views, cmp_urgent);
		struct sway_view *target;
		if (criteria->urgent == 'o') { // oldest
			target = urgent_views->items[0];
		} else { // latest
			target = urgent_views->items[urgent_views->length - 1];
		}
		list_free(urgent_views);
		if (view != target) {
			return false;
		}
	}

	if (criteria->workspace) {
		if (!view->swayc) {
			return false;
		}
		struct sway_container *ws = container_parent(view->swayc, C_WORKSPACE);
		if (!ws || strcmp(ws->name, criteria->workspace) != 0) {
			return false;
		}
	}

	return true;
}

list_t *criteria_for_view(struct sway_view *view, enum criteria_type types) {
	list_t *criterias = config->criteria;
	list_t *matches = create_list();
	for (int i = 0; i < criterias->length; ++i) {
		struct criteria *criteria = criterias->items[i];
		if ((criteria->type & types) && criteria_matches_view(criteria, view)) {
			list_add(matches, criteria);
		}
	}
	return matches;
}

struct match_data {
	struct criteria *criteria;
	list_t *matches;
};

static void criteria_get_views_iterator(struct sway_container *container,
		void *data) {
	struct match_data *match_data = data;
	if (container->type == C_VIEW) {
		if (criteria_matches_view(match_data->criteria, container->sway_view)) {
			list_add(match_data->matches, container->sway_view);
		}
	}
}

list_t *criteria_get_views(struct criteria *criteria) {
	list_t *matches = create_list();
	struct match_data data = {
		.criteria = criteria,
		.matches = matches,
	};
	container_for_each_descendant_dfs(&root_container,
		criteria_get_views_iterator, &data);

	// Scratchpad items which are hidden are not in the tree.
	for (int i = 0; i < root_container.sway_root->scratchpad->length; ++i) {
		struct sway_container *con =
			root_container.sway_root->scratchpad->items[i];
		if (!con->parent) {
			criteria_get_views_iterator(con, &data);
		}
	}
	return matches;
}

// The error pointer is used for parsing functions, and saves having to pass it
// as an argument in several places.
char *error = NULL;

// Returns error string on failure or NULL otherwise.
static bool generate_regex(pcre **regex, char *value) {
	const char *reg_err;
	int offset;

	*regex = pcre_compile(value, PCRE_UTF8 | PCRE_UCP, &reg_err, &offset, NULL);

	if (!*regex) {
		const char *fmt = "Regex compilation for '%s' failed: %s";
		int len = strlen(fmt) + strlen(value) + strlen(reg_err) - 3;
		error = malloc(len);
		snprintf(error, len, fmt, value, reg_err);
		return false;
	}

	return true;
}

enum criteria_token {
	T_APP_ID,
	T_CLASS,
	T_CON_ID,
	T_CON_MARK,
	T_FLOATING,
  #ifdef HAVE_XWAYLAND
	T_ID,
  #endif
	T_INSTANCE,
	T_SHELL,
	T_TILING,
	T_TITLE,
	T_URGENT,
	T_WINDOW_ROLE,
	T_WINDOW_TYPE,
	T_WORKSPACE,

	T_INVALID,
};

static enum criteria_token token_from_name(char *name) {
	if (strcmp(name, "app_id") == 0) {
		return T_APP_ID;
	} else if (strcmp(name, "class") == 0) {
		return T_CLASS;
	} else if (strcmp(name, "con_id") == 0) {
		return T_CON_ID;
	} else if (strcmp(name, "con_mark") == 0) {
		return T_CON_MARK;
  #ifdef HAVE_XWAYLAND
	} else if (strcmp(name, "id") == 0) {
		return T_ID;
  #endif
	} else if (strcmp(name, "instance") == 0) {
		return T_INSTANCE;
	} else if (strcmp(name, "shell") == 0) {
		return T_SHELL;
	} else if (strcmp(name, "title") == 0) {
		return T_TITLE;
	} else if (strcmp(name, "urgent") == 0) {
		return T_URGENT;
	} else if (strcmp(name, "window_role") == 0) {
		return T_WINDOW_ROLE;
	} else if (strcmp(name, "window_type") == 0) {
		return T_WINDOW_TYPE;
	} else if (strcmp(name, "workspace") == 0) {
		return T_WORKSPACE;
	}
	return T_INVALID;
}

/**
 * Get a property of the focused view.
 *
 * Note that we are taking the focused view at the time of criteria parsing, not
 * at the time of execution. This is because __focused__ only makes sense when
 * using criteria via IPC. Using __focused__ in config is not useful because
 * criteria is only executed once per view.
 */
static char *get_focused_prop(enum criteria_token token) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);

	if (!focus || focus->type != C_VIEW) {
		return NULL;
	}
	struct sway_view *view = focus->sway_view;
	const char *value = NULL;

	switch (token) {
	case T_APP_ID:
		value = view_get_app_id(view);
		break;
	case T_CLASS:
		value = view_get_class(view);
		break;
	case T_INSTANCE:
		value = view_get_instance(view);
		break;
	case T_SHELL:
		value = view_get_shell(view);
		break;
	case T_TITLE:
		value = view_get_class(view);
		break;
	case T_WINDOW_ROLE:
		value = view_get_class(view);
		break;
	case T_WORKSPACE:
		{
			struct sway_container *ws = container_parent(focus, C_WORKSPACE);
			if (ws) {
				value = ws->name;
			}
		}
		break;
	case T_CON_ID: // These do not support __focused__
	case T_CON_MARK:
	case T_FLOATING:
  #ifdef HAVE_XWAYLAND
	case T_ID:
  #endif
	case T_TILING:
	case T_URGENT:
	case T_WINDOW_TYPE:
	case T_INVALID:
		break;
	}
	if (value) {
		return strdup(value);
	}
	return NULL;
}

static bool parse_token(struct criteria *criteria, char *name, char *value) {
	enum criteria_token token = token_from_name(name);
	if (token == T_INVALID) {
		const char *fmt = "Token '%s' is not recognized";
		int len = strlen(fmt) + strlen(name) - 1;
		error = malloc(len);
		snprintf(error, len, fmt, name);
		return false;
	}

	char *effective_value = NULL;
	if (value && strcmp(value, "__focused__") == 0) {
		effective_value = get_focused_prop(token);
	} else if (value) {
		effective_value = strdup(value);
	}

	// Require value, unless token is floating or tiled
	if (!effective_value && token != T_FLOATING && token != T_TILING) {
		const char *fmt = "Token '%s' requires a value";
		int len = strlen(fmt) + strlen(name) - 1;
		error = malloc(len);
		snprintf(error, len, fmt, name);
		return false;
	}

	char *endptr = NULL;
	switch (token) {
	case T_TITLE:
		generate_regex(&criteria->title, effective_value);
		break;
	case T_SHELL:
		generate_regex(&criteria->shell, effective_value);
		break;
	case T_APP_ID:
		generate_regex(&criteria->app_id, effective_value);
		break;
	case T_CLASS:
		generate_regex(&criteria->class, effective_value);
		break;
	case T_INSTANCE:
		generate_regex(&criteria->instance, effective_value);
		break;
	case T_CON_ID:
		criteria->con_id = strtoul(effective_value, &endptr, 10);
		if (*endptr != 0) {
			error = strdup("The value for 'con_id' should be numeric");
		}
		break;
	case T_CON_MARK:
		generate_regex(&criteria->con_mark, effective_value);
		break;
	case T_WINDOW_ROLE:
		generate_regex(&criteria->window_role, effective_value);
		break;
	case T_WINDOW_TYPE:
		// TODO: This is a string but will be stored as an enum or integer
		break;
  #ifdef HAVE_XWAYLAND
	case T_ID:
		criteria->id = strtoul(effective_value, &endptr, 10);
		if (*endptr != 0) {
			error = strdup("The value for 'id' should be numeric");
		}
		break;
  #endif
	case T_FLOATING:
		criteria->floating = true;
		break;
	case T_TILING:
		criteria->tiling = true;
		break;
	case T_URGENT:
		if (strcmp(effective_value, "latest") == 0) {
			criteria->urgent = 'l';
		} else if (strcmp(effective_value, "oldest") == 0) {
			criteria->urgent = 'o';
		} else {
			error =
				strdup("The value for 'urgent' must be 'latest' or 'oldest'");
		}
		break;
	case T_WORKSPACE:
		criteria->workspace = strdup(effective_value);
		break;
	case T_INVALID:
		break;
	}
	free(effective_value);

	if (error) {
		return false;
	}

	return true;
}

static void skip_spaces(char **head) {
	while (**head == ' ') {
		++*head;
	}
}

// Remove escaping slashes from value
static void unescape(char *value) {
	if (!strchr(value, '\\')) {
		return;
	}
	char *copy = calloc(strlen(value) + 1, 1);
	char *readhead = value;
	char *writehead = copy;
	while (*readhead) {
		if (*readhead == '\\' && *(readhead + 1) == '"') {
			// skip the slash
			++readhead;
		}
		*writehead = *readhead;
		++writehead;
		++readhead;
	}
	strcpy(value, copy);
	free(copy);
}

/**
 * Parse a raw criteria string such as [class="foo" instance="bar"] into a
 * criteria struct.
 *
 * If errors are found, NULL will be returned and the error argument will be
 * populated with an error string. It is up to the caller to free the error.
 */
struct criteria *criteria_parse(char *raw, char **error_arg) {
	*error_arg = NULL;
	error = NULL;

	char *head = raw;
	skip_spaces(&head);
	if (*head != '[') {
		*error_arg = strdup("No criteria");
		return NULL;
	}
	++head;

	struct criteria *criteria = calloc(sizeof(struct criteria), 1);
	char *name = NULL, *value = NULL;
	bool in_quotes = false;

	while (*head && *head != ']') {
		skip_spaces(&head);
		// Parse token name
		char *namestart = head;
		while ((*head >= 'a' && *head <= 'z') || *head == '_') {
			++head;
		}
		name = calloc(head - namestart + 1, 1);
		if (head != namestart) {
			strncpy(name, namestart, head - namestart);
		}
		// Parse token value
		skip_spaces(&head);
		value = NULL;
		if (*head == '=') {
			++head;
			skip_spaces(&head);
			if (*head == '"') {
				in_quotes = true;
				++head;
			}
			char *valuestart = head;
			if (in_quotes) {
				while (*head && (*head != '"' || *(head - 1) == '\\')) {
					++head;
				}
				if (!*head) {
					*error_arg = strdup("Quote mismatch in criteria");
					goto cleanup;
				}
			} else {
				while (*head && *head != ' ' && *head != ']') {
					++head;
				}
			}
			value = calloc(head - valuestart + 1, 1);
			strncpy(value, valuestart, head - valuestart);
			if (in_quotes) {
				++head;
				in_quotes = false;
			}
			unescape(value);
		}
		wlr_log(WLR_DEBUG, "Found pair: %s=%s", name, value);
		if (!parse_token(criteria, name, value)) {
			*error_arg = error;
			goto cleanup;
		}
		skip_spaces(&head);
		free(name);
		free(value);
		name = NULL;
		value = NULL;
	}
	if (*head != ']') {
		*error_arg = strdup("No closing brace found in criteria");
		goto cleanup;
	}

	if (criteria_is_empty(criteria)) {
		*error_arg = strdup("Criteria is empty");
		goto cleanup;
	}

	++head;
	int len = head - raw;
	criteria->raw = calloc(len + 1, 1);
	strncpy(criteria->raw, raw, len);
	return criteria;

cleanup:
	free(name);
	free(value);
	criteria_destroy(criteria);
	return NULL;
}
