#include <strings.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "log.h"

struct sway_container *output_by_name(const char *name) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		if (strcasecmp(output->name, name) == 0){
			return output;
		}
	}
	return NULL;
}
