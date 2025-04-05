#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_buffer.h>
#include "sway/config.h"
#include "sway/scene_descriptor.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"

struct sway_transaction {
	struct wl_event_source *timer;
	list_t *instructions;   // struct sway_transaction_instruction *
	size_t num_waiting;
	size_t num_configures;
	struct timespec commit_time;
};

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_node *node;
	union {
		struct sway_output_state output_state;
		struct sway_workspace_state workspace_state;
		struct sway_container_state container_state;
	};
	uint32_t serial;
	bool server_request;
	bool waiting;
};

static struct sway_transaction *transaction_create(void) {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	if (!sway_assert(transaction, "Unable to allocate transaction")) {
		return NULL;
	}
	transaction->instructions = create_list();
	return transaction;
}

static void transaction_destroy(struct sway_transaction *transaction) {
	// Free instructions
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		node->ntxnrefs--;
		if (node->instruction == instruction) {
			node->instruction = NULL;
		}
		if (node->destroying && node->ntxnrefs == 0) {
			switch (node->type) {
			case N_ROOT:
				sway_assert(false, "Never reached");
				break;
			case N_OUTPUT:
				output_destroy(node->sway_output);
				break;
			case N_WORKSPACE:
				workspace_destroy(node->sway_workspace);
				break;
			case N_CONTAINER:
				container_destroy(node->sway_container);
				break;
			}
		}
		free(instruction);
	}
	list_free(transaction->instructions);

	if (transaction->timer) {
		wl_event_source_remove(transaction->timer);
	}
	free(transaction);
}

static void copy_output_state(struct sway_output *output,
		struct sway_transaction_instruction *instruction) {
	struct sway_output_state *state = &instruction->output_state;
	if (state->workspaces) {
		state->workspaces->length = 0;
	} else {
		state->workspaces = create_list();
	}
	list_cat(state->workspaces, output->workspaces);

	state->active_workspace = output_get_active_workspace(output);
}

static void copy_workspace_state(struct sway_workspace *ws,
		struct sway_transaction_instruction *instruction) {
	struct sway_workspace_state *state = &instruction->workspace_state;

	state->fullscreen = ws->fullscreen;
	state->x = ws->x;
	state->y = ws->y;
	state->width = ws->width;
	state->height = ws->height;
	state->layout = ws->layout;

	state->output = ws->output;
	if (state->floating) {
		state->floating->length = 0;
	} else {
		state->floating = create_list();
	}
	if (state->tiling) {
		state->tiling->length = 0;
	} else {
		state->tiling = create_list();
	}
	list_cat(state->floating, ws->floating);
	list_cat(state->tiling, ws->tiling);

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &ws->node;

	// Set focused_inactive_child to the direct tiling child
	struct sway_container *focus = seat_get_focus_inactive_tiling(seat, ws);
	if (focus) {
		while (focus->pending.parent) {
			focus = focus->pending.parent;
		}
	}
	state->focused_inactive_child = focus;
}

static void copy_container_state(struct sway_container *container,
		struct sway_transaction_instruction *instruction) {
	struct sway_container_state *state = &instruction->container_state;

	if (state->children) {
		list_free(state->children);
	}

	memcpy(state, &container->pending, sizeof(struct sway_container_state));

	if (!container->view) {
		// We store a copy of the child list to avoid having it mutated after
		// we copy the state.
		state->children = create_list();
		list_cat(state->children, container->pending.children);
	} else {
		state->children = NULL;
	}

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &container->node;

	if (!container->view) {
		struct sway_node *focus =
			seat_get_active_tiling_child(seat, &container->node);
		state->focused_inactive_child = focus ? focus->sway_container : NULL;
	}
}

