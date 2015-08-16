#ifndef _SWAY_MOVEMENT_H
#define _SWAY_MOVEMENT_H

#include <wlc/wlc.h>
#include "list.h"

enum movement_direction {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_PARENT
};

bool move_focus(enum movement_direction direction);

#endif
