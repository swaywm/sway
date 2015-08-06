#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "list.h"

struct sway_container {
    wlc_handle output; // May be NULL
    list_t children;
};

wlc_handle get_topmost(wlc_handle output, size_t offset);

#endif
