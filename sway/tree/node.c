#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "log.h"

void node_init(struct sway_node *node, enum sway_node_type type, void *thing) {
	static size_t next_id = 1;
	node->id = next_id++;
	node->type = type;
	node->sway_root = thing;
	wl_signal_init(&node->events.destroy);
}

const char *node_type_to_str(enum sway_node_type type) {
	switch (type) {
	case N_ROOT:
		return "root";
	case N_OUTPUT:
		return "output";
	case N_WORKSPACE:
		return "workspace";
	case N_CONTAINER:
		return "container";
	}
	return "";
}

void node_set_dirty(struct sway_node *node) {
	if (node->dirty) {
		return;
	}
	node->dirty = true;
	list_add(server.dirty_nodes, node);
}

bool node_is_view(struct sway_node *node) {
	return node->type == N_CONTAINER && node->sway_container->view;
}

char *node_get_name(struct sway_node *node) {
	switch (node->type) {
	case N_ROOT:
		return "root";
	case N_OUTPUT:
		return node->sway_output->wlr_output->name;
	case N_WORKSPACE:
		return node->sway_workspace->name;
	case N_CONTAINER:
		return node->sway_container->title;
	}
	return NULL;
}

void node_get_box(struct sway_node *node, struct wlr_box *box) {
	switch (node->type) {
	case N_ROOT:
		root_get_box(root, box);
		break;
	case N_OUTPUT:
		output_get_box(node->sway_output, box);
		break;
	case N_WORKSPACE:
		workspace_get_box(node->sway_workspace, box);
		break;
	case N_CONTAINER:
		container_get_box(node->sway_container, box);
		break;
	}
}

struct sway_output *node_get_output(struct sway_node *node) {
	switch (node->type) {
	case N_CONTAINER: {
		struct sway_workspace *ws = node->sway_container->pending.workspace;
		return ws ? ws->output : NULL;
    }
	case N_WORKSPACE:
		return node->sway_workspace->output;
	case N_OUTPUT:
		return node->sway_output;
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

enum sway_container_layout node_get_layout(struct sway_node *node) {
	switch (node->type) {
	case N_CONTAINER:
		return node->sway_container->pending.layout;
	case N_WORKSPACE:
		return node->sway_workspace->layout;
	case N_OUTPUT:
	case N_ROOT:
		return L_NONE;
	}
	return L_NONE;
}

struct sway_node *node_get_parent(struct sway_node *node) {
	switch (node->type) {
	case N_CONTAINER: {
			struct sway_container *con = node->sway_container;
			if (con->pending.parent) {
				return &con->pending.parent->node;
			}
			if (con->pending.workspace) {
				return &con->pending.workspace->node;
			}
		}
		return NULL;
	case N_WORKSPACE: {
			struct sway_workspace *ws = node->sway_workspace;
			if (ws->output) {
				return &ws->output->node;
			}
		}
		return NULL;
	case N_OUTPUT:
		return &root->node;
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

list_t *node_get_children(struct sway_node *node) {
	switch (node->type) {
	case N_CONTAINER:
		return node->sway_container->pending.children;
	case N_WORKSPACE:
		return node->sway_workspace->tiling;
	case N_OUTPUT:
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

bool node_has_ancestor(struct sway_node *node, struct sway_node *ancestor) {
	if (ancestor->type == N_ROOT && node->type == N_CONTAINER &&
			node->sway_container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		return true;
	}
	struct sway_node *parent = node_get_parent(node);
	while (parent) {
		if (parent == ancestor) {
			return true;
		}
		if (ancestor->type == N_ROOT && parent->type == N_CONTAINER &&
				parent->sway_container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
			return true;
		}
		parent = node_get_parent(parent);
	}
	return false;
}

void scene_node_disown_children(struct wlr_scene_tree *tree) {
	// this function can be called as part of destruction code that will be invoked
	// upon an allocation failure. Let's not crash on NULL due to an allocation error.
	if (!tree) {
		return;
	}

	struct wlr_scene_node *child, *tmp_child;
	wl_list_for_each_safe(child, tmp_child, &tree->children, link) {
		wlr_scene_node_reparent(child, root->staging);
	}
}

struct wlr_scene_tree *alloc_scene_tree(struct wlr_scene_tree *parent,
		bool *failed) {
	// fallthrough
	if (*failed) {
		return NULL;
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_create(parent);
	if (!tree) {
		sway_log(SWAY_ERROR, "Failed to allocate a scene node");
		*failed = true;
	}

	return tree;
}
