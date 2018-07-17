#ifndef SWAY_DEBUG_H
#define SWAY_DEBUG_H

// Tree
extern bool enable_debug_tree;
void update_debug_tree();

// Damage
extern const char *damage_debug;

// Transactions
extern int txn_timeout_ms;
extern bool txn_debug;

#endif
