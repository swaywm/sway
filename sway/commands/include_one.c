#define _XOPEN_SOURCE 700
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include "list.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"

static bool basename_in_list(list_t *basenames, const char *path) {
	char *path_copy = strdup(path);
	if (!path_copy) {
		return false;
	}
	char *base = basename(path_copy);

	for (int i = 0; i < basenames->length; ++i) {
		if (strcmp(basenames->items[i], base) == 0) {
			free(path_copy);
			return true;
		}
	}
	free(path_copy);
	return false;
}

static char *get_basename_copy(const char *path) {
	char *path_copy = strdup(path);
	if (!path_copy) {
		return NULL;
	}
	char *base = basename(path_copy);
	char *result = strdup(base);
	free(path_copy);
	return result;
}

struct cmd_results *cmd_include_one(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include_one", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	list_t *included_basenames = create_list();
	if (!included_basenames) {
		return cmd_results_new(CMD_FAILURE,
			"Unable to allocate basename list");
	}

	char *wd = getcwd(NULL, 0);
	char *parent_path = strdup(config->current_config_path);
	if (!wd || !parent_path) {
		free(wd);
		free(parent_path);
		list_free(included_basenames);
		sway_log(SWAY_ERROR, "Unable to allocate memory for include_one");
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	const char *parent_dir = dirname(parent_path);

	if (chdir(parent_dir) < 0) {
		sway_log(SWAY_ERROR, "failed to change working directory");
		goto cleanup;
	}

	for (int glob_idx = 0; glob_idx < argc; ++glob_idx) {
		wordexp_t p;
		if (wordexp(argv[glob_idx], &p, 0) != 0) {
			continue;
		}

		for (size_t i = 0; i < p.we_wordc; ++i) {
			const char *path = p.we_wordv[i];

			// For subsequent globs (not the first), skip files whose
			// basename was already included
			if (glob_idx > 0 && basename_in_list(included_basenames, path)) {
				sway_log(SWAY_DEBUG,
					"include_one: skipping %s (basename already included)",
					path);
				continue;
			}

			// Record the basename before including
			char *base_copy = get_basename_copy(path);
			if (base_copy) {
				list_add(included_basenames, base_copy);
			}

			// Use the existing load_include_configs mechanism for a single file
			// We call it directly with the path, it will handle the inclusion
			load_include_configs(path, config, &config->swaynag_config_errors);
		}

		wordfree(&p);
	}

	// Attempt to restore working directory before returning.
	if (chdir(wd) < 0) {
		sway_log(SWAY_ERROR, "failed to change working directory");
	}

cleanup:
	free(parent_path);
	free(wd);
	list_free_items_and_destroy(included_basenames);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
