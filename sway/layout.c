#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include "layout.h"

wlc_handle get_topmost(wlc_handle output, size_t offset) {
   size_t memb;
   const wlc_handle *views = wlc_output_get_views(output, &memb);
   return (memb > 0 ? views[(memb - 1 + offset) % memb] : 0);
}
