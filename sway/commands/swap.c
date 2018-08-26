#define _POSIX_C_SOURCE 200809L
#include <strings.h>
#include <wlr/util/log.h>
#include "config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "stringop.h"

static const char* EXPECTED_SYNTAX =
	"Expected 'swap container with id|con_id|mark <arg>'";

static void swap_places(struct sway_container *con1,
		struct sway_container *con2) {
	struct sway_container *temp = malloc(sizeof(struct sway_container));
	temp->x = con1->x;
	temp->y = con1->y;
	temp->width = con1->width;
	temp->height = con1->height;
	temp->parent = con1->parent;

	con1->x = con2->x;
	con1->y = con2->y;
	con1->width = con2->width;
	con1->height = con2->height;

	con2->x = temp->x;
	con2->y = temp->y;
	con2->width = temp->width;
	con2->height = temp->height;

	int temp_index = container_sibling_index(con1);
	container_insert_child(con2->parent, con1, container_sibling_index(con2));
	container_insert_child(temp->parent, con2, temp_index);

	free(temp);
}

static void swap_focus(struct sway_container *con1,
		struct sway_container *con2, struct sway_seat *seat,
		struct sway_container *focus) {
	if (focus == con1 || focus == con2) {
		struct sway_container *ws1 = container_parent(con1, C_WORKSPACE);
		struct sway_container *ws2 = container_parent(con2, C_WORKSPACE);
		if (focus == con1 && (con2->parent->layout == L_TABBED
					|| con2->parent->layout == L_STACKED)) {
			if (workspace_is_visible(ws2)) {
				seat_set_focus_warp(seat, con2, false, true);
			}
			seat_set_focus(seat, ws1 != ws2 ? con2 : con1);
		} else if (focus == con2 && (con1->parent->layout == L_TABBED
					|| con1->parent->layout == L_STACKED)) {
			if (workspace_is_visible(ws1)) {
				seat_set_focus_warp(seat, con1, false, true);
			}
			seat_set_focus(seat, ws1 != ws2 ? con1 : con2);
		} else if (ws1 != ws2) {
			seat_set_focus(seat, focus == con1 ? con2 : con1);
		} else {
			seat_set_focus(seat, focus);
		}
	} else {
		seat_set_focus(seat, focus);
	}
}

static void container_swap(struct sway_container *con1,
		struct sway_container *con2) {
	if (!sway_assert(con1 && con2, "Cannot swap with nothing")) {
		return;
	}
	if (!sway_assert(con1->type >= C_CONTAINER && con2->type >= C_CONTAINER,
				"Can only swap containers and views")) {
		return;
	}
	if (!sway_assert(!container_has_ancestor(con1, con2)
				&& !container_has_ancestor(con2, con1),
				"Cannot swap ancestor and descendant")) {
		return;
	}
	if (!sway_assert(!container_is_floating(con1)
				&& !container_is_floating(con2),
				"Swapping with floating containers is not supported")) {
		return;
	}

	wlr_log(WLR_DEBUG, "Swapping containers %zu and %zu", con1->id, con2->id);

	int fs1 = con1->is_fullscreen;
	int fs2 = con2->is_fullscreen;
	if (fs1) {
		container_set_fullscreen(con1, false);
	}
	if (fs2) {
		container_set_fullscreen(con2, false);
	}

	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *vis1 = container_parent(
			seat_get_focus_inactive(seat, container_parent(con1, C_OUTPUT)),
			C_WORKSPACE);
	struct sway_container *vis2 = container_parent(
			seat_get_focus_inactive(seat, container_parent(con2, C_OUTPUT)),
			C_WORKSPACE);

	char *stored_prev_name = NULL;
	if (prev_workspace_name) {
		stored_prev_name = strdup(prev_workspace_name);
	}

	swap_places(con1, con2);

	if (!workspace_is_visible(vis1)) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, vis1));
	}
	if (!workspace_is_visible(vis2)) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, vis2));
	}

	swap_focus(con1, con2, seat, focus);

	if (stored_prev_name) {
		free(prev_workspace_name);
		prev_workspace_name = stored_prev_name;
	}

	if (fs1) {
		container_set_fullscreen(con2, true);
	}
	if (fs2) {
		container_set_fullscreen(con1, true);
	}
}

static bool test_con_id(struct sway_container *container, void *con_id) {
	return container->id == (size_t)con_id;
}

static bool test_id(struct sway_container *container, void *id) {
#ifdef HAVE_XWAYLAND
	xcb_window_t *wid = id;
	return (container->type == C_VIEW
			&& container->sway_view->type == SWAY_VIEW_XWAYLAND
			&& container->sway_view->wlr_xwayland_surface->window_id == *wid);
#else
	return false;
#endif
}

static bool test_mark(struct sway_container *container, void *mark) {
	if (container->type == C_VIEW && container->sway_view->marks->length) {
		return !list_seq_find(container->sway_view->marks,
				(int (*)(const void *, const void *))strcmp, mark);
	}
	return false;
}

struct cmd_results *cmd_swap(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swap", EXPECTED_AT_LEAST, 4))) {
		return error;
	}

	if (strcasecmp(argv[0], "container") || strcasecmp(argv[1], "with")) {
		return cmd_results_new(CMD_INVALID, "swap", EXPECTED_SYNTAX);
	}

	struct sway_container *current = config->handler_context.current_container;
	struct sway_container *other;

	char *value = join_args(argv + 3, argc - 3);
	if (strcasecmp(argv[2], "id") == 0) {
#ifdef HAVE_XWAYLAND
		xcb_window_t id = strtol(value, NULL, 0);
		other = root_find_container(test_id, (void *)&id);
#endif
	} else if (strcasecmp(argv[2], "con_id") == 0) {
		size_t con_id = atoi(value);
		other = root_find_container(test_con_id, (void *)con_id);
	} else if (strcasecmp(argv[2], "mark") == 0) {
		other = root_find_container(test_mark, (void *)value);
	} else {
		free(value);
		return cmd_results_new(CMD_INVALID, "swap", EXPECTED_SYNTAX);
	}

	if (!other) {
		error = cmd_results_new(CMD_FAILURE, "swap",
				"Failed to find %s '%s'", argv[2], value);
	} else if (current->type < C_CONTAINER || other->type < C_CONTAINER) {
		error = cmd_results_new(CMD_FAILURE, "swap",
				"Can only swap with containers and views");
	} else if (container_has_ancestor(current, other)
			|| container_has_ancestor(other, current)) {
		error = cmd_results_new(CMD_FAILURE, "swap",
				"Cannot swap ancestor and descendant");
	} else if (container_is_floating(current) || container_is_floating(other)) {
		error = cmd_results_new(CMD_FAILURE, "swap",
				"Swapping with floating containers is not supported");
	}

	free(value);

	if (error) {
		return error;
	}

	container_swap(current, other);

	arrange_windows(current->parent);
	if (other->parent != current->parent) {
		arrange_windows(other->parent);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
