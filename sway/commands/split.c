#include <string.h>
#include <strings.h>
#include <wlc/wlc-render.h>
#include "sway/border.h"
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"
#include "sway/layout.h"
#include "log.h"

static struct cmd_results *_do_split(int argc, char **argv, int layout) {
	char *name = layout == L_VERT  ? "splitv" :
		layout == L_HORIZ ? "splith" : "split";
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, name, "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, name, "Can only be used when sway is running.");
	if ((error = checkarg(argc, name, EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	swayc_t *focused = current_container;

	// Case of floating window, don't split
	if (focused->is_floating) {
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	/* Case that focus is on an workspace with 0/1 children.change its layout */
	if (focused->type == C_WORKSPACE && focused->children->length <= 1) {
		sway_log(L_DEBUG, "changing workspace layout");
		swayc_change_layout(focused, layout);
	} else if (focused->type != C_WORKSPACE && focused->parent->children->length == 1) {
		/* Case of no siblings. change parent layout */
		sway_log(L_DEBUG, "changing container layout");
		swayc_change_layout(focused->parent, layout);
	} else {
		/* regular case where new split container is build around focused container
		 * or in case of workspace, container inherits its children */
		sway_log(L_DEBUG, "Adding new container around current focused container");
		sway_log(L_INFO, "FOCUSED SIZE: %.f %.f", focused->width, focused->height);
		swayc_t *parent = new_container(focused, layout);
		set_focused_container(focused);
		arrange_windows(parent, -1, -1);
	}

	// update container every time
	// if it is tabbed/stacked then the title must change
	// if the indicator color is different then the border must change
	update_container_border(focused);
	swayc_t *output = swayc_parent_by_type(focused, C_OUTPUT);
	// schedule render to make changes take effect right away,
	// otherwise we would have to wait for the view to render,
	// which is unpredictable.
	wlc_output_schedule_render(output->handle);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_split(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "split", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "split", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "split", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		return _do_split(argc - 1, argv + 1, L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 || strcasecmp(argv[0], "horizontal") == 0) {
		return _do_split(argc - 1, argv + 1, L_HORIZ);
	} else if (strcasecmp(argv[0], "t") == 0 || strcasecmp(argv[0], "toggle") == 0) {
		swayc_t *focused = current_container;
		if (focused->parent->layout == L_VERT) {
			return _do_split(argc - 1, argv + 1, L_HORIZ);
		} else {
			return _do_split(argc - 1, argv + 1, L_VERT);
		}
	} else {
		error = cmd_results_new(CMD_FAILURE, "split",
			"Invalid split command (expected either horizontal or vertical).");
		return error;
	}
}

struct cmd_results *cmd_splitv(int argc, char **argv) {
	return _do_split(argc, argv, L_VERT);
}

struct cmd_results *cmd_splith(int argc, char **argv) {
	return _do_split(argc, argv, L_HORIZ);
}

struct cmd_results *cmd_splitt(int argc, char **argv) {
	swayc_t *focused = current_container;
	if (focused->parent->layout == L_VERT) {
		return _do_split(argc, argv, L_HORIZ);
	} else {
		return _do_split(argc, argv, L_VERT);
	}
}
