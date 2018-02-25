#include <strings.h>
#include "sway/commands.h"
#include "sway/server.h"
#include "sway/output.h"
#include "log.h"

static struct cmd_results *backend_cmd_del(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "del", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	sway_server_remove_subbackend(&server, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_add(int argc, char **argv) {
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

static struct cmd_results *backend_cmd_add_output(int argc, char **argv,
		struct sway_subbackend *backend) {
	// TODO allow to name the output
	sway_subbackend_add_output(&server, backend, NULL);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_del_output(int argc, char **argv,
		struct sway_subbackend *backend) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "backend", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	for (int i = 0; i < root_container.children->length; ++i) {
		swayc_t *output = root_container.children->items[i];
		if (output->type != C_OUTPUT) {
			continue;
		}

		const char *name = output->sway_output->wlr_output->name;

		if (output->sway_output->wlr_output->backend == backend->backend &&
				strcmp(argv[0], name) == 0) {
			wlr_output_destroy(output->sway_output->wlr_output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_add_input(int argc, char **argv,
		struct sway_subbackend *backend) {
	wlr_log(L_DEBUG, "TODO: backend cmd add_input");
	return NULL;
}

static struct cmd_results *backend_cmd_del_input(int argc, char **argv,
		struct sway_subbackend *backend) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "backend", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	return NULL;
}

struct cmd_results *cmd_backend(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "backend", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int argc_new = argc-2;
	char **argv_new = argv+2;

	if (strcasecmp("add", argv[0]) == 0) {
		return backend_cmd_add(argc_new, argv_new);
	} else if (strcasecmp("del", argv[0]) == 0) {
		return backend_cmd_del(argc_new, argv_new);
	}

	struct sway_subbackend *subbackend =
		sway_server_get_subbackend(&server, argv[0]);

	if (subbackend == NULL) {
		return cmd_results_new(CMD_INVALID, "backend <cmd> [args]",
				"Cannot find backend: %s", argv[0]);
	}

	if (strcasecmp("add-output", argv[1]) == 0) {
		return backend_cmd_add_output(argc_new, argv_new, subbackend);
	} else if (strcasecmp("del-output", argv[1]) == 0) {
		return backend_cmd_del_output(argc_new, argv_new, subbackend);
	} else if (strcasecmp("add-input", argv[1]) == 0) {
		return backend_cmd_add_input(argc_new, argv_new, subbackend);
	} else if (strcasecmp("del-input", argv[1]) == 0) {
		return backend_cmd_del_input(argc_new, argv_new, subbackend);
	}

	return cmd_results_new(CMD_INVALID, "backend <cmd> [args]", "Unknown command %s", argv[1]);
}