static void transaction_add_node(struct sway_transaction *transaction,
		struct sway_node *node, bool server_request) {
	struct sway_transaction_instruction *instruction = NULL;

	// Check if we have an instruction for this node already, in which case we
	// update that instead of creating a new one.
	if (node->ntxnrefs > 0) {
		for (int idx = 0; idx < transaction->instructions->length; idx++) {
			struct sway_transaction_instruction *other =
				transaction->instructions->items[idx];
			if (other->node == node) {
				instruction = other;
				break;
			}
		}
	}

	if (!instruction) {
		instruction = calloc(1, sizeof(struct sway_transaction_instruction));
		if (!sway_assert(instruction, "Unable to allocate instruction")) {
			return;
		}
		instruction->transaction = transaction;
		instruction->node = node;
		instruction->server_request = server_request;

		list_add(transaction->instructions, instruction);
		node->ntxnrefs++;
	} else if (server_request) {
		instruction->server_request = true;
	}

	switch (node->type) {
	case N_ROOT:
		break;
	case N_OUTPUT:
		copy_output_state(node->sway_output, instruction);
		break;
	case N_WORKSPACE:
		copy_workspace_state(node->sway_workspace, instruction);
		break;
	case N_CONTAINER:
		copy_container_state(node->sway_container, instruction);
		break;
	}
}

static void apply_output_state(struct sway_output *output,
		struct sway_output_state *state) {
	list_free(output->current.workspaces);
	memcpy(&output->current, state, sizeof(struct sway_output_state));
}

static void apply_workspace_state(struct sway_workspace *ws,
		struct sway_workspace_state *state) {
	list_free(ws->current.floating);
	list_free(ws->current.tiling);
	memcpy(&ws->current, state, sizeof(struct sway_workspace_state));
}

static void apply_container_state(struct sway_container *container,
		struct sway_container_state *state) {
	struct sway_view *view = container->view;
	// There are separate children lists for each instruction state, the
	// container's current state and the container's pending state
	// (ie. con->children). The list itself needs to be freed here.
	// Any child containers which are being deleted will be cleaned up in
	// transaction_destroy().
	list_free(container->current.children);

	memcpy(&container->current, state, sizeof(struct sway_container_state));

	if (view) {
		if (view->saved_surface_tree) {
			if (!container->node.destroying || container->node.ntxnrefs == 1) {
				view_remove_saved_buffer(view);
			}
		}

		// If the view hasn't responded to the configure, center it within
		// the container. This is important for fullscreen views which
		// refuse to resize to the size of the output.
		if (view->surface) {
			view_center_and_clip_surface(view);
		}
	}
}

static void arrange_title_bar(struct sway_container *con,
		int x, int y, int width, int height) {
	container_update(con);

	bool has_title_bar = height > 0;
	wlr_scene_node_set_enabled(&con->title_bar.tree->node, has_title_bar);
	if (!has_title_bar) {
		return;
	}

	wlr_scene_node_set_position(&con->title_bar.tree->node, x, y);

	con->title_width = width;
	container_arrange_title_bar(con);
}

static void disable_container(struct sway_container *con) {
	if (con->view) {
		wlr_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
	} else {
		for (int i = 0; i < con->current.children->length; i++) {
			struct sway_container *child = con->current.children->items[i];

			wlr_scene_node_reparent(&child->scene_tree->node, con->content_tree);

			disable_container(child);
		}
	}
}

static void arrange_container(struct sway_container *con,
		int width, int height, bool title_bar, int gaps);

