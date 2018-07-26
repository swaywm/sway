#define _XOPEN_SOURCE 500
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/scratchpad.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/workspace.h"
#include "stringop.h"
#include "list.h"

static const char* expected_syntax =
	"Expected 'move <left|right|up|down> <[px] px>' or "
	"'move <container|window> to workspace <name>' or "
	"'move <container|window|workspace> to output <name|direction>' or "
	"'move position mouse'";

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
				EXPECTED_AT_LEAST, 4))) {
		return error;
	} else if (strcasecmp(argv[1], "to") == 0
			&& strcasecmp(argv[2], "workspace") == 0) {
		// move container to workspace x
		if (current->type == C_WORKSPACE) {
			// TODO: Wrap children in a container and move that
			return cmd_results_new(CMD_FAILURE, "move", "Unimplemented");
		} else if (current->type != C_CONTAINER && current->type != C_VIEW) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Can only move containers and views.");
		}
		struct sway_container *ws;
		char *ws_name = NULL;
		if (argc == 5 && strcasecmp(argv[3], "number") == 0) {
			// move "container to workspace number x"
			ws_name = strdup(argv[4]);
			ws = workspace_by_number(ws_name);
		} else {
			ws_name = join_args(argv + 3, argc - 3);
			ws = workspace_by_name(ws_name);
		}

		if (config->auto_back_and_forth && prev_workspace_name) {
			// auto back and forth move
			struct sway_container *curr_ws = container_parent(current, C_WORKSPACE);
			if (curr_ws->name && strcmp(curr_ws->name, ws_name) == 0) {
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
		struct sway_container *old_parent = current->parent;
		struct sway_container *old_ws = container_parent(current, C_WORKSPACE);
		struct sway_container *destination = seat_get_focus_inactive(
				config->handler_context.seat, ws);
		container_move_to(current, destination);
		struct sway_container *focus = seat_get_focus_inactive(
				config->handler_context.seat, old_parent);
		seat_set_focus(config->handler_context.seat, focus);
		container_reap_empty(old_parent);
		container_reap_empty(destination->parent);

		// TODO: Ideally we would arrange the surviving parent after reaping,
		// but container_reap_empty does not return it, so we arrange the
		// workspace instead.
		arrange_windows(old_ws);
		arrange_windows(destination->parent);

		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (strcasecmp(argv[1], "to") == 0
			&& strcasecmp(argv[2], "output") == 0) {
		if (current->type == C_WORKSPACE) {
			// TODO: Wrap children in a container and move that
			return cmd_results_new(CMD_FAILURE, "move", "Unimplemented");
		} else if (current->type != C_CONTAINER
				&& current->type != C_VIEW) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Can only move containers and views.");
		}
		struct sway_container *source = container_parent(current, C_OUTPUT);
		struct sway_container *destination = output_in_direction(argv[3],
				source->sway_output->wlr_output, current->x, current->y);
		if (!destination) {
			return cmd_results_new(CMD_FAILURE, "move workspace",
				"Can't find output with name/direction '%s'", argv[3]);
		}
		struct sway_container *focus = seat_get_focus_inactive(
				config->handler_context.seat, destination);
		if (!focus) {
			// We've never been to this output before
			focus = destination->children->items[0];
		}
		struct sway_container *old_parent = current->parent;
		struct sway_container *old_ws = container_parent(current, C_WORKSPACE);
		container_move_to(current, focus);
		seat_set_focus(config->handler_context.seat, old_parent);
		container_reap_empty(old_parent);
		container_reap_empty(focus->parent);

		// TODO: Ideally we would arrange the surviving parent after reaping,
		// but container_reap_empty does not return it, so we arrange the
		// workspace instead.
		arrange_windows(old_ws);
		arrange_windows(focus->parent);

		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return cmd_results_new(CMD_INVALID, "move", expected_syntax);
}

static struct cmd_results *cmd_move_workspace(struct sway_container *current,
		int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move workspace", EXPECTED_EQUAL_TO, 4))) {
		return error;
	} else if (strcasecmp(argv[1], "to") != 0
			|| strcasecmp(argv[2], "output") != 0) {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
	struct sway_container *source = container_parent(current, C_OUTPUT);
	int center_x = current->width / 2 + current->x,
		center_y = current->height / 2 + current->y;
	struct sway_container *destination = output_in_direction(argv[3],
			source->sway_output->wlr_output, center_x, center_y);
	if (!destination) {
		return cmd_results_new(CMD_FAILURE, "move workspace",
			"Can't find output with name/direction '%s'", argv[3]);
	}
	if (current->type != C_WORKSPACE) {
		current = container_parent(current, C_WORKSPACE);
	}
	container_move_to(current, destination);

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

static const char* expected_position_syntax =
	"Expected 'move [absolute] position <x> <y>' or "
	"'move [absolute] position mouse'";

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
	}
	if (argc != 2) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	double lx, ly;
	char *inv;
	lx = (double)strtol(argv[0], &inv, 10);
	if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Invalid position specified");
	}
	ly = (double)strtol(argv[1], &inv, 10);
	if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
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
	scratchpad_add_container(con);
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
	} else if (strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0) {
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
