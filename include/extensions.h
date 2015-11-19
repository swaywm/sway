#ifndef _SWAY_EXTENSIONS_H
#define _SWAY_EXTENSIONS_H

#include "list.h"

struct background_config {
        wlc_handle output;
        wlc_handle surface;
};

struct desktop_shell_state {
        list_t *backgrounds;
};

extern struct desktop_shell_state desktop_shell;

void register_extensions(void);

#endif
