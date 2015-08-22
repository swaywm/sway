#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlc/wlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "stringop.h"
#include "layout.h"
#include "focus.h"
#include "log.h"
#include "workspace.h"
#include "commands.h"
#include "container.h"
#include "handlers.h"
#include "sway.h"

struct modifier_key {
	char *name;
	uint32_t mod;
};

static struct modifier_key modifiers[] = {
	{ XKB_MOD_NAME_SHIFT, WLC_BIT_MOD_SHIFT },
	{ XKB_MOD_NAME_CAPS, WLC_BIT_MOD_CAPS },
	{ XKB_MOD_NAME_CTRL, WLC_BIT_MOD_CTRL },
	{ XKB_MOD_NAME_ALT, WLC_BIT_MOD_ALT },
	{ XKB_MOD_NAME_NUM, WLC_BIT_MOD_MOD2 },
	{ "Mod3", WLC_BIT_MOD_MOD3 },
	{ XKB_MOD_NAME_LOGO, WLC_BIT_MOD_LOGO },
	{ "Mod5", WLC_BIT_MOD_MOD5 },
};

enum expected_args {
	EXPECTED_MORE_THAN,
	EXPECTED_AT_LEAST,
	EXPECTED_LESS_THAN,
	EXPECTED_EQUAL_TO
};

static bool checkarg(int argc, char *name, enum expected_args type, int val) {
	switch (type) {
	case EXPECTED_MORE_THAN:
		if (argc > val) {
			return true;
		}
		sway_log(L_ERROR, "Invalid %s command."
			"(expected more than %d argument%s, got %d",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_AT_LEAST:
		if (argc >= val) {
			return true;
		}
		sway_log(L_ERROR, "Invalid %s command."
			"(expected at least %d argument%s, got %d",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_LESS_THAN:
		if (argc  < val) {
			return true;
		};
		sway_log(L_ERROR, "Invalid %s command."
			"(expected less than %d argument%s, got %d",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_EQUAL_TO:
		if (argc == val) {
			return true;
		};
		sway_log(L_ERROR, "Invalid %s command."
			"(expected %d arguments, got %d", name, val, argc);
		break;
	}
	return false;
}

static int bindsym_sort(const void *_lbind, const void *_rbind) {
	const struct sway_binding *lbind = *(void **)_lbind;
	const struct sway_binding *rbind = *(void **)_rbind;
	unsigned int lmod = 0, rmod = 0, i;

	// Count how any modifiers are pressed
	for (i = 0; i < 8 * sizeof(lbind->modifiers); ++i) {
		lmod += lbind->modifiers & 1 << i;
		rmod += rbind->modifiers & 1 << i;
	}
	return (rbind->keys->length + rmod) - (lbind->keys->length + lmod);
}

static bool cmd_bindsym(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "bindsym", EXPECTED_MORE_THAN, 1)) {
		return false;
	};

	struct sway_binding *binding = malloc(sizeof(struct sway_binding));
	binding->keys = create_list();
	binding->modifiers = 0;
	binding->command = join_args(argv + 1, argc - 1);

	list_t *split = split_string(argv[0], "+");
	int i;
	for (i = 0; i < split->length; ++i) {
		// Check for a modifier key
		int j;
		bool is_mod = false;
		for (j = 0; j < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++j) {
			if (strcasecmp(modifiers[j].name, split->items[i]) == 0) {
				binding->modifiers |= modifiers[j].mod;
				is_mod = true;
				break;
			}
		}
		if (is_mod) continue;
		// Check for xkb key
		xkb_keysym_t sym = xkb_keysym_from_name(split->items[i], XKB_KEYSYM_CASE_INSENSITIVE);
		if (!sym) {
			sway_log(L_ERROR, "bindsym - unknown key %s", (char *)split->items[i]);
			list_free(binding->keys);
			free(binding->command);
			free(binding);
			list_free(split);
			return false;
		}
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		*key = sym;
		list_add(binding->keys, key);
	}
	list_free(split);

	// TODO: Check if there are other commands with this key binding
	struct sway_mode *mode = config->current_mode;
	list_add(mode->bindings, binding);
	qsort(mode->bindings->items, mode->bindings->length,
			sizeof(mode->bindings->items[0]), bindsym_sort);

	sway_log(L_DEBUG, "bindsym - Bound %s to command %s", argv[0], binding->command);
	return true;
}

static bool cmd_exec_always(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "exec_always", EXPECTED_MORE_THAN, 0)) {
		return false;
	}

	pid_t pid = fork();
	/* Failed to fork */
	if (pid  < 0) {
		sway_log(L_ERROR, "exec command failed, sway did not fork");
		return false;
	}
	/* Child process */
	if (pid == 0) {
		char *args = join_args(argv, argc);
		sway_log(L_DEBUG, "Executing %s", args);
		execl("/bin/sh", "sh", "-c", args, (char *)NULL);
		/* Execl doesnt return unless failure */
		sway_log(L_ERROR, "could not find /bin/sh");
		free(args);
		exit(-1);
	}
	/* Parent */
	return true;
}

