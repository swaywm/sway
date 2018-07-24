#ifndef _SWAY_CRITERIA_H
#define _SWAY_CRITERIA_H

#include <pcre.h>
#include "list.h"
#include "tree/view.h"

enum criteria_type {
	CT_COMMAND          = 1 << 0,
	CT_ASSIGN_OUTPUT    = 1 << 1,
	CT_ASSIGN_WORKSPACE = 1 << 2,
	CT_NO_FOCUS         = 1 << 3,
};

struct criteria {
	enum criteria_type type;
	char *raw; // entire criteria string (for logging)
	char *cmdlist;
	char *target; // workspace or output name for `assign` criteria

	pcre *title;
	pcre *shell;
	pcre *app_id;
	pcre *class;
	pcre *instance;
	pcre *con_mark;
	uint32_t con_id; // internal ID
#ifdef HAVE_XWAYLAND
	uint32_t id; // X11 window ID
#endif
	pcre *window_role;
	uint32_t window_type;
	bool floating;
	bool tiling;
	char urgent; // 'l' for latest or 'o' for oldest
	char *workspace;
};

bool criteria_is_empty(struct criteria *criteria);

void criteria_destroy(struct criteria *criteria);

/**
 * Generate a criteria struct from a raw criteria string such as
 * [class="foo" instance="bar"] (brackets inclusive).
 *
 * The error argument is expected to be an address of a null pointer. If an
 * error is encountered, the function will return NULL and the pointer will be
 * changed to point to the error string. This string should be freed afterwards.
 */
struct criteria *criteria_parse(char *raw, char **error);

/**
 * Compile a list of criterias matching the given view.
 *
 * Criteria types can be bitwise ORed.
 */
list_t *criteria_for_view(struct sway_view *view, enum criteria_type types);

/**
 * Compile a list of views matching the given criteria.
 */
list_t *criteria_get_views(struct criteria *criteria);

#endif
