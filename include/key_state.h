#ifndef  _SWAY_KEY_STATE_H
#define  _SWAY_KEY_STATE_H
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t keycode;

// returns true if key has been pressed, otherwise false
bool check_key(keycode key);

// sets a key as pressed
void press_key(keycode key);

// unsets a key as pressed
void release_key(keycode key);

#endif