static void arrange_children(enum sway_container_layout layout, list_t *children,
		struct sway_container *active, struct wlr_scene_tree *content,
		int width, int height, int gaps) {
	int title_bar_height = container_titlebar_height();

	if (layout == L_TABBED) {
		struct sway_container *first = children->length == 1 ?
			((struct sway_container *)children->items[0]) : NULL;
		if (config->hide_lone_tab && first && first->view &&
				first->current.border != B_NORMAL) {
			title_bar_height = 0;
		}

		double w = (double) width / children->length;
		int title_offset = 0;
		for (int i = 0; i < children->length; i++) {
			struct sway_container *child = children->items[i];
			bool activated = child == active;
			int next_title_offset = round(w * i + w);

			arrange_title_bar(child, title_offset, -title_bar_height,
				next_title_offset - title_offset, title_bar_height);
			wlr_scene_node_set_enabled(&child->border.tree->node, activated);
			wlr_scene_node_set_enabled(&child->scene_tree->node, true);
			wlr_scene_node_set_position(&child->scene_tree->node, 0, title_bar_height);
			wlr_scene_node_reparent(&child->scene_tree->node, content);

			int net_height = height - title_bar_height;
			if (activated && width > 0 && net_height > 0) {
				arrange_container(child, width, net_height, title_bar_height == 0, 0);
			} else {
				disable_container(child);
			}

			title_offset = next_title_offset;
		}
	} else if (layout == L_STACKED) {
		struct sway_container *first = children->length == 1 ?
			((struct sway_container *)children->items[0]) : NULL;
		if (config->hide_lone_tab && first && first->view &&
				first->current.border != B_NORMAL) {
			title_bar_height = 0;
		}

		int title_height = title_bar_height * children->length;

		int y = 0;
		for (int i = 0; i < children->length; i++) {
			struct sway_container *child = children->items[i];
			bool activated = child == active;

			arrange_title_bar(child, 0, y - title_height, width, title_bar_height);
			wlr_scene_node_set_enabled(&child->border.tree->node, activated);
			wlr_scene_node_set_enabled(&child->scene_tree->node, true);
			wlr_scene_node_set_position(&child->scene_tree->node, 0, title_height);
			wlr_scene_node_reparent(&child->scene_tree->node, content);

			int net_height = height - title_height;
			if (activated && width > 0 && net_height > 0) {
				arrange_container(child, width, net_height, title_bar_height == 0, 0);
			} else {
				disable_container(child);
			}

			y += title_bar_height;
		}
	} else if (layout == L_VERT) {
		int off = 0;
		for (int i = 0; i < children->length; i++) {
			struct sway_container *child = children->items[i];
			int cheight = child->current.height;

			wlr_scene_node_set_enabled(&child->border.tree->node, true);
			wlr_scene_node_set_position(&child->scene_tree->node, 0, off);
			wlr_scene_node_reparent(&child->scene_tree->node, content);
			if (width > 0 && cheight > 0) {
				arrange_container(child, width, cheight, true, gaps);
				off += cheight + gaps;
			} else {
				disable_container(child);
			}
		}
	} else if (layout == L_HORIZ) {
		int off = 0;
		for (int i = 0; i < children->length; i++) {
			struct sway_container *child = children->items[i];
			int cwidth = child->current.width;

			wlr_scene_node_set_enabled(&child->border.tree->node, true);
			wlr_scene_node_set_position(&child->scene_tree->node, off, 0);
			wlr_scene_node_reparent(&child->scene_tree->node, content);
			if (cwidth > 0 && height > 0) {
				arrange_container(child, cwidth, height, true, gaps);
				off += cwidth + gaps;
			} else {
				disable_container(child);
			}
		}
	} else {
		sway_assert(false, "unreachable");
	}
}

static void arrange_container(struct sway_container *con,
		int width, int height, bool title_bar, int gaps) {
	// this container might have previously been in the scratchpad,
	// make sure it's enabled for viewing
	wlr_scene_node_set_enabled(&con->scene_tree->node, true);

	if (con->output_handler) {
		wlr_scene_buffer_set_dest_size(con->output_handler, width, height);
	}

	if (con->view) {
		int border_top = container_titlebar_height();
		int border_width = con->current.border_thickness;

		if (title_bar && con->current.border != B_NORMAL) {
			wlr_scene_node_set_enabled(&con->title_bar.tree->node, false);
			wlr_scene_node_set_enabled(&con->border.top->node, true);
		} else {
			wlr_scene_node_set_enabled(&con->border.top->node, false);
		}

		if (con->current.border == B_NORMAL) {
			if (title_bar) {
				arrange_title_bar(con, 0, 0, width, border_top);
			} else {
				border_top = 0;
				// should be handled by the parent container
			}
		} else if (con->current.border == B_PIXEL) {
			container_update(con);
			border_top = title_bar && con->current.border_top ? border_width : 0;
		} else if (con->current.border == B_NONE) {
			container_update(con);
			border_top = 0;
			border_width = 0;
		} else if (con->current.border == B_CSD) {
			border_top = 0;
			border_width = 0;
		} else {
			sway_assert(false, "unreachable");
		}

		int border_bottom = con->current.border_bottom ? border_width : 0;
		int border_left = con->current.border_left ? border_width : 0;
		int border_right = con->current.border_right ? border_width : 0;
		int vert_border_height = MAX(0, height - border_top - border_bottom);

		wlr_scene_rect_set_size(con->border.top, width, border_top);
		wlr_scene_rect_set_size(con->border.bottom, width, border_bottom);
		wlr_scene_rect_set_size(con->border.left,
			border_left, vert_border_height);
		wlr_scene_rect_set_size(con->border.right,
			border_right, vert_border_height);

		wlr_scene_node_set_position(&con->border.top->node, 0, 0);
		wlr_scene_node_set_position(&con->border.bottom->node,
			0, height - border_bottom);
		wlr_scene_node_set_position(&con->border.left->node,
			0, border_top);
		wlr_scene_node_set_position(&con->border.right->node,
			width - border_right, border_top);

		// make sure to reparent, it's possible that the client just came out of
		// fullscreen mode where the parent of the surface is not the container
		wlr_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
		wlr_scene_node_set_position(&con->view->scene_tree->node,
			border_left, border_top);
	} else {
		// make sure to disable the title bar if the parent is not managing it
		if (title_bar) {
			wlr_scene_node_set_enabled(&con->title_bar.tree->node, false);
		}

		arrange_children(con->current.layout, con->current.children,
			con->current.focused_inactive_child, con->content_tree,
			width, height, gaps);
	}
}

