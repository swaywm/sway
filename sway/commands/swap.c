#include <strings.h>
#include <wlr/util/log.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/tree/arrange.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "stringop.h"

static const char* EXPECTED_SYNTAX =
	"Expected 'swap container with id|con_id|mark <arg>'";

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
		other = container_find(&root_container, test_id, (void *)&id);
#endif
	} else if (strcasecmp(argv[2], "con_id") == 0) {
		size_t con_id = atoi(value);
		other = container_find(&root_container, test_con_id, (void *)con_id);
	} else if (strcasecmp(argv[2], "mark") == 0) {
		other = container_find(&root_container, test_mark, (void *)value);
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
	} else if (current->layout == L_FLOATING || other->layout == L_FLOATING) {
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