static bool cmd_exec(struct sway_config *config, int argc, char **argv) {
	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(L_DEBUG, "Ignoring exec %s due to reload", args);
		free(args);
		return true;
	}
	return cmd_exec_always(config, argc, argv);
}

static void kill_views(swayc_t *container, void *data) {
	if (container->type == C_VIEW) {
		wlc_view_close(container->handle);
	}
}

static bool cmd_exit(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "exit", EXPECTED_EQUAL_TO, 0)) {
		return false;
	}
	// Close all views
	container_map(&root_container, kill_views, NULL);
	sway_terminate();
	return true;
}

static bool cmd_floating(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "floating", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}

	if (strcasecmp(argv[0], "toggle") == 0) {
		swayc_t *view = get_focused_container(&root_container);
		// Prevent running floating commands on things like workspaces
		if (view->type != C_VIEW) {
			return true;
		}
		// Change from nonfloating to floating
		if (!view->is_floating) {
			// Remove view from its current location
			destroy_container(remove_child(view));

			// and move it into workspace floating
			add_floating(swayc_active_workspace(),view);
			view->x = (swayc_active_workspace()->width - view->width)/2;
			view->y = (swayc_active_workspace()->height - view->height)/2;
			if (view->desired_width != -1) {
				view->width = view->desired_width;
			}
			if (view->desired_height != -1) {
				view->height = view->desired_height;
			}
			arrange_windows(swayc_active_workspace(), -1, -1);
		} else {
			// Delete the view from the floating list and unset its is_floating flag
			// Using length-1 as the index is safe because the view must be the currently
			// focused floating output
			remove_child(view);
			view->is_floating = false;
			// Get the properly focused container, and add in the view there
			swayc_t *focused = container_under_pointer();
			// If focused is null, it's because the currently focused container is a workspace
			if (focused == NULL) {
				focused = swayc_active_workspace();
			}
			set_focused_container(focused);

			sway_log(L_DEBUG, "Non-floating focused container is %p", focused);

			// Case of focused workspace, just create as child of it
			if (focused->type == C_WORKSPACE) {
				add_child(focused, view);
			}
			// Regular case, create as sibling of current container
			else {
				add_sibling(focused, view);
			}
			// Refocus on the view once its been put back into the layout
			view->width = view->height = 0;
			arrange_windows(swayc_active_workspace(), -1, -1);
		}
		set_focused_container(view);
	}
	return true;
}

static bool cmd_floating_mod(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "floating_modifier", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}
	int i, j;
	list_t *split = split_string(argv[0], "+");
	config->floating_mod = 0;

	// set modifer keys
	for (i = 0; i < split->length; ++i) {
		for (j = 0; j < (int)(sizeof(modifiers) / sizeof(struct modifier_key)); ++j) {
			if (strcasecmp(modifiers[j].name, split->items[i]) == 0) {
				config->floating_mod |= modifiers[j].mod;
			}
		}
	}
	list_free(split);
	if (!config->floating_mod) {
		sway_log(L_ERROR, "bindsym - unknown keys %s", argv[0]);
		return false;
	}
	return true;
}

