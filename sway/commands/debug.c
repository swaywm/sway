#include "sway/commands.h"
#include "sway/tree/root.h"
#include "sway/scene_descriptor.h"
#include "log.h"

struct prefix_stack {
	struct prefix_stack *parent;
	bool more_children;
};

static void print_prefix(struct prefix_stack *stack, bool top) {
	if (stack->parent) {
		print_prefix(stack->parent, false);
	}
	if (stack->more_children) {
		fprintf(stderr, top ? "  ┣━" : "  ┃ ");
	} else {
		fprintf(stderr, top ? "  ┗━" : "    ");
	}
}

static void node_type(struct wlr_scene_node *node) {
	enum sway_scene_descriptor_type types[] = {
		SWAY_SCENE_DESC_BUFFER_TIMER,
		SWAY_SCENE_DESC_NON_INTERACTIVE,
		SWAY_SCENE_DESC_CONTAINER,
		SWAY_SCENE_DESC_VIEW,
		SWAY_SCENE_DESC_LAYER_SHELL,
		SWAY_SCENE_DESC_XWAYLAND_UNMANAGED,
		SWAY_SCENE_DESC_POPUP,
		SWAY_SCENE_DESC_DRAG_ICON,
	};

	const char *names[] = {
		"buffer_timer",
		"non_interactive",
		"container",
		"view",
		"layer_shell",
		"xwayland_unmanaged",
		"popup",
		"drag_icon",
		NULL,
	};
	bool first = true;
	for (size_t idx = 0; names[idx] != NULL; idx++) {
		if (scene_descriptor_try_get(node, types[idx])) {
			fprintf(stderr, first ? " %s" : ",%s", names[idx]);
			first = false;
		}
	}
	if (node->enabled) {
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, first ? " disabled\n" : ",disabled\n");
	}
}

void *scene_descriptor_try_get(struct wlr_scene_node *node,
	enum sway_scene_descriptor_type type);

static void _dump_tree(struct wlr_scene_node *node, struct prefix_stack *parent, int x, int y) {
	if (parent) {
		print_prefix(parent, true);
	}
	switch (node->type) {
	case WLR_SCENE_NODE_TREE:
		fprintf(stderr, "tree %d,%d (%p)", x, y, (void *)node);
		break;
	case WLR_SCENE_NODE_RECT:;
		struct wlr_scene_rect *rect = (struct wlr_scene_rect *)node;
		fprintf(stderr, "rect %d,%d %dx%d (%p)", x, y, rect->width, rect->height, (void *)node);
		break;
	case WLR_SCENE_NODE_BUFFER:;
		struct wlr_scene_buffer *buffer = (struct wlr_scene_buffer *)node;
		fprintf(stderr, "buffer %d,%d %dx%d (%p)", x, y, buffer->dst_width, buffer->dst_height, (void *)node);
		break;
	default:
		fprintf(stderr, "UNKNOWN\n");
		break;
	}

	node_type(node);
	if (node->type != WLR_SCENE_NODE_TREE) {
		return;
	}
	struct wlr_scene_tree *tree = (struct wlr_scene_tree *)node;

	struct prefix_stack stack = {
		.parent = parent,
	};
	struct wlr_scene_node *child;
	wl_list_for_each(child, &tree->children, link) {
		if (child->link.next == &tree->children) {
			stack.more_children = false;
		} else {
			stack.more_children = true;
		}
		_dump_tree(child, &stack, x + child->x, y + child->y);
	}
}

static void dump_tree(struct wlr_scene_node *node) {
	_dump_tree(node, NULL, node->x, node->y);
}

struct cmd_results *cmd_dump_scene(int argc, char **argv) {
	dump_tree(&root->root_scene->tree.node);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
