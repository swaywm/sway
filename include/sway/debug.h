#ifndef SWAY_DEBUG_H
#define SWAY_DEBUG_H
#include <stdbool.h>

struct sway_debug {
	bool noatomic;         // Ignore atomic layout updates
	bool render_tree;      // Render the tree overlay
	bool txn_timings;      // Log verbose messages about transactions
	bool txn_wait;         // Always wait for the timeout before applying

	enum {
		DAMAGE_DEFAULT,    // Default behaviour
		DAMAGE_HIGHLIGHT,  // Highlight regions of the screen being damaged
		DAMAGE_RERENDER,   // Render the full output when any damage occurs
	} damage;
};

extern struct sway_debug debug;

void update_debug_tree(void);

#endif