static bool cmd_focus(struct sway_config *config, int argc, char **argv) {
	static int floating_toggled_index = 0;
	static int tiled_toggled_index = 0;
	if (!checkarg(argc, "focus", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}
	if (strcasecmp(argv[0], "left") == 0) {
		return move_focus(MOVE_LEFT);
	} else if (strcasecmp(argv[0], "right") == 0) {
		return move_focus(MOVE_RIGHT);
	} else if (strcasecmp(argv[0], "up") == 0) {
		return move_focus(MOVE_UP);
	} else if (strcasecmp(argv[0], "down") == 0) {
		return move_focus(MOVE_DOWN);
	} else if (strcasecmp(argv[0], "parent") == 0) {
		return move_focus(MOVE_PARENT);
	} else if (strcasecmp(argv[0], "mode_toggle") == 0) {
		int i;
		swayc_t *workspace = swayc_active_workspace();
		swayc_t *focused = get_focused_view(workspace);
		if (focused->is_floating) {
			if (workspace->children->length > 0) {
				for (i = 0;i < workspace->floating->length; i++) {
					if (workspace->floating->items[i] == focused) {
						floating_toggled_index = i;
						break;
					}
				}
				if (workspace->children->length > tiled_toggled_index) {
					set_focused_container(get_focused_view(workspace->children->items[tiled_toggled_index]));
				} else {
					set_focused_container(get_focused_view(workspace->children->items[0]));
					tiled_toggled_index = 0;
				}
			}
		} else {
			if (workspace->floating->length > 0) {
				for (i = 0;i < workspace->children->length; i++) {
					if (workspace->children->items[i] == focused) {
						tiled_toggled_index = i;
						break;
					}
				}
				if (workspace->floating->length > floating_toggled_index) {
					swayc_t *floating = workspace->floating->items[floating_toggled_index];
					set_focused_container(get_focused_view(floating));
				} else {
					swayc_t *floating = workspace->floating->items[workspace->floating->length - 1];
					set_focused_container(get_focused_view(floating));
					tiled_toggled_index = workspace->floating->length - 1;
				}
			}
		}
	}

	return true;
}

static bool cmd_focus_follows_mouse(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "focus_follows_mouse", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}

	config->focus_follows_mouse = !strcasecmp(argv[0], "yes");
	return true;
}

static bool cmd_move(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "workspace", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}

	swayc_t *view = get_focused_container(&root_container);

	if (strcasecmp(argv[0], "left") == 0) {
		move_container(view,&root_container,MOVE_LEFT);
	} else if (strcasecmp(argv[0], "right") == 0) {
		move_container(view,&root_container,MOVE_RIGHT);
	} else if (strcasecmp(argv[0], "up") == 0) {
		move_container(view,&root_container,MOVE_UP);
	} else if (strcasecmp(argv[0], "down") == 0) {
		move_container(view,&root_container,MOVE_DOWN);
	} else {
		return false;
	}
	return true;
}

static bool cmd_gaps(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1)) {
		return false;
	}

	if (argc == 1) {
		char *end;
		int amount = (int)strtol(argv[0], &end, 10);
		if (errno == ERANGE || amount == 0) {
			errno = 0;
			return false;
		}
		if (config->gaps_inner == 0) {
			config->gaps_inner = amount;
		}
		if (config->gaps_outer == 0) {
			config->gaps_outer = amount;
		}
	} else if (argc == 2) {
		char *end;
		int amount = (int)strtol(argv[1], &end, 10);
		if (errno == ERANGE || amount == 0) {
			errno = 0;
			return false;
		}
		if (strcasecmp(argv[0], "inner") == 0) {
			config->gaps_inner = amount;
		} else if (strcasecmp(argv[0], "outer") == 0) {
			config->gaps_outer = amount;
		} else {
			return false;
		}
	} else {
		return false;
	}
	return true;
}

static bool cmd_kill(struct sway_config *config, int argc, char **argv) {
	swayc_t *view = get_focused_container(&root_container);
	wlc_view_close(view->handle);
	return true;
}

static bool cmd_layout(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "layout", EXPECTED_MORE_THAN, 0)) {
		return false;
	}
	swayc_t *parent = get_focused_container(&root_container);
	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	if (strcasecmp(argv[0], "splith") == 0) {
		parent->layout = L_HORIZ;
	} else if (strcasecmp(argv[0], "splitv") == 0) {
		parent->layout = L_VERT;
	} else if (strcasecmp(argv[0], "toggle") == 0 && argc == 2 && strcasecmp(argv[1], "split") == 0) {
		if (parent->layout == L_VERT) {
			parent->layout = L_HORIZ;
		} else {
			parent->layout = L_VERT;
		}
	}
	arrange_windows(parent, parent->width, parent->height);

	return true;
}

static bool cmd_reload(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0)) {
		return false;
	}
	if (!load_config(NULL)) { // TODO: Use config given from -c
		return false;
	}
	arrange_windows(&root_container, -1, -1);
	return true;
}

