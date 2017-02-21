#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <wordexp.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <float.h>
#include <libinput.h>
#include "sway/layout.h"
#include "sway/focus.h"
#include "sway/workspace.h"
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/output.h"
#include "sway/handlers.h"
#include "sway/input_state.h"
#include "sway/criteria.h"
#include "sway/ipc-server.h"
#include "sway/security.h"
#include "sway/input.h"
#include "sway/border.h"
#include "stringop.h"
#include "sway.h"
#include "util.h"
#include "list.h"
#include "log.h"

struct cmd_handler {
	char *command;
	sway_cmd *handle;
};

int sp_index = 0;

// Returns error object, or NULL if check succeeds.
struct cmd_results *checkarg(int argc, const char *name, enum expected_args type, int val) {
	struct cmd_results *error = NULL;
	switch (type) {
	case EXPECTED_MORE_THAN:
		if (argc > val) {
			return NULL;
		}
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected more than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_AT_LEAST:
		if (argc >= val) {
			return NULL;
		}
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected at least %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_LESS_THAN:
		if (argc  < val) {
			return NULL;
		};
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected less than %d argument%s, got %d)",
			name, val, (char*[2]){"s", ""}[argc==1], argc);
		break;
	case EXPECTED_EQUAL_TO:
		if (argc == val) {
			return NULL;
		};
		error = cmd_results_new(CMD_INVALID, name, "Invalid %s command "
			"(expected %d arguments, got %d)", name, val, argc);
		break;
	}
	return error;
}

void hide_view_in_scratchpad(swayc_t *sp_view) {
	if (sp_view == NULL) {
		return;
	}

	wlc_view_set_mask(sp_view->handle, 0);
	sp_view->visible = false;
	swayc_t *ws = sp_view->parent;
	remove_child(sp_view);
	if (swayc_active_workspace() != ws && ws->floating->length == 0 && ws->children->length == 0) {
		destroy_workspace(ws);
	} else {
		arrange_windows(ws, -1, -1);
	}
	set_focused_container(container_under_pointer());
}

void input_cmd_apply(struct input_config *input) {
	int i;
	i = list_seq_find(config->input_configs, input_identifier_cmp, input->identifier);
	if (i >= 0) {
		// merge existing config
		struct input_config *ic = config->input_configs->items[i];
		merge_input_config(ic, input);
		free_input_config(input);
		input = ic;
	} else {
		list_add(config->input_configs, input);
	}

	current_input_config = input;

	if (input->identifier) {
		// Try to find the input device and apply configuration now. If
		// this is during startup then there will be no container and config
		// will be applied during normal "new input" event from wlc.
		struct libinput_device *device = NULL;
		for (int i = 0; i < input_devices->length; ++i) {
			device = input_devices->items[i];
			char* dev_identifier = libinput_dev_unique_id(device);
			if (!dev_identifier) {
				break;
			}
			int match = dev_identifier && strcmp(dev_identifier, input->identifier) == 0;
			free(dev_identifier);
			if (match) {
				apply_input_config(input, device);
				break;
			}
		}
	}
}

void remove_view_from_scratchpad(swayc_t *view) {
	int i;
	for (i = 0; i < scratchpad->length; i++) {
		if (scratchpad->items[i] == view) {
			if (sp_index == 0) {
				sp_index = scratchpad->length - 1;
			} else {
				sp_index--;
			}
			list_del(scratchpad, sp_index);
			sp_view = NULL;
		}
	}
}

