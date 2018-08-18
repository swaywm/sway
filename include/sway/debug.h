#ifndef SWAY_DEBUG_H
#define SWAY_DEBUG_H
#include <stdbool.h>

struct sway_debug {
	bool highlight_damage; // Highlight regions of the screen being damaged
	bool noatomic;         // Ignore atomic layout updates
	bool nodamage;         // Render the full output on each frame
	bool render_tree;      // Render the tree overlay
	bool txn_timings;      // Log verbose messages about transactions
	bool txn_wait;         // Always wait for the timeout before applying
};

extern struct sway_debug debug;

void update_debug_tree();

#endif