static bool cmd_resize(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "resize", EXPECTED_AT_LEAST, 3)) {
		return false;
	}
	char *end;
	int min_sane_w = 100;
	int min_sane_h = 60;
	int amount = (int)strtol(argv[2], &end, 10);
	if (errno == ERANGE || amount == 0) {
		errno = 0;
		return false;
	}
	if (strcmp(argv[0], "shrink") != 0 && strcmp(argv[0], "grow") != 0) {
		return false;
	}
	if (strcmp(argv[0], "shrink") == 0) {
		amount *= -1;
	}

	swayc_t *parent = get_focused_view(swayc_active_workspace());
	swayc_t *focused = parent;
	swayc_t *sibling;
	if (!parent) {
		return true;
	}
	// Find the closest parent container which has siblings of the proper layout.
	// Then apply the resize to all of them.
	int i;
	if (strcmp(argv[1], "width") == 0) {
		int lnumber = 0;
		int rnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_HORIZ) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->x != focused->x) {
						if (sibling->x < parent->x) {
							lnumber++;
						} else if (sibling->x > parent->x) {
							rnumber++;
						}
					}
				}
				if (rnumber || lnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent == &root_container) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d l conts, and %d r conts", parent->parent, lnumber, rnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		for (i = 0; i < parent->parent->children->length; i++) {
			bool valid = true;
			sibling = parent->parent->children->items[i];
			if (sibling->x != focused->x) {
				if (sibling->x < parent->x) {
					double pixels = -1 * amount;
					pixels /= lnumber;
					if (rnumber) {
						recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_RIGHT);
					} else {
						recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_RIGHT);
					}
				} else if (sibling->x > parent->x) {
					double pixels = -1 * amount;
					pixels /= rnumber;
					if (lnumber) {
						recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_LEFT);
					} else {
						recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_LEFT);
					}
				}
			} else {
				if (rnumber != 0 && lnumber != 0) {
					double pixels = amount;
					pixels /= 2;
					recursive_resize(parent, pixels, WLC_RESIZE_EDGE_LEFT);
					recursive_resize(parent, pixels, WLC_RESIZE_EDGE_RIGHT);
				} else if (rnumber) {
					recursive_resize(parent, amount, WLC_RESIZE_EDGE_RIGHT);
				} else if (lnumber) {
					recursive_resize(parent, amount, WLC_RESIZE_EDGE_LEFT);
				}
			}
		}
		// Recursive resize does not handle positions, let arrange_windows
		// take care of that.
		arrange_windows(swayc_active_workspace(), -1, -1);
		return true;
	} else if (strcmp(argv[1], "height") == 0) {
		int tnumber = 0;
		int bnumber = 0;
		while (parent->parent) {
			if (parent->parent->layout == L_VERT) {
				for (i = 0; i < parent->parent->children->length; i++) {
					sibling = parent->parent->children->items[i];
					if (sibling->y != focused->y) {
						if (sibling->y < parent->y) {
							bnumber++;
						} else if (sibling->y > parent->y) {
							tnumber++;
						}
					}
				}
				if (bnumber || tnumber) {
					break;
				}
			}
			parent = parent->parent;
		}
		if (parent == &root_container) {
			return true;
		}
		sway_log(L_DEBUG, "Found the proper parent: %p. It has %d b conts, and %d t conts", parent->parent, bnumber, tnumber);
		//TODO: Ensure rounding is done in such a way that there are NO pixel leaks
		for (i = 0; i < parent->parent->children->length; i++) {
			sibling = parent->parent->children->items[i];
			if (sibling->y != focused->y) {
				if (sibling->y < parent->y) {
					double pixels = -1 * amount;
					pixels /= bnumber;
					if (tnumber) {
						recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_BOTTOM);
					} else {
						recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_BOTTOM);
					}
				} else if (sibling->x > parent->x) {
					double pixels = -1 * amount;
					pixels /= tnumber;
					if (bnumber) {
						recursive_resize(sibling, pixels/2, WLC_RESIZE_EDGE_TOP);
					} else {
						recursive_resize(sibling, pixels, WLC_RESIZE_EDGE_TOP);
					}
				}
			} else {
				if (bnumber != 0 && tnumber != 0) {
					double pixels = amount/2;
					recursive_resize(parent, pixels, WLC_RESIZE_EDGE_TOP);
					recursive_resize(parent, pixels, WLC_RESIZE_EDGE_BOTTOM);
				} else if (tnumber) {
					recursive_resize(parent, amount, WLC_RESIZE_EDGE_TOP);
				} else if (bnumber) {
					recursive_resize(parent, amount, WLC_RESIZE_EDGE_BOTTOM);
				}
			}
		}
		arrange_windows(swayc_active_workspace(), -1, -1);
		return true;
	}
	return true;
}

