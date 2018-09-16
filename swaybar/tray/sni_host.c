#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

char *init_dbus_sni_host(const char *prefix) {
	char *name = NULL;
	
	name = calloc(sizeof(char), 256);
	if (name == NULL) {
		fprintf(stderr, "Could not allocate SNI host name\n");	
		return NULL;
	}

	pid_t pid = getpid();
	snprintf(name, sizeof(char) * 256, "%s-%d", prefix, pid);
	return name;
}