static int container_get_gaps(struct sway_container *con) {
	struct sway_workspace *ws = con->current.workspace;
	struct sway_container *temp = con;
	while (temp) {
		enum sway_container_layout layout;
		if (temp->current.parent) {
			layout = temp->current.parent->current.layout;
		} else {
			layout = ws->current.layout;
		}
		if (layout == L_TABBED || layout == L_STACKED) {
			return 0;
		}
		temp = temp->pending.parent;
	}
	return ws->gaps_inner;
}

static void arrange_fullscreen(struct wlr_scene_tree *tree,
		struct sway_container *fs, struct sway_workspace *ws,
		int width, int height) {
	struct wlr_scene_node *fs_node;
	if (fs->view) {
		fs_node = &fs->view->scene_tree->node;

		// if we only care about the view, disable any decorations
		wlr_scene_node_set_enabled(&fs->scene_tree->node, false);
	} else {
		fs_node = &fs->scene_tree->node;
		arrange_container(fs, width, height, true, container_get_gaps(fs));
	}

	wlr_scene_node_reparent(fs_node, tree);
	wlr_scene_node_lower_to_bottom(fs_node);
	wlr_scene_node_set_position(fs_node, 0, 0);
}

static void arrange_workspace_floating(struct sway_workspace *ws) {
	for (int i = 0; i < ws->current.floating->length; i++) {
		struct sway_container *floater = ws->current.floating->items[i];
		struct wlr_scene_tree *layer = root->layers.floating;

		if (floater->current.fullscreen_mode != FULLSCREEN_NONE) {
			continue;
		}

		if (root->fullscreen_global) {
			if (container_is_transient_for(floater, root->fullscreen_global)) {
				layer = root->layers.fullscreen_global;
			}
		} else {
			for (int i = 0; i < root->outputs->length; i++) {
				struct sway_output *output = root->outputs->items[i];
				struct sway_workspace *active = output->current.active_workspace;

				if (active && active->fullscreen &&
						container_is_transient_for(floater, active->fullscreen)) {
					layer = root->layers.fullscreen;
				}
			}
		}

		wlr_scene_node_reparent(&floater->scene_tree->node, layer);
		wlr_scene_node_set_position(&floater->scene_tree->node,
			floater->current.x, floater->current.y);
		wlr_scene_node_set_enabled(&floater->scene_tree->node, true);
		wlr_scene_node_set_enabled(&floater->border.tree->node, true);

		arrange_container(floater, floater->current.width, floater->current.height,
			true, ws->gaps_inner);
	}
}

static void arrange_workspace_tiling(struct sway_workspace *ws,
		int width, int height) {
	arrange_children(ws->current.layout, ws->current.tiling,
		ws->current.focused_inactive_child, ws->layers.tiling,
		width, height, ws->gaps_inner);
}

static void disable_workspace(struct sway_workspace *ws) {
	// if any containers were just moved to a disabled workspace it will
	// have the parent of the old workspace. Move the workspace so that it won't
	// be shown.
	for (int i = 0; i < ws->current.tiling->length; i++) {
		struct sway_container *child = ws->current.tiling->items[i];

		wlr_scene_node_reparent(&child->scene_tree->node, ws->layers.tiling);
		disable_container(child);
	}

	for (int i = 0; i < ws->current.floating->length; i++) {
		struct sway_container *floater = ws->current.floating->items[i];
		wlr_scene_node_reparent(&floater->scene_tree->node, root->layers.floating);
		disable_container(floater);
		wlr_scene_node_set_enabled(&floater->scene_tree->node, false);
	}
}

