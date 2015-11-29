#include <stdio.h>
#include <stdlib.h>
#include "client/registry.h"
#include "client/window.h"
#include "log.h"

struct registry *registry;
struct window *window;

void sway_terminate(void) {
	window_teardown(window);
	registry_teardown(registry);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	sway_log(L_INFO, "Hello world!");
	return 0;
}
