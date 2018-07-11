#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sway/config.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

static pid_t swaybg_pid = -1;

static void terminate_swaybg(void) {
	wlr_log(WLR_DEBUG, "Terminating swaybg %d", swaybg_pid);
	int ret = kill(-swaybg_pid, SIGTERM);
	if (ret != 0) {
		wlr_log_errno(WLR_ERROR, "Unable to terminate swaybg %d", swaybg_pid);
	} else {
		waitpid(swaybg_pid, NULL, 0);
	}
	swaybg_pid = 0;
}

void load_swaybg(void) {
	if (swaybg_pid >= 0) {
		terminate_swaybg();
	}

	swaybg_pid = fork();
	if (swaybg_pid < 0) {
		wlr_log(WLR_ERROR, "Failed to fork()");
		return;
	} else if (swaybg_pid == 0) {
		setsid();

		const char *cmd = config->swaybg_command ?
			config->swaybg_command : "swaybg";

		size_t argv_cap = 2 + 4 * config->output_configs->length;
		size_t argv_len = 0;
		char *argv[argv_cap];
		argv[argv_len++] = "swaybg";

		for (int i = 0; i < config->output_configs->length; ++i) {
			struct output_config *oc = config->output_configs->items[i];

			if (oc->background_option) {
				if (strcmp(oc->background_option, "solid_color") == 0) {
					argv[argv_len++] = "--color";
					argv[argv_len++] = oc->background;
					continue;
				}

				argv[argv_len++] = "--scaling";
				argv[argv_len++] = oc->background_option;
			}

			if (oc->background) {
				char *arg;
				if (strcmp(oc->name, "*") == 0) {
					arg = oc->background;
				} else {
					arg = malloc(strlen(oc->name) + 1 +
						strlen(oc->background) + 1);
					strcpy(arg, oc->name);
					strcat(arg, ":");
					strcat(arg, oc->background);
				}

				argv[argv_len++] = "--image";
				argv[argv_len++] = arg;
			}
		}

		argv[argv_len++] = NULL;
		assert(argv_len <= argv_cap);

		execvp(cmd, argv);

		wlr_log(WLR_ERROR, "Failed to exec swaybg");
		exit(EXIT_FAILURE);
	}

	wlr_log(WLR_ERROR, "Spawned swaybg %d", swaybg_pid);
}