static void arrange_output(struct sway_output *output, int width, int height) {
	for (int i = 0; i < output->current.workspaces->length; i++) {
		struct sway_workspace *child = output->current.workspaces->items[i];

		bool activated = output->current.active_workspace == child && output->wlr_output->enabled;

		wlr_scene_node_reparent(&child->layers.tiling->node, output->layers.tiling);
		wlr_scene_node_reparent(&child->layers.fullscreen->node, output->layers.fullscreen);

		for (int i = 0; i < child->current.floating->length; i++) {
			struct sway_container *floater = child->current.floating->items[i];
			wlr_scene_node_reparent(&floater->scene_tree->node, root->layers.floating);
			wlr_scene_node_set_enabled(&floater->scene_tree->node, activated);
		}

		if (activated) {
			struct sway_container *fs = child->current.fullscreen;
			wlr_scene_node_set_enabled(&child->layers.tiling->node, !fs);
			wlr_scene_node_set_enabled(&child->layers.fullscreen->node, fs);

			wlr_scene_node_set_enabled(&output->layers.shell_background->node, !fs);
			wlr_scene_node_set_enabled(&output->layers.shell_bottom->node, !fs);
			wlr_scene_node_set_enabled(&output->layers.fullscreen->node, fs);

			if (fs) {
				disable_workspace(child);

				wlr_scene_rect_set_size(output->fullscreen_background, width, height);

				arrange_workspace_floating(child);
				arrange_fullscreen(child->layers.fullscreen, fs, child,
					width, height);
			} else {
				struct wlr_box *area = &output->usable_area;
				struct side_gaps *gaps = &child->current_gaps;

				wlr_scene_node_set_position(&child->layers.tiling->node,
					gaps->left + area->x, gaps->top + area->y);

				arrange_workspace_tiling(child,
					area->width - gaps->left - gaps->right,
					area->height - gaps->top - gaps->bottom);
				arrange_workspace_floating(child);
			}
		} else {
			wlr_scene_node_set_enabled(&child->layers.tiling->node, false);
			wlr_scene_node_set_enabled(&child->layers.fullscreen->node, false);

			disable_workspace(child);
		}
	}
}

void arrange_popups(struct wlr_scene_tree *popups) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &popups->children, link) {
		struct sway_popup_desc *popup = scene_descriptor_try_get(node,
			SWAY_SCENE_DESC_POPUP);

		if (popup) {
			double lx, ly;
			wlr_scene_node_coords(popup->relative, &lx, &ly);
			wlr_scene_node_set_position(node, lx, ly);
		}
	}
}

