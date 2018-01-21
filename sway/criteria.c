#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pcre.h>
#include "sway/criteria.h"
#include "sway/container.h"
#include "sway/config.h"
#include "sway/view.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

enum criteria_type { // *must* keep in sync with criteria_strings[]
	CRIT_APP_ID,
	CRIT_CLASS,
	CRIT_CON_ID,
	CRIT_CON_MARK,
	CRIT_FLOATING,
	CRIT_ID,
	CRIT_INSTANCE,
	CRIT_TILING,
	CRIT_TITLE,
	CRIT_URGENT,
	CRIT_WINDOW_ROLE,
	CRIT_WINDOW_TYPE,
	CRIT_WORKSPACE,
	CRIT_LAST
};

static const char * const criteria_strings[CRIT_LAST] = {
	[CRIT_APP_ID] = "app_id",
	[CRIT_CLASS] = "class",
	[CRIT_CON_ID] = "con_id",
	[CRIT_CON_MARK] = "con_mark",
	[CRIT_FLOATING] = "floating",
	[CRIT_ID] = "id",
	[CRIT_INSTANCE] = "instance",
	[CRIT_TILING] = "tiling",
	[CRIT_TITLE] = "title",
	[CRIT_URGENT] = "urgent", // either "latest" or "oldest" ...
	[CRIT_WINDOW_ROLE] = "window_role",
	[CRIT_WINDOW_TYPE] = "window_type",
	[CRIT_WORKSPACE] = "workspace"
};

/**
 * A single criteria token (ie. value/regex pair),
 * e.g. 'class="some class regex"'.
 */
struct crit_token {
	enum criteria_type type;
	pcre *regex;
	char *raw;
};

static void free_crit_token(struct crit_token *crit) {
	pcre_free(crit->regex);
	free(crit->raw);
	free(crit);
}

static void free_crit_tokens(list_t *crit_tokens) {
	for (int i = 0; i < crit_tokens->length; i++) {
		free_crit_token(crit_tokens->items[i]);
	}
	list_free(crit_tokens);
}

// Extracts criteria string from its brackets. Returns new (duplicate)
// substring.
static char *criteria_from(const char *arg) {
	char *criteria = NULL;
	if (*arg == '[') {
		criteria = strdup(arg + 1);
	} else {
		criteria = strdup(arg);
	}

	int last = strlen(criteria) - 1;
	if (criteria[last] == ']') {
		criteria[last] = '\0';
	}
	return criteria;
}

// Return instances of c found in str.
static int countchr(char *str, char c) {
	int found = 0;
	for (int i = 0; str[i]; i++) {
		if (str[i] == c) {
			++found;
		}
	}
	return found;
}

// criteria_str is e.g. '[class="some class regex" instance="instance name"]'.
//
// Will create array of pointers in buf, where first is duplicate of given
// string (must be freed) and the rest are pointers to names and values in the
// base string (every other, naturally). argc will be populated with the length
// of buf.
//
// Returns error string or NULL if successful.
static char *crit_tokens(int *argc, char ***buf, const char * const criteria_str) {
	wlr_log(L_DEBUG, "Parsing criteria: '%s'", criteria_str);
	char *base = criteria_from(criteria_str);
	char *head = base;
	char *namep = head; // start of criteria name
	char *valp = NULL; // start of value

	// We're going to place EOS markers where we need to and fill up an array
	// of pointers to the start of each token (either name or value).
	int pairs = countchr(base, '=');
	int max_tokens = pairs * 2 + 1; // this gives us at least enough slots

	char **argv = *buf = calloc(max_tokens, sizeof(char*));
	argv[0] = base; // this needs to be freed by caller
	bool quoted = true;

	*argc = 1; // uneven = name, even = value
	while (*head && *argc < max_tokens) {
		if (namep != head && *(head - 1) == '\\') {
			// escaped character: don't try to parse this
		} else if (*head == '=' && namep != head) {
			if (*argc % 2 != 1) {
				// we're not expecting a name
				return strdup("Unable to parse criteria: "
					"Found out of place equal sign");
			} else {
				// name ends here
				char *end = head; // don't want to rewind the head
				while (*(end - 1) == ' ') {
					--end;
				}
				*end = '\0';
				if (*(namep) == ' ') {
					namep = strrchr(namep, ' ') + 1;
				}
				argv[*argc] = namep;
				*argc += 1;
			}
		} else if (*head == '"') {
			if (*argc % 2 != 0) {
				// we're not expecting a value
				return strdup("Unable to parse criteria: "
					"Found quoted value where it was not expected");
			} else if (!valp) { // value starts here
				valp = head + 1;
				quoted = true;
			} else {
				// value ends here
				argv[*argc] = valp;
				*argc += 1;
				*head = '\0';
				valp = NULL;
				namep = head + 1;
			}
		} else if (*argc % 2 == 0 && *head != ' ') {
			// parse unquoted values
			if (!valp) {
				quoted = false;
				valp = head;  // value starts here
			}
		} else if (valp && !quoted && *head == ' ') {
			// value ends here
			argv[*argc] = valp;
			*argc += 1;
			*head = '\0';
			valp = NULL;
			namep = head + 1;
		}
		head++;
	}

	// catch last unquoted value if needed
	if (valp && !quoted && !*head) {
		argv[*argc] = valp;
		*argc += 1;
	}

	return NULL;
}