/* Keep alphabetized */
static struct cmd_handler handlers[] = {
	{ "assign", cmd_assign },
	{ "bar", cmd_bar },
	{ "bindcode", cmd_bindcode },
	{ "bindsym", cmd_bindsym },
	{ "border", cmd_border },
	{ "client.background", cmd_client_background },
	{ "client.focused", cmd_client_focused },
	{ "client.focused_inactive", cmd_client_focused_inactive },
	{ "client.placeholder", cmd_client_placeholder },
	{ "client.unfocused", cmd_client_unfocused },
	{ "client.urgent", cmd_client_urgent },
	{ "commands", cmd_commands },
	{ "debuglog", cmd_debuglog },
	{ "default_orientation", cmd_orientation },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "exit", cmd_exit },
	{ "floating", cmd_floating },
	{ "floating_maximum_size", cmd_floating_maximum_size },
	{ "floating_minimum_size", cmd_floating_minimum_size },
	{ "floating_modifier", cmd_floating_mod },
	{ "floating_scroll", cmd_floating_scroll },
	{ "focus", cmd_focus },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "font", cmd_font },
	{ "for_window", cmd_for_window },
	{ "force_focus_wrapping", cmd_force_focus_wrapping },
	{ "fullscreen", cmd_fullscreen },
	{ "gaps", cmd_gaps },
	{ "hide_edge_borders", cmd_hide_edge_borders },
	{ "include", cmd_include },
	{ "input", cmd_input },
	{ "ipc", cmd_ipc },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "log_colors", cmd_log_colors },
	{ "mode", cmd_mode },
	{ "mouse_warping", cmd_mouse_warping },
	{ "move", cmd_move },
	{ "new_float", cmd_new_float },
	{ "new_window", cmd_new_window },
	{ "output", cmd_output },
	{ "permit", cmd_permit },
	{ "reject", cmd_reject },
	{ "reload", cmd_reload },
	{ "resize", cmd_resize },
	{ "scratchpad", cmd_scratchpad },
	{ "seamless_mouse", cmd_seamless_mouse },
	{ "set", cmd_set },
	{ "smart_gaps", cmd_smart_gaps },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitt", cmd_splitt },
	{ "splitv", cmd_splitv },
	{ "sticky", cmd_sticky },
	{ "workspace", cmd_workspace },
	{ "workspace_auto_back_and_forth", cmd_ws_auto_back_and_forth },
	{ "workspace_layout", cmd_workspace_layout },
};

static struct cmd_handler bar_handlers[] = {
	{ "binding_mode_indicator", bar_cmd_binding_mode_indicator },
	{ "bindsym", bar_cmd_bindsym },
	{ "colors", bar_cmd_colors },
	{ "font", bar_cmd_font },
	{ "height", bar_cmd_height },
	{ "hidden_state", bar_cmd_hidden_state },
	{ "id", bar_cmd_id },
	{ "mode", bar_cmd_mode },
	{ "modifier", bar_cmd_modifier },
	{ "output", bar_cmd_output },
	{ "pango_markup", bar_cmd_pango_markup },
	{ "position", bar_cmd_position },
	{ "separator_symbol", bar_cmd_separator_symbol },
	{ "status_command", bar_cmd_status_command },
	{ "strip_workspace_numbers", bar_cmd_strip_workspace_numbers },
	{ "swaybar_command", bar_cmd_swaybar_command },
	{ "tray_output", bar_cmd_tray_output },
	{ "tray_padding", bar_cmd_tray_padding },
	{ "workspace_buttons", bar_cmd_workspace_buttons },
	{ "wrap_scroll", bar_cmd_wrap_scroll },
};

/**
 * Check and add color to buffer.
 *
 * return error object, or NULL if color is valid.
 */
struct cmd_results *add_color(const char *name, char *buffer, const char *color) {
	int len = strlen(color);
	if (len != 7 && len != 9 ) {
		return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
	}

	if (color[0] != '#') {
		return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
	}

	int i;
	for (i = 1; i < len; ++i) {
		if (!isxdigit(color[i])) {
			return cmd_results_new(CMD_INVALID, name, "Invalid color definition %s", color);
		}
	}

	// copy color to buffer
	strncpy(buffer, color, len);
	// add default alpha channel if color was defined without it
	if (len == 7) {
		buffer[7] = 'f';
		buffer[8] = 'f';
	}
	buffer[9] = '\0';

	return NULL;
}