static void arrange_root(struct sway_root *root) {
	struct sway_container *fs = root->fullscreen_global;

	wlr_scene_node_set_enabled(&root->layers.shell_background->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.shell_bottom->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.tiling->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.floating->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.shell_top->node, !fs);
	wlr_scene_node_set_enabled(&root->layers.fullscreen->node, !fs);

	// hide all contents in the scratchpad
	for (int i = 0; i < root->scratchpad->length; i++) {
		struct sway_container *con = root->scratchpad->items[i];

		// When a container is moved to a scratchpad, it's possible that it
		// was moved into a floating container as part of the same transaction.
		// In this case, we need to make sure we reparent all the container's
		// children so that disabling the container will disable all descendants.
		if (!con->view) for (int ii = 0; ii < con->current.children->length; ii++) {
			struct sway_container *child = con->current.children->items[ii];
			wlr_scene_node_reparent(&child->scene_tree->node, con->content_tree);
		}

		wlr_scene_node_set_enabled(&con->scene_tree->node, false);
	}

	if (fs) {
		for (int i = 0; i < root->outputs->length; i++) {
			struct sway_output *output = root->outputs->items[i];
			struct sway_workspace *ws = output->current.active_workspace;

			wlr_scene_output_set_position(output->scene_output, output->lx, output->ly);

			// disable all workspaces to get to a known state
			for (int j = 0; j < output->current.workspaces->length; j++) {
				struct sway_workspace *workspace = output->current.workspaces->items[j];
				disable_workspace(workspace);
			}

			// arrange the active workspace
			if (ws) {
				arrange_workspace_floating(ws);
			}
		}

		arrange_fullscreen(root->layers.fullscreen_global, fs, NULL,
			root->width, root->height);
	} else {
		for (int i = 0; i < root->outputs->length; i++) {
			struct sway_output *output = root->outputs->items[i];

			wlr_scene_output_set_position(output->scene_output, output->lx, output->ly);

			wlr_scene_node_reparent(&output->layers.shell_background->node, root->layers.shell_background);
			wlr_scene_node_reparent(&output->layers.shell_bottom->node, root->layers.shell_bottom);
			wlr_scene_node_reparent(&output->layers.tiling->node, root->layers.tiling);
			wlr_scene_node_reparent(&output->layers.shell_top->node, root->layers.shell_top);
			wlr_scene_node_reparent(&output->layers.shell_overlay->node, root->layers.shell_overlay);
			wlr_scene_node_reparent(&output->layers.fullscreen->node, root->layers.fullscreen);
			wlr_scene_node_reparent(&output->layers.session_lock->node, root->layers.session_lock);

			wlr_scene_node_set_position(&output->layers.shell_background->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_bottom->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.tiling->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.fullscreen->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_top->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.shell_overlay->node, output->lx, output->ly);
			wlr_scene_node_set_position(&output->layers.session_lock->node, output->lx, output->ly);

			arrange_output(output, output->width, output->height);
		}
	}

	arrange_popups(root->layers.popup);
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Applying transaction %p", transaction);
	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *commit = &transaction->commit_time;
		float ms = (now.tv_sec - commit->tv_sec) * 1000 +
			(now.tv_nsec - commit->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %.1fms waiting "
				"(%.1f frames if 60Hz)", transaction, ms, ms / (1000.0f / 60));
	}

	// Apply the instruction state to the node's current state
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;

		switch (node->type) {
		case N_ROOT:
			break;
		case N_OUTPUT:
			apply_output_state(node->sway_output, &instruction->output_state);
			break;
		case N_WORKSPACE:
			apply_workspace_state(node->sway_workspace,
					&instruction->workspace_state);
			break;
		case N_CONTAINER:
			apply_container_state(node->sway_container,
					&instruction->container_state);
			break;
		}

		node->instruction = NULL;
	}
}

static void transaction_commit_pending(void);

static void transaction_progress(void) {
	if (!server.queued_transaction) {
		return;
	}
	if (server.queued_transaction->num_waiting > 0) {
		return;
	}
	transaction_apply(server.queued_transaction);
	arrange_root(root);
	cursor_rebase_all();
	transaction_destroy(server.queued_transaction);
	server.queued_transaction = NULL;

	if (!server.pending_transaction) {
		sway_idle_inhibit_v1_check_active();
		return;
	}

	transaction_commit_pending();
}

static int handle_timeout(void *data) {
	struct sway_transaction *transaction = data;
	sway_log(SWAY_DEBUG, "Transaction %p timed out (%zi waiting)",
			transaction, transaction->num_waiting);
	transaction->num_waiting = 0;
	transaction_progress();
	return 0;
}

static bool should_configure(struct sway_node *node,
		struct sway_transaction_instruction *instruction) {
	if (!node_is_view(node)) {
		return false;
	}
	if (node->destroying) {
		return false;
	}
	if (!instruction->server_request) {
		return false;
	}
	struct sway_container_state *cstate = &node->sway_container->current;
	struct sway_container_state *istate = &instruction->container_state;
#if WLR_HAS_XWAYLAND
	// Xwayland views are position-aware and need to be reconfigured
	// when their position changes.
	if (node->sway_container->view->type == SWAY_VIEW_XWAYLAND) {
		// Sway logical coordinates are doubles, but they get truncated to
		// integers when sent to Xwayland through `xcb_configure_window`.
		// X11 apps will not respond to duplicate configure requests (from their
		// truncated point of view) and cause transactions to time out.
		if ((int)cstate->content_x != (int)istate->content_x ||
				(int)cstate->content_y != (int)istate->content_y) {
			return true;
		}
	}
#endif
	if (cstate->content_width == istate->content_width &&
			cstate->content_height == istate->content_height) {
		return false;
	}
	return true;
}

