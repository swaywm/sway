#define _XOPEN_SOURCE 500
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

static const char *expected_syntax =
	"Expected 'move <left|right|up|down> <[px] px>' or "
	"'move [--no-auto-back-and-forth] <container|window> [to] workspace <name>' or "
	"'move [--no-auto-back-and-forth] <container|window|workspace> [to] output <name|direction>' or "
	"'move <container|window> [to] mark <mark>'";

static struct sway_container *output_in_direction(const char *direction,
		struct wlr_output *reference, int ref_lx, int ref_ly) {
	struct {
		char *name;
		enum wlr_direction direction;
	} names[] = {
		{ "up", WLR_DIRECTION_UP },
		{ "down", WLR_DIRECTION_DOWN },
		{ "left", WLR_DIRECTION_LEFT },
		{ "right", WLR_DIRECTION_RIGHT },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (strcasecmp(names[i].name, direction) == 0) {
			struct wlr_output *adjacent = wlr_output_layout_adjacent_output(
					root_container.sway_root->output_layout,
					names[i].direction, reference, ref_lx, ref_ly);
			if (adjacent) {
				struct sway_output *sway_output = adjacent->data;
				return sway_output->swayc;
			}
			break;
		}
	}
	return output_by_name(direction);
}

static struct cmd_results *cmd_move_container(struct sway_container *current,
		int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move container/window",
				EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	if (current->type == C_WORKSPACE) {
		if (current->children->length == 0) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Can't move an empty workspace");
		}
		current = container_wrap_children(current);
	} else if (current->type != C_CONTAINER && current->type != C_VIEW) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Can only move containers and views.");
	}

	bool no_auto_back_and_forth = false;
	while (strcasecmp(argv[0], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		++argv;
	}
	while (strcasecmp(argv[1], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		argv++;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		argv++;
	}

	struct sway_container *old_parent = current->parent;
	struct sway_container *old_ws = container_parent(current, C_WORKSPACE);
	struct sway_container *destination = NULL;

	// determine destination
	if (strcasecmp(argv[1], "workspace") == 0) {
		// move container to workspace x
		struct sway_container *ws;
		if (strcasecmp(argv[2], "next") == 0 ||
				strcasecmp(argv[2], "prev") == 0 ||
				strcasecmp(argv[2], "next_on_output") == 0 ||
				strcasecmp(argv[2], "prev_on_output") == 0 ||
				strcasecmp(argv[2], "current") == 0) {
			ws = workspace_by_name(argv[2]);
		} else if (strcasecmp(argv[2], "back_and_forth") == 0) {
			if (!(ws = workspace_by_name(argv[2]))) {
				if (prev_workspace_name) {
					ws = workspace_create(NULL, prev_workspace_name);
				} else {
					return cmd_results_new(CMD_FAILURE, "move",
							"No workspace was previously active.");
				}
			}
		} else {
			char *ws_name = NULL;
			if (strcasecmp(argv[2], "number") == 0) {
				// move "container to workspace number x"
				if (argc < 4) {
					return cmd_results_new(CMD_INVALID, "move",
							expected_syntax);
				}
				ws_name = strdup(argv[3]);
				ws = workspace_by_number(ws_name);
			} else {
				ws_name = join_args(argv + 2, argc - 2);
				ws = workspace_by_name(ws_name);
			}

			if (!no_auto_back_and_forth && config->auto_back_and_forth &&
					prev_workspace_name) {
				// auto back and forth move
				if (old_ws->name && strcmp(old_ws->name, ws_name) == 0) {
					// if target workspace is the current one
					free(ws_name);
					ws_name = strdup(prev_workspace_name);
					ws = workspace_by_name(ws_name);
				}
			}

			if (!ws) {
				ws = workspace_create(NULL, ws_name);
			}
			free(ws_name);
		}
		destination = seat_get_focus_inactive(config->handler_context.seat, ws);
	} else if (strcasecmp(argv[1], "output") == 0) {
		struct sway_container *source = container_parent(current, C_OUTPUT);
		struct sway_container *dest_output = output_in_direction(argv[2],
				source->sway_output->wlr_output, current->x, current->y);
		if (!dest_output) {
			return cmd_results_new(CMD_FAILURE, "move workspace",
				"Can't find output with name/direction '%s'", argv[2]);
		}
		destination = seat_get_focus_inactive(
				config->handler_context.seat, dest_output);
		if (!destination) {
			// We've never been to this output before
			destination = dest_output->children->items[0];
		}
	} else if (strcasecmp(argv[1], "mark") == 0) {
		struct sway_view *dest_view = view_find_mark(argv[2]);
		if (dest_view == NULL) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Mark '%s' not found", argv[2]);
		}
		destination = dest_view->swayc;
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}

	// move container, arrange windows and return focus
	container_move_to(current, destination);
	struct sway_container *focus =
		seat_get_focus_inactive(config->handler_context.seat, old_parent);
	seat_set_focus_warp(config->handler_context.seat, focus, true, false);
	container_reap_empty(old_parent);
	container_reap_empty(destination->parent);

	// TODO: Ideally we would arrange the surviving parent after reaping,
	// but container_reap_empty does not return it, so we arrange the
	// workspace instead.
	arrange_windows(old_ws);
	arrange_windows(destination->parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static void workspace_move_to_output(struct sway_container *workspace,
		struct sway_container *output) {
	if (!sway_assert(workspace->type == C_WORKSPACE, "Expected a workspace")) {
		return;
	}
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	if (workspace->parent == output) {
		return;
	}
	struct sway_container *old_output = container_remove_child(workspace);
	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	struct sway_container *new_output_focus =
		seat_get_focus_inactive(seat, output);

	container_add_child(output, workspace);
	wl_signal_emit(&workspace->events.reparent, old_output);

	// If moving the last workspace from the old output, create a new workspace
	// on the old output
	if (old_output->children->length == 0) {
		char *ws_name = workspace_next_name(old_output->name);
		struct sway_container *ws = workspace_create(old_output, ws_name);
		free(ws_name);
		seat_set_focus(seat, ws);
	}

	// Try to remove an empty workspace from the destination output.
	container_reap_empty_recursive(new_output_focus);

	container_sort_workspaces(output);
	seat_set_focus(seat, output);
	workspace_output_raise_priority(workspace, old_output, output);
	ipc_event_workspace(NULL, workspace, "move");

	container_notify_subtree_changed(old_output);
	container_notify_subtree_changed(output);
}

static struct cmd_results *cmd_move_workspace(struct sway_container *current,
		int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move workspace", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		++argv;
	}

	if (strcasecmp(argv[1], "output") != 0) {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}

	struct sway_container *source = container_parent(current, C_OUTPUT);
	int center_x = current->width / 2 + current->x,
		center_y = current->height / 2 + current->y;
	struct sway_container *destination = output_in_direction(argv[2],
			source->sway_output->wlr_output, center_x, center_y);
	if (!destination) {
		return cmd_results_new(CMD_FAILURE, "move workspace",
			"Can't find output with name/direction '%s'", argv[2]);
	}
	if (current->type != C_WORKSPACE) {
		current = container_parent(current, C_WORKSPACE);
	}
	workspace_move_to_output(current, destination);

	arrange_windows(source);
	arrange_windows(destination);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *move_in_direction(struct sway_container *container,
		enum movement_direction direction, int argc, char **argv) {
	int move_amt = 10;
	if (argc > 1) {
		char *inv;
		move_amt = (int)strtol(argv[1], &inv, 10);
		if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Invalid distance specified");
		}
	}

	if (container->type == C_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Cannot move workspaces in a direction");
	}
	if (container_is_floating(container)) {
		if (container->is_fullscreen) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Cannot move fullscreen floating container");
		}
		double lx = container->x;
		double ly = container->y;
		switch (direction) {
		case MOVE_LEFT:
			lx -= move_amt;
			break;
		case MOVE_RIGHT:
			lx += move_amt;
			break;
		case MOVE_UP:
			ly -= move_amt;
			break;
		case MOVE_DOWN:
			ly += move_amt;
			break;
		case MOVE_PARENT:
		case MOVE_CHILD:
			return cmd_results_new(CMD_FAILURE, "move",
					"Cannot move floating container to parent or child");
		}
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// For simplicity, we'll arrange the entire workspace. The reason for this
	// is moving the container might reap the old parent, and container_move
	// does not return a surviving parent.
	// TODO: Make container_move return the surviving parent so we can arrange
	// just that.
	struct sway_container *old_ws = container_parent(container, C_WORKSPACE);
	container_move(container, direction, move_amt);
	struct sway_container *new_ws = container_parent(container, C_WORKSPACE);

	arrange_windows(old_ws);
	if (new_ws != old_ws) {
		arrange_windows(new_ws);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static const char *expected_position_syntax =
	"Expected 'move [absolute] position <x> [px] <y> [px]' or "
	"'move [absolute] position center|mouse'";

static struct cmd_results *move_to_position(struct sway_container *container,
		int argc, char **argv) {
	if (!container_is_floating(container)) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Only floating containers "
				"can be moved to an absolute position");
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	if (strcmp(argv[0], "absolute") == 0) {
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	if (strcmp(argv[0], "position") == 0) {
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	if (strcmp(argv[0], "mouse") == 0) {
		struct sway_seat *seat = config->handler_context.seat;
		if (!seat->cursor) {
			return cmd_results_new(CMD_FAILURE, "move", "No cursor device");
		}
		double lx = seat->cursor->cursor->x - container->width / 2;
		double ly = seat->cursor->cursor->y - container->height / 2;
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (strcmp(argv[0], "center") == 0) {
		struct sway_container *ws = container_parent(container, C_WORKSPACE);
		double lx = ws->x + (ws->width - container->width) / 2;
		double ly = ws->y + (ws->height - container->height) / 2;
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	if (argc < 2) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}

	double lx, ly;
	char *inv;
	lx = (double)strtol(argv[0], &inv, 10);
	if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Invalid position specified");
	}
	if (strcmp(argv[1], "px") == 0) {
		--argc;
		++argv;
	}

	if (argc > 3) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}

	ly = (double)strtol(argv[1], &inv, 10);
	if ((*inv != '\0' && strcasecmp(inv, "px") != 0) ||
			(argc == 3 && strcmp(argv[2], "px") != 0)) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Invalid position specified");
	}

	container_floating_move_to(container, lx, ly);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *move_to_scratchpad(struct sway_container *con) {
	if (con->type == C_WORKSPACE && con->children->length == 0) {
		return cmd_results_new(CMD_INVALID, "move",
				"Can't move an empty workspace to the scratchpad");
	}
	if (con->type == C_WORKSPACE) {
		// Wrap the workspace's children in a container
		struct sway_container *workspace = con;
		con = container_wrap_children(con);
		workspace->layout = L_HORIZ;
	}

	// If the container is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(con)) {
		while (con->parent->layout != L_FLOATING) {
			con = con->parent;
		}
	}

	if (con->scratchpad) {
		return cmd_results_new(CMD_INVALID, "move",
				"Container is already in the scratchpad");
	}
	root_scratchpad_add_container(con);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_move(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct sway_container *current = config->handler_context.current_container;

	if (strcasecmp(argv[0], "left") == 0) {
		return move_in_direction(current, MOVE_LEFT, argc, argv);
	} else if (strcasecmp(argv[0], "right") == 0) {
		return move_in_direction(current, MOVE_RIGHT, argc, argv);
	} else if (strcasecmp(argv[0], "up") == 0) {
		return move_in_direction(current, MOVE_UP, argc, argv);
	} else if (strcasecmp(argv[0], "down") == 0) {
		return move_in_direction(current, MOVE_DOWN, argc, argv);
	} else if ((strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0) ||
			(strcasecmp(argv[0], "--no-auto-back-and-forth") &&
			(strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0))) {
		return cmd_move_container(current, argc, argv);
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		return cmd_move_workspace(current, argc, argv);
	} else if (strcasecmp(argv[0], "scratchpad") == 0
			|| (strcasecmp(argv[0], "to") == 0 && argc == 2
				&& strcasecmp(argv[1], "scratchpad") == 0)) {
		return move_to_scratchpad(current);
	} else if (strcasecmp(argv[0], "position") == 0) {
		return move_to_position(current, argc, argv);
	} else if (strcasecmp(argv[0], "absolute") == 0) {
		return move_to_position(current, argc, argv);
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