// Returns error string on failure or NULL otherwise.
static char *parse_criteria_name(enum criteria_type *type, char *name) {
	*type = CRIT_LAST;
	for (int i = 0; i < CRIT_LAST; i++) {
		if (strcmp(criteria_strings[i], name) == 0) {
			*type = (enum criteria_type) i;
			break;
		}
	}
	if (*type == CRIT_LAST) {
		const char *fmt = "Criteria type '%s' is invalid or unsupported.";
		int len = strlen(name) + strlen(fmt) - 1;
		char *error = malloc(len);
		snprintf(error, len, fmt, name);
		return error;
	} else if (*type == CRIT_URGENT || *type == CRIT_WINDOW_ROLE ||
			*type == CRIT_WINDOW_TYPE) {
		// (we're just being helpful here)
		const char *fmt = "\"%s\" criteria currently unsupported, "
			"no window will match this";
		int len = strlen(fmt) + strlen(name) - 1;
		char *error = malloc(len);
		snprintf(error, len, fmt, name);
		return error;
	}
	return NULL;
}

// Returns error string on failure or NULL otherwise.
static char *generate_regex(pcre **regex, char *value) {
	const char *reg_err;
	int offset;

	*regex = pcre_compile(value, PCRE_UTF8 | PCRE_UCP, &reg_err, &offset, NULL);

	if (!*regex) {
		const char *fmt = "Regex compilation (for '%s') failed: %s";
		int len = strlen(fmt) + strlen(value) + strlen(reg_err) - 3;
		char *error = malloc(len);
		snprintf(error, len, fmt, value, reg_err);
		return error;
	}
	return NULL;
}

// Test whether the criterion corresponds to the currently focused window
static bool crit_is_focused(const char *value) {
	return !strcmp(value, "focused") || !strcmp(value, "__focused__");
}

// Populate list with crit_tokens extracted from criteria string, returns error
// string or NULL if successful.
char *extract_crit_tokens(list_t *tokens, const char * const criteria) {
	int argc;
	char **argv = NULL, *error = NULL;
	if ((error = crit_tokens(&argc, &argv, criteria))) {
		goto ect_cleanup;
	}
	for (int i = 1; i + 1 < argc; i += 2) {
		char* name = argv[i], *value = argv[i + 1];
		struct crit_token *token = calloc(1, sizeof(struct crit_token));
		token->raw = strdup(value);

		if ((error = parse_criteria_name(&token->type, name))) {
			free_crit_token(token);
			goto ect_cleanup;
		} else if (token->type == CRIT_URGENT || crit_is_focused(value)) {
			wlr_log(L_DEBUG, "%s -> \"%s\"", name, value);
			list_add(tokens, token);
		} else if((error = generate_regex(&token->regex, value))) {
			free_crit_token(token);
			goto ect_cleanup;
		} else {
			wlr_log(L_DEBUG, "%s -> /%s/", name, value);
			list_add(tokens, token);
		}
	}
ect_cleanup:
	free(argv[0]); // base string
	free(argv);
	return error;
}

static int regex_cmp(const char *item, const pcre *regex) {
	return pcre_exec(regex, NULL, item, strlen(item), 0, 0, NULL, 0);
}

