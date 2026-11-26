#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

struct cmd_results *cmd_stacking_titlebar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	struct sway_container *container = config->handler_context.container;
	if (!container) {
		return cmd_results_new(CMD_INVALID, "No container to set");
	}

	if (strcmp(argv[0], "always_visible") == 0) {
		container->pending.stacking_titlebar_follows_border = false;
	} else if (strcmp(argv[0], "follows_border") == 0) {
		container->pending.stacking_titlebar_follows_border = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'stacking_titlebar <always_visible|follows_border>");
	}

	if (container_is_floating(container)) {
		container_set_geometry_from_content(container);
	}

	arrange_container(container);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_tabbed_titlebar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	struct sway_container *container = config->handler_context.container;
	if (!container) {
		return cmd_results_new(CMD_INVALID, "No container to set");
	}

	if (strcmp(argv[0], "always_visible") == 0) {
		container->pending.tabbed_titlebar_follows_border = false;
	} else if (strcmp(argv[0], "follows_border") == 0) {
		container->pending.tabbed_titlebar_follows_border = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'tabbed_titlebar <always_visible|follows_border>");
	}

	if (container_is_floating(container)) {
		container_set_geometry_from_content(container);
	}

	arrange_container(container);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