static bool cmd_set(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "set", EXPECTED_EQUAL_TO, 2)) {
		return false;
	}
	struct sway_variable *var = malloc(sizeof(struct sway_variable));
	var->name = malloc(strlen(argv[0]) + 1);
	strcpy(var->name, argv[0]);
	var->value = malloc(strlen(argv[1]) + 1);
	strcpy(var->value, argv[1]);
	list_add(config->symbols, var);
	return true;
}

static bool _do_split(struct sway_config *config, int argc, char **argv, int layout) {
	char *name = layout == L_VERT  ? "splitv" :
		layout == L_HORIZ ? "splith" : "split";
	if (!checkarg(argc, name, EXPECTED_EQUAL_TO, 0)) {
		return false;
	}
	swayc_t *focused = get_focused_container(&root_container);

	// Case of floating window, dont split
	if (focused->is_floating) {
		return true;
	}
	/* Case that focus is on an workspace with 0/1 children.change its layout */
	if (focused->type == C_WORKSPACE && focused->children->length <= 1) {
		sway_log(L_DEBUG, "changing workspace layout");
		focused->layout = layout;
	} else if (focused->type != C_WORKSPACE && focused->parent->children->length == 1) {
		/* Case of no siblings. change parent layout */
		sway_log(L_DEBUG, "changing container layout");
		focused->parent->layout = layout;
	} else {
		/* regular case where new split container is build around focused container
		 * or in case of workspace, container inherits its children */
		sway_log(L_DEBUG, "Adding new container around current focused container");
		swayc_t *parent = new_container(focused, layout);
		set_focused_container(focused);
		arrange_windows(parent, -1, -1);
	}
	return true;
}

static bool cmd_split(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "split", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}

	if (strcasecmp(argv[0], "v") == 0 || strcasecmp(argv[0], "vertical") == 0) {
		_do_split(config, argc - 1, argv + 1, L_VERT);
	} else if (strcasecmp(argv[0], "h") == 0 || strcasecmp(argv[0], "horizontal") == 0) {
		_do_split(config, argc - 1, argv + 1, L_HORIZ);
	} else {
		sway_log(L_ERROR, "Invalid split command (expected either horiziontal or vertical).");
		return false;
	}

	return true;
}

static bool cmd_splitv(struct sway_config *config, int argc, char **argv) {
	return _do_split(config, argc, argv, L_VERT);
}

static bool cmd_splith(struct sway_config *config, int argc, char **argv) {
	return _do_split(config, argc, argv, L_HORIZ);
}

static bool cmd_log_colors(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "log_colors", EXPECTED_EQUAL_TO, 1)) {
		return false;
	}
	if (strcasecmp(argv[0], "no") != 0 && strcasecmp(argv[0], "yes") != 0) {
		sway_log(L_ERROR, "Invalid log_colors command (expected `yes` or `no`, got '%s')", argv[0]);
		return false;
	}

	sway_log_colors(!strcasecmp(argv[0], "yes"));
	return true;
}

static bool cmd_fullscreen(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "fullscreen", EXPECTED_AT_LEAST, 0)) {
		return false;
	}

	swayc_t *container = get_focused_view(&root_container);
	bool current = swayc_is_fullscreen(container);
	wlc_view_set_state(container->handle, WLC_BIT_FULLSCREEN, !current);
	// Resize workspace if going from  fullscreen -> notfullscreen
	// otherwise just resize container
	if (current) {
		container = swayc_parent_by_type(container, C_WORKSPACE);
	}
	// Only resize container when going into fullscreen
	arrange_windows(container, -1, -1);

	return true;
}