static struct cmd_handler input_handlers[] = {
	{ "accel_profile", input_cmd_accel_profile },
	{ "click_method", input_cmd_click_method },
	{ "drag_lock", input_cmd_drag_lock },
	{ "dwt", input_cmd_dwt },
	{ "events", input_cmd_events },
	{ "left_handed", input_cmd_left_handed },
	{ "middle_emulation", input_cmd_middle_emulation },
	{ "natural_scroll", input_cmd_natural_scroll },
	{ "pointer_accel", input_cmd_pointer_accel },
	{ "scroll_method", input_cmd_scroll_method },
	{ "tap", input_cmd_tap },
};

static struct cmd_handler bar_colors_handlers[] = {
	{ "active_workspace", bar_colors_cmd_active_workspace },
	{ "background", bar_colors_cmd_background },
	{ "binding_mode", bar_colors_cmd_binding_mode },
	{ "focused_background", bar_colors_cmd_focused_background },
	{ "focused_separator", bar_colors_cmd_focused_separator },
	{ "focused_statusline", bar_colors_cmd_focused_statusline },
	{ "focused_workspace", bar_colors_cmd_focused_workspace },
	{ "inactive_workspace", bar_colors_cmd_inactive_workspace },
	{ "separator", bar_colors_cmd_separator },
	{ "statusline", bar_colors_cmd_statusline },
	{ "urgent_workspace", bar_colors_cmd_urgent_workspace },
};

static struct cmd_handler ipc_handlers[] = {
	{ "*", cmd_ipc_cmd },
	{ "bar-config", cmd_ipc_cmd },
	{ "command", cmd_ipc_cmd },
	{ "events", cmd_ipc_events },
	{ "inputs", cmd_ipc_cmd },
	{ "marks", cmd_ipc_cmd },
	{ "outputs", cmd_ipc_cmd },
	{ "tree", cmd_ipc_cmd },
	{ "workspaces", cmd_ipc_cmd },
};

static struct cmd_handler ipc_event_handlers[] = {
	{ "*", cmd_ipc_event_cmd },
	{ "binding", cmd_ipc_event_cmd },
	{ "input", cmd_ipc_event_cmd },
	{ "mode", cmd_ipc_event_cmd },
	{ "output", cmd_ipc_event_cmd },
	{ "window", cmd_ipc_event_cmd },
	{ "workspace", cmd_ipc_event_cmd },
};

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

