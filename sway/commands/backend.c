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

	sway_server_remove_backend(&server, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_add(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "add", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	const char *type_name = argv[0];
	enum sway_backend_type type;
	if (strcasecmp(type_name, "wayland") == 0) {
		type = SWAY_BACKEND_WAYLAND;
	} else if (strcasecmp(type_name, "x11") == 0) {
		type = SWAY_BACKEND_X11;
	} else if (strcasecmp(type_name, "drm") == 0) {
		type = SWAY_BACKEND_DRM;
	} else if (strcasecmp(type_name, "headless") == 0) {
		type = SWAY_BACKEND_HEADLESS;
	} else {
		error =
			cmd_results_new(CMD_INVALID,
				"add", "Expected 'add <wayland|x11|drm|headless>'");
		return error;
	}

	struct sway_backend *backend =
		sway_backend_create(type, NULL);
	sway_server_add_backend(&server, backend);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_add_output(int argc, char **argv,
		struct sway_backend *backend) {
	// TODO allow to name the output
	char *name = NULL;
	if (argc > 0) {
		name = argv[0];
		if (strlen(name) > 16) {
			return cmd_results_new(CMD_INVALID, "backend <cmd> [args]",
					"output name must be less than 16 characters");
		}
	}

	sway_backend_add_output(&server, backend, name);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_del_output(int argc, char **argv,
		struct sway_backend *backend) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "backend", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	sway_backend_remove_output(&server, backend, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *backend_cmd_add_input(int argc, char **argv,
		struct sway_backend *backend) {
	return cmd_results_new(CMD_INVALID, "backend <cmd> [args]",
			"add_input is not implemented");
}

static struct cmd_results *backend_cmd_del_input(int argc, char **argv,
		struct sway_backend *backend) {
	return cmd_results_new(CMD_INVALID, "backend <cmd> [args]",
			"del_input is not implemented");
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

	struct sway_backend *backend =
		sway_server_get_backend(&server, argv[0]);

	if (backend == NULL) {
		return cmd_results_new(CMD_INVALID, "backend <cmd> [args]",
				"Cannot find backend: %s", argv[0]);
	}

	if (strcasecmp("add-output", argv[1]) == 0) {
		return backend_cmd_add_output(argc_new, argv_new, backend);
	} else if (strcasecmp("del-output", argv[1]) == 0) {
		return backend_cmd_del_output(argc_new, argv_new, backend);
	} else if (strcasecmp("add-input", argv[1]) == 0) {
		return backend_cmd_add_input(argc_new, argv_new, backend);
	} else if (strcasecmp("del-input", argv[1]) == 0) {
		return backend_cmd_del_input(argc_new, argv_new, backend);
	}

	return cmd_results_new(CMD_INVALID, "backend <cmd> [args]", "Unknown command %s", argv[1]);
}
