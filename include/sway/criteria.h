#ifndef _SWAY_CRITERIA_H
#define _SWAY_CRITERIA_H

#include "container.h"
#include "list.h"

/**
 * Maps criteria (as a list of criteria tokens) to a command list.
 *
 * A list of tokens together represent a single criteria string (e.g.
 * '[class="abc" title="xyz"]' becomes two criteria tokens).
 *
 * for_window: Views matching all criteria will have the bound command list
 * executed on them.
 *
 * Set via `for_window <criteria> <cmd list>`.
 */
struct criteria {
	list_t *tokens; // struct crit_token, contains compiled regex.
	char *crit_raw; // entire criteria string (for logging)

	char *cmdlist;
};

int criteria_cmp(const void *item, const void *data);
void free_criteria(struct criteria *crit);

// Pouplate list with crit_tokens extracted from criteria string, returns error
// string or NULL if successful.
char *extract_crit_tokens(list_t *tokens, const char *criteria);

// Returns list of criteria that match given container. These criteria have
// been set with `for_window` commands and have an associated cmdlist.
list_t *criteria_for(swayc_t *cont);

// Returns a list of all containers that match the given list of tokens.
list_t *container_for_crit_tokens(list_t *tokens);

// Returns true if any criteria in the given list matches this container
bool criteria_any(swayc_t *cont, list_t *criteria);

#endif