static struct cmd_handler *find_handler(char *line, enum cmd_status block) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = NULL;
	sway_log(L_DEBUG, "find_handler(%s) %d", line, block == CMD_BLOCK_INPUT);
	if (block == CMD_BLOCK_BAR) {
		res = bsearch(&d, bar_handlers,
			sizeof(bar_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_BAR_COLORS){
		res = bsearch(&d, bar_colors_handlers,
			sizeof(bar_colors_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_INPUT) {
		res = bsearch(&d, input_handlers,
			sizeof(input_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_IPC) {
		res = bsearch(&d, ipc_handlers,
			sizeof(ipc_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_IPC_EVENTS) {
		res = bsearch(&d, ipc_event_handlers,
			sizeof(ipc_event_handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	} else {
		res = bsearch(&d, handlers,
			sizeof(handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);
	}
	return res;
}

struct cmd_results *handle_command(char *_exec, enum command_context context) {
	// Even though this function will process multiple commands we will only
	// return the last error, if any (for now). (Since we have access to an
	// error string we could e.g. concatonate all errors there.)
	struct cmd_results *results = NULL;
	char *exec = strdup(_exec);
	char *head = exec;
	char *cmdlist;
	char *cmd;
	char *criteria __attribute__((unused));

	head = exec;
	do {
		// Extract criteria (valid for this command list only).
		criteria = NULL;
		if (*head == '[') {
			++head;
			criteria = argsep(&head, "]");
			if (head) {
				++head;
				// TODO handle criteria
			} else {
				if (!results) {
					results = cmd_results_new(CMD_INVALID, criteria, "Unmatched [");
				}
				goto cleanup;
			}
			// Skip leading whitespace
			head += strspn(head, whitespace);

			// TODO: it will yield unexpected results to execute commands
			// (on any view) that where meant for certain views only.
			if (!results) {
				int len = strlen(criteria) + strlen(head) + 4;
				char *tmp = malloc(len);
				if (tmp) {
					snprintf(tmp, len, "[%s] %s", criteria, head);
				} else {
					sway_log(L_DEBUG, "Unable to allocate criteria string for cmd result");
				}
				results = cmd_results_new(CMD_INVALID, tmp,
					"Can't handle criteria string: Refusing to execute command");
				free(tmp);
			}
			goto cleanup;
		}
		// Split command list
		cmdlist = argsep(&head, ";");
		cmdlist += strspn(cmdlist, whitespace);
		do {
			// Split commands
			cmd = argsep(&cmdlist, ",");
			cmd += strspn(cmd, whitespace);
			if (strcmp(cmd, "") == 0) {
				sway_log(L_INFO, "Ignoring empty command.");
				continue;
			}
			sway_log(L_INFO, "Handling command '%s'", cmd);
			//TODO better handling of argv
			int argc;
			char **argv = split_args(cmd, &argc);
			if (strcmp(argv[0], "exec") != 0) {
				int i;
				for (i = 1; i < argc; ++i) {
					if (*argv[i] == '\"' || *argv[i] == '\'') {
						strip_quotes(argv[i]);
					}
				}
			}
			struct cmd_handler *handler = find_handler(argv[0], CMD_BLOCK_END);
			if (!handler) {
				if (results) {
					free_cmd_results(results);
				}
				results = cmd_results_new(CMD_INVALID, cmd, "Unknown/invalid command");
				free_argv(argc, argv);
				goto cleanup;
			}
			if (!(get_command_policy(argv[0]) & context)) {
				if (results) {
					free_cmd_results(results);
				}
				results = cmd_results_new(CMD_INVALID, cmd,
						"Permission denied for %s via %s", cmd,
						command_policy_str(context));
				free_argv(argc, argv);
				goto cleanup;
			}
			struct cmd_results *res = handler->handle(argc-1, argv+1);
			if (res->status != CMD_SUCCESS) {
				free_argv(argc, argv);
				if (results) {
					free_cmd_results(results);
				}
				results = res;
				goto cleanup;
			}
			free_argv(argc, argv);
			free_cmd_results(res);
		} while(cmdlist);
	} while(head);
	cleanup:
	free(exec);
	if (!results) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return results;
}

// this is like handle_command above, except:
// 1) it ignores empty commands (empty lines)
// 2) it does variable substitution
// 3) it doesn't split commands (because the multiple commands are supposed to
//	  be chained together)
// 4) handle_command handles all state internally while config_command has some
//	  state handled outside (notably the block mode, in read_config)
struct cmd_results *config_command(char *exec, enum cmd_status block) {
	struct cmd_results *results = NULL;
	int argc;
	char **argv = split_args(exec, &argc);
	if (!argc) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
		goto cleanup;
	}

	sway_log(L_INFO, "handling config command '%s'", exec);
	// Endblock
	if (**argv == '}') {
		results = cmd_results_new(CMD_BLOCK_END, NULL, NULL);
		goto cleanup;
	}
	struct cmd_handler *handler = find_handler(argv[0], block);
	if (!handler) {
		char *input = argv[0] ? argv[0] : "(empty)";
		results = cmd_results_new(CMD_INVALID, input, "Unknown/invalid command");
		goto cleanup;
	}
	int i;
	// Var replacement, for all but first argument of set
	for (i = handler->handle == cmd_set ? 2 : 1; i < argc; ++i) {
		argv[i] = do_var_replacement(argv[i]);
		unescape_string(argv[i]);
	}
	/* Strip quotes for first argument.
	 * TODO This part needs to be handled much better */
	if (argc>1 && (*argv[1] == '\"' || *argv[1] == '\'')) {
		strip_quotes(argv[1]);
	}
	if (handler->handle) {
		results = handler->handle(argc-1, argv+1);
	} else {
		results = cmd_results_new(CMD_INVALID, argv[0], "This command is shimmed, but unimplemented");
	}

cleanup:
	free_argv(argc, argv);
	return results;
}

struct cmd_results *config_commands_command(char *exec) {
	struct cmd_results *results = NULL;
	int argc;
	char **argv = split_args(exec, &argc);
	if (!argc) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
		goto cleanup;
	}

	// Find handler for the command this is setting a policy for
	char *cmd = argv[0];

	if (strcmp(cmd, "}") == 0) {
		results = cmd_results_new(CMD_BLOCK_END, NULL, NULL);
		goto cleanup;
	}

	struct cmd_handler *handler = find_handler(cmd, CMD_BLOCK_END);
	if (!handler && strcmp(cmd, "*") != 0) {
		char *input = cmd ? cmd : "(empty)";
		results = cmd_results_new(CMD_INVALID, input, "Unknown/invalid command");
		goto cleanup;
	}

	enum command_context context = 0;

	struct {
		char *name;
		enum command_context context;
	} context_names[] = {
		{ "config", CONTEXT_CONFIG },
		{ "binding", CONTEXT_BINDING },
		{ "ipc", CONTEXT_IPC },
		{ "criteria", CONTEXT_CRITERIA },
		{ "all", CONTEXT_ALL },
	};

	for (int i = 1; i < argc; ++i) {
		size_t j;
		for (j = 0; j < sizeof(context_names) / sizeof(context_names[0]); ++j) {
			if (strcmp(context_names[j].name, argv[i]) == 0) {
				break;
			}
		}
		if (j == sizeof(context_names) / sizeof(context_names[0])) {
			results = cmd_results_new(CMD_INVALID, cmd,
					"Invalid command context %s", argv[i]);
			goto cleanup;
		}
		context |= context_names[j].context;
	}

	struct command_policy *policy = NULL;
	for (int i = 0; i < config->command_policies->length; ++i) {
		struct command_policy *p = config->command_policies->items[i];
		if (strcmp(p->command, cmd) == 0) {
			policy = p;
			break;
		}
	}
	if (!policy) {
		policy = alloc_command_policy(cmd);
		if (!policy) {
			sway_abort("Unable to allocate security policy");
		}
		list_add(config->command_policies, policy);
	}
	policy->context = context;

	sway_log(L_INFO, "Set command policy for %s to %d",
			policy->command, policy->context);

	results = cmd_results_new(CMD_SUCCESS, NULL, NULL);

cleanup:
	free_argv(argc, argv);
	return results;
}

struct cmd_results *cmd_results_new(enum cmd_status status, const char* input, const char *format, ...) {
	struct cmd_results *results = malloc(sizeof(struct cmd_results));
	if (!results) {
		sway_log(L_ERROR, "Unable to allocate command results");
		return NULL;
	}
	results->status = status;
	if (input) {
		results->input = strdup(input); // input is the command name
	} else {
		results->input = NULL;
	}
	if (format) {
		char *error = malloc(256);
		va_list args;
		va_start(args, format);
		if (error) {
			vsnprintf(error, 256, format, args);
		}
		va_end(args);
		results->error = error;
	} else {
		results->error = NULL;
	}
	return results;
}

void free_cmd_results(struct cmd_results *results) {
	if (results->input) {
		free(results->input);
	}
	if (results->error) {
		free(results->error);
	}
	free(results);
}

const char *cmd_results_to_json(struct cmd_results *results) {
	json_object *result_array = json_object_new_array();
	json_object *root = json_object_new_object();
	json_object_object_add(root, "success", json_object_new_boolean(results->status == CMD_SUCCESS));
	if (results->input) {
		json_object_object_add(root, "input", json_object_new_string(results->input));
	}
	if (results->error) {
		json_object_object_add(root, "error", json_object_new_string(results->error));
	}
	json_object_array_add(result_array, root);
	const char *json = json_object_to_json_string(result_array);
	free(result_array);
	free(root);
	return json;
}
