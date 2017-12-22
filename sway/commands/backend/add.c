#define _XOPEN_SOURCE 700
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/server.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *backend_cmd_add(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "add", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	const char *type_name = argv[0];
	enum sway_subbackend_type type;
	if (strcasecmp(type_name, "wayland") == 0) {
		type = SWAY_SUBBACKEND_WAYLAND;
	} else if (strcasecmp(type_name, "x11") == 0) {
		type = SWAY_SUBBACKEND_X11;
	} else if (strcasecmp(type_name, "drm") == 0) {
		type = SWAY_SUBBACKEND_DRM;
	} else if (strcasecmp(type_name, "headless") == 0) {
		type = SWAY_SUBBACKEND_HEADLESS;
	} else {
		error =
			cmd_results_new(CMD_INVALID,
				"add", "Expected 'add <wayland|x11|drm|headless>'");
		return error;
	}

	struct sway_subbackend *subbackend =
		sway_subbackend_create(type, NULL);
	sway_server_add_subbackend(&server, subbackend);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