// test a single view if it matches list of criteria tokens (all of them).
static bool criteria_test(swayc_t *cont, list_t *tokens) {
	if (cont->type != C_VIEW) {
		return false;
	}
	int matches = 0;
	for (int i = 0; i < tokens->length; i++) {
		struct crit_token *crit = tokens->items[i];
		switch (crit->type) {
		case CRIT_CLASS:
			{
				const char *class = view_get_class(cont->sway_view);
				if (!class) {
					break;
				}
				if (crit->regex && regex_cmp(class, crit->regex) == 0) {
					matches++;
				}
				break;
			}
		case CRIT_CON_ID:
			{
				char *endptr;
				size_t crit_id = strtoul(crit->raw, &endptr, 10);

				if (*endptr == 0 && cont->id == crit_id) {
					++matches;
				}
				break;
			}
		case CRIT_CON_MARK:
			// TODO
			break;
		case CRIT_FLOATING:
			// TODO
			break;
		case CRIT_ID:
			// TODO
			break;
		case CRIT_APP_ID:
			{
				const char *app_id = view_get_app_id(cont->sway_view);
				if (!app_id) {
					break;
				}

				if (crit->regex && regex_cmp(app_id, crit->regex) == 0) {
					matches++;
				}
				break;
			}
		case CRIT_INSTANCE:
			{
				const char *instance = view_get_instance(cont->sway_view);
				if (!instance) {
					break;
				}

				if (crit->regex && regex_cmp(instance, crit->regex) == 0) {
					matches++;
				}
				break;
			}
		case CRIT_TILING:
			// TODO
			break;
		case CRIT_TITLE:
			{
				const char *title = view_get_title(cont->sway_view);
				if (!title) {
					break;
				}

				if (crit->regex && regex_cmp(title, crit->regex) == 0) {
					matches++;
				}
				break;
			}
		case CRIT_URGENT:
			// TODO "latest" or "oldest"
			break;
		case CRIT_WINDOW_ROLE:
			// TODO
			break;
		case CRIT_WINDOW_TYPE:
			// TODO
			break;
		case CRIT_WORKSPACE:
			// TODO
			break;
		default:
			sway_abort("Invalid criteria type (%i)", crit->type);
			break;
		}
	}
	return matches == tokens->length;
}

int criteria_cmp(const void *a, const void *b) {
	if (a == b) {
		return 0;
	} else if (!a) {
		return -1;
	} else if (!b) {
		return 1;
	}
	const struct criteria *crit_a = a, *crit_b = b;
	int cmp = lenient_strcmp(crit_a->cmdlist, crit_b->cmdlist);
	if (cmp != 0) {
		return cmp;
	}
	return lenient_strcmp(crit_a->crit_raw, crit_b->crit_raw);
}

void free_criteria(struct criteria *crit) {
	if (crit->tokens) {
		free_crit_tokens(crit->tokens);
	}
	if (crit->cmdlist) {
		free(crit->cmdlist);
	}
	if (crit->crit_raw) {
		free(crit->crit_raw);
	}
	free(crit);
}

bool criteria_any(swayc_t *cont, list_t *criteria) {
	for (int i = 0; i < criteria->length; i++) {
		struct criteria *bc = criteria->items[i];
		if (criteria_test(cont, bc->tokens)) {
			return true;
		}
	}
	return false;
}

list_t *criteria_for(swayc_t *cont) {
	list_t *criteria = config->criteria, *matches = create_list();
	for (int i = 0; i < criteria->length; i++) {
		struct criteria *bc = criteria->items[i];
		if (criteria_test(cont, bc->tokens)) {
			list_add(matches, bc);
		}
	}
	return matches;
}

struct list_tokens {
	list_t *list;
	list_t *tokens;
};

static void container_match_add(swayc_t *container, struct list_tokens *list_tokens) {
	if (criteria_test(container, list_tokens->tokens)) {
		list_add(list_tokens->list, container);
	}
}

list_t *container_for(list_t *tokens) {
	struct list_tokens list_tokens = (struct list_tokens){create_list(), tokens};

	container_map(&root_container, (void (*)(swayc_t *, void *))container_match_add, &list_tokens);

	// TODO look in the scratchpad
	
	return list_tokens.list;
}
