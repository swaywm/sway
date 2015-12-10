#include "wayland-desktop-shell-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include "client/window.h"
#include "client/registry.h"
#include "log.h"

void sway_terminate(void) {
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	init_log(L_INFO);
	sway_log(L_INFO, "Hello world");
	return 0;
}