static void transaction_commit(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Transaction %p committing with %i instructions",
			transaction, transaction->instructions->length);
	transaction->num_waiting = 0;
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		bool hidden = node_is_view(node) && !node->destroying &&
			!view_is_visible(node->sway_container->view);
		if (should_configure(node, instruction)) {
			instruction->serial = view_configure(node->sway_container->view,
					instruction->container_state.content_x,
					instruction->container_state.content_y,
					instruction->container_state.content_width,
					instruction->container_state.content_height);
			if (!hidden) {
				instruction->waiting = true;
				++transaction->num_waiting;
			}

			view_send_frame_done(node->sway_container->view);
		}
		if (!hidden && node_is_view(node) &&
				!node->sway_container->view->saved_surface_tree) {
			view_save_buffer(node->sway_container->view);
		}
		node->instruction = instruction;
	}
	transaction->num_configures = transaction->num_waiting;
	if (debug.txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->commit_time);
	}
	if (debug.noatomic) {
		transaction->num_waiting = 0;
	} else if (debug.txn_wait) {
		// Force the transaction to time out even if all views are ready.
		// We do this by inflating the waiting counter.
		transaction->num_waiting += 1000000;
	}

	if (transaction->num_waiting) {
		// Set up a timer which the views must respond within
		transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
				handle_timeout, transaction);
		if (transaction->timer) {
			wl_event_source_timer_update(transaction->timer,
					server.txn_timeout_ms);
		} else {
			sway_log_errno(SWAY_ERROR, "Unable to create transaction timer "
					"(some imperfect frames might be rendered)");
			transaction->num_waiting = 0;
		}
	}
}

static void transaction_commit_pending(void) {
	if (server.queued_transaction) {
		return;
	}
	struct sway_transaction *transaction = server.pending_transaction;
	server.pending_transaction = NULL;
	server.queued_transaction = transaction;
	transaction_commit(transaction);
	transaction_progress();
}

static void set_instruction_ready(
		struct sway_transaction_instruction *instruction) {
	struct sway_transaction *transaction = instruction->transaction;

	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *start = &transaction->commit_time;
		float ms = (now.tv_sec - start->tv_sec) * 1000 +
			(now.tv_nsec - start->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %zi/%zi ready in %.1fms (%s)",
				transaction,
				transaction->num_configures - transaction->num_waiting + 1,
				transaction->num_configures, ms,
				instruction->node->sway_container->title);
	}

	// If the transaction has timed out then its num_waiting will be 0 already.
	if (instruction->waiting && transaction->num_waiting > 0 &&
			--transaction->num_waiting == 0) {
		sway_log(SWAY_DEBUG, "Transaction %p is ready", transaction);
		wl_event_source_timer_update(transaction->timer, 0);
	}

	instruction->node->instruction = NULL;
	transaction_progress();
}

bool transaction_notify_view_ready_by_serial(struct sway_view *view,
		uint32_t serial) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction != NULL && instruction->serial == serial) {
		set_instruction_ready(instruction);
		return true;
	}
	return false;
}

bool transaction_notify_view_ready_by_geometry(struct sway_view *view,
		double x, double y, int width, int height) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction != NULL &&
			(int)instruction->container_state.content_x == (int)x &&
			(int)instruction->container_state.content_y == (int)y &&
			instruction->container_state.content_width == width &&
			instruction->container_state.content_height == height) {
		set_instruction_ready(instruction);
		return true;
	}
	return false;
}

static void _transaction_commit_dirty(bool server_request) {
	if (!server.dirty_nodes->length) {
		return;
	}

	if (!server.pending_transaction) {
		server.pending_transaction = transaction_create();
		if (!server.pending_transaction) {
			return;
		}
	}

	for (int i = 0; i < server.dirty_nodes->length; ++i) {
		struct sway_node *node = server.dirty_nodes->items[i];
		transaction_add_node(server.pending_transaction, node, server_request);
		node->dirty = false;
	}
	server.dirty_nodes->length = 0;

	transaction_commit_pending();
}

void transaction_commit_dirty(void) {
	_transaction_commit_dirty(true);
}

void transaction_commit_dirty_client(void) {
	_transaction_commit_dirty(false);
}
