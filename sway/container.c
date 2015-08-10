#include "container.h"
#include "layout.h"

void container_map(swayc_t *container, void (*f)(swayc_t *view, void *data), void *data) {
	if (!container->children) {
		return;
	}
	int i;
	for (i = 0; i < container->children->length; ++i) {
		swayc_t *child = container->children->items[i];
		f(child, data);

		if (child->children) {
			container_map(child, f, data);
		}
	}
}