static bool cmd_workspace(struct sway_config *config, int argc, char **argv) {
	if (!checkarg(argc, "workspace", EXPECTED_AT_LEAST, 1)) {
		return false;
	}

	if (argc == 1) {
		// Handle workspace next/prev
		if (strcmp(argv[0], "next") == 0) {
			workspace_next();
			return true;
		}

		if (strcmp(argv[0], "prev") == 0) {
			workspace_next();
			return true;
		}

		// Handle workspace output_next/prev
		if (strcmp(argv[0], "next_on_output") == 0) {
			workspace_output_next();
			return true;
		}

		if (strcmp(argv[0], "prev_on_output") == 0) {
			workspace_output_prev();
			return true;
		}

		swayc_t *workspace = workspace_by_name(argv[0]);
		if (!workspace) {
			workspace = workspace_create(argv[0]);
		}
		workspace_switch(workspace);
	} else {
		if (strcasecmp(argv[1], "output") == 0) {
			if (!checkarg(argc, "workspace", EXPECTED_EQUAL_TO, 3)) {
				return false;
			}
			struct workspace_output *wso = calloc(1, sizeof(struct workspace_output));
			sway_log(L_DEBUG, "Assigning workspace %s to output %s", argv[0], argv[2]);
			wso->workspace = strdup(argv[0]);
			wso->output = strdup(argv[2]);
			list_add(config->workspace_outputs, wso);
			// TODO: Consider moving any existing workspace to that output? This might be executed sometime after config load
		}
	}
	return true;
}

/* Keep alphabetized */
static struct cmd_handler handlers[] = {
	{ "bindsym", cmd_bindsym },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "exit", cmd_exit },
	{ "floating", cmd_floating },
	{ "floating_modifier", cmd_floating_mod },
	{ "focus", cmd_focus },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "fullscreen", cmd_fullscreen },
	{ "gaps", cmd_gaps },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "log_colors", cmd_log_colors },
	{ "move", cmd_move},
	{ "reload", cmd_reload },
	{ "resize", cmd_resize },
	{ "set", cmd_set },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitv", cmd_splitv },
	{ "workspace", cmd_workspace }
};

static char **split_directive(char *line, int *argc) {
	const char *delimiters = " ";
	*argc = 0;
	while (isspace(*line) && *line) ++line;

	int capacity = 10;
	char **parts = malloc(sizeof(char *) * capacity);

	if (!*line) return parts;

	int in_string = 0, in_character = 0;
	int i, j, _;
	for (i = 0, j = 0; line[i]; ++i) {
		if (line[i] == '\\') {
			++i;
		} else if (line[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (line[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (strchr(delimiters, line[i]) != NULL) {
				char *item = malloc(i - j + 1);
				strncpy(item, line + j, i - j);
				item[i - j] = '\0';
				item = strip_whitespace(item, &_);
				if (item[0] == '\0') {
					free(item);
				} else {
					if (*argc == capacity) {
						capacity *= 2;
						parts = realloc(parts, sizeof(char *) * capacity);
					}
					parts[*argc] = item;
					j = i + 1;
					++*argc;
				}
			}
		}
	}
	char *item = malloc(i - j + 1);
	strncpy(item, line + j, i - j);
	item[i - j] = '\0';
	item = strip_whitespace(item, &_);
	if (*argc == capacity) {
		capacity++;
		parts = realloc(parts, sizeof(char *) * capacity);
	}
	parts[*argc] = item;
	++*argc;
	return parts;
}

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

static struct cmd_handler *find_handler(struct cmd_handler handlers[], int l, char *line) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = bsearch(&d, handlers, l, sizeof(struct cmd_handler), handler_compare);
	return res;
}

bool handle_command(struct sway_config *config, char *exec) {
	sway_log(L_INFO, "Handling command '%s'", exec);
	char *ptr, *cmd;
	bool exec_success;

	if ((ptr = strchr(exec, ' ')) == NULL) {
		cmd = exec;
	} else {
		int index = ptr - exec;
		cmd = malloc(index + 1);
		strncpy(cmd, exec, index);
		cmd[index] = '\0';
	}
	struct cmd_handler *handler = find_handler(handlers, sizeof(handlers) / sizeof(struct cmd_handler), cmd);
	if (handler == NULL) {
		sway_log(L_ERROR, "Unknown command '%s'", cmd);
		exec_success = false; // TODO: return error, probably
	} else {
		int argc;
		char **argv = split_directive(exec + strlen(handler->command), &argc);
		int i;

		 // Perform var subs on all parts of the command
		 for (i = 0; i < argc; ++i) {
			 argv[i] = do_var_replacement(config, argv[i]);
		 }

		exec_success = handler->handle(config, argc, argv);
		for (i = 0; i < argc; ++i) {
			free(argv[i]);
		}
		free(argv);
		if (!exec_success) {
			sway_log(L_ERROR, "Command failed: %s", cmd);
		}
	}
	if (ptr) {
		free(cmd);
	}
	return exec_success;
}
