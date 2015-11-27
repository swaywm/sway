#include <stdio.h>
#include <stdlib.h>
#include "log.h"

void sway_terminate(void) {
	exit(1);
}

int main(int argc, const char **argv) {
	init_log(L_INFO);
	sway_log(L_INFO, "Hello world!");
}
