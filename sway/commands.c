#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <json-c/json.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/criteria.h"
#include "sway/security.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "stringop.h"
#include "log.h"

struct cmd_handler {
	char *command;
	sway_cmd *handle;
};

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

void apply_input_config(struct input_config *input) {
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

	input_manager_apply_input_config(input_manager, input);
}

void apply_seat_config(struct seat_config *seat_config) {
	int i;
	i = list_seq_find(config->seat_configs, seat_name_cmp, seat_config->name);
	if (i >= 0) {
		// merge existing config
		struct seat_config *sc = config->seat_configs->items[i];
		merge_seat_config(sc, seat_config);
		free_seat_config(seat_config);
		seat_config = sc;
	} else {
		list_add(config->seat_configs, seat_config);
	}

	input_manager_apply_seat_config(input_manager, seat_config);
}

/* Keep alphabetized */
static struct cmd_handler handlers[] = {
	{ "assign", cmd_assign },
	{ "bar", cmd_bar },
	{ "bindcode", cmd_bindcode },
	{ "bindsym", cmd_bindsym },
	{ "exec", cmd_exec },
	{ "exec_always", cmd_exec_always },
	{ "focus_follows_mouse", cmd_focus_follows_mouse },
	{ "for_window", cmd_for_window },
	{ "fullscreen", cmd_fullscreen },
	{ "include", cmd_include },
	{ "input", cmd_input },
	{ "mode", cmd_mode },
	{ "mouse_warping", cmd_mouse_warping },
	{ "output", cmd_output },
	{ "seat", cmd_seat },
	{ "workspace", cmd_workspace },
	{ "workspace_auto_back_and_forth", cmd_ws_auto_back_and_forth },
};

static struct cmd_handler bar_handlers[] = {
	{ "activate_button", bar_cmd_activate_button },
	{ "binding_mode_indicator", bar_cmd_binding_mode_indicator },
	{ "bindsym", bar_cmd_bindsym },
	{ "colors", bar_cmd_colors },
	{ "context_button", bar_cmd_context_button },
	{ "font", bar_cmd_font },
	{ "height", bar_cmd_height },
	{ "hidden_state", bar_cmd_hidden_state },
	{ "icon_theme", bar_cmd_icon_theme },
	{ "id", bar_cmd_id },
	{ "mode", bar_cmd_mode },
	{ "modifier", bar_cmd_modifier },
	{ "output", bar_cmd_output },
	{ "pango_markup", bar_cmd_pango_markup },
	{ "position", bar_cmd_position },
	{ "secondary_button", bar_cmd_secondary_button },
	{ "separator_symbol", bar_cmd_separator_symbol },
	{ "status_command", bar_cmd_status_command },
	{ "strip_workspace_numbers", bar_cmd_strip_workspace_numbers },
	{ "swaybar_command", bar_cmd_swaybar_command },
	{ "tray_output", bar_cmd_tray_output },
	{ "tray_padding", bar_cmd_tray_padding },
	{ "workspace_buttons", bar_cmd_workspace_buttons },
	{ "wrap_scroll", bar_cmd_wrap_scroll },
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

/* Config-time only commands. Keep alphabetized */
static struct cmd_handler config_handlers[] = {
	{ "default_orientation", cmd_default_orientation },
	{ "set", cmd_set },
	{ "swaybg_command", cmd_swaybg_command },
};

/* Runtime-only commands. Keep alphabetized */
static struct cmd_handler command_handlers[] = {
	{ "exit", cmd_exit },
	{ "focus", cmd_focus },
	{ "kill", cmd_kill },
	{ "layout", cmd_layout },
	{ "move", cmd_move },
	{ "opacity", cmd_opacity },
	{ "reload", cmd_reload },
	{ "rename", cmd_rename },
	{ "resize", cmd_resize },
	{ "split", cmd_split },
	{ "splith", cmd_splith },
	{ "splitt", cmd_splitt },
	{ "splitv", cmd_splitv },
};

static int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

// must be in order for the bsearch
static struct cmd_handler input_handlers[] = {
	{ "accel_profile", input_cmd_accel_profile },
	{ "click_method", input_cmd_click_method },
	{ "drag_lock", input_cmd_drag_lock },
	{ "dwt", input_cmd_dwt },
	{ "events", input_cmd_events },
	{ "left_handed", input_cmd_left_handed },
	{ "map_to_output", input_cmd_map_to_output },
	{ "middle_emulation", input_cmd_middle_emulation },
	{ "natural_scroll", input_cmd_natural_scroll },
	{ "pointer_accel", input_cmd_pointer_accel },
	{ "repeat_delay", input_cmd_repeat_delay },
	{ "repeat_rate", input_cmd_repeat_rate },
	{ "scroll_method", input_cmd_scroll_method },
	{ "tap", input_cmd_tap },
	{ "xkb_layout", input_cmd_xkb_layout },
	{ "xkb_model", input_cmd_xkb_model },
	{ "xkb_options", input_cmd_xkb_options },
	{ "xkb_rules", input_cmd_xkb_rules },
	{ "xkb_variant", input_cmd_xkb_variant },
};

// must be in order for the bsearch
static struct cmd_handler seat_handlers[] = {
	{ "attach", seat_cmd_attach },
	{ "cursor", seat_cmd_cursor },
	{ "fallback", seat_cmd_fallback },
};

static struct cmd_handler *find_handler(char *line, enum cmd_status block) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = NULL;
	wlr_log(L_DEBUG, "find_handler(%s) %d", line, block == CMD_BLOCK_SEAT);

	bool config_loading = config->reading || !config->active;

	if (block == CMD_BLOCK_BAR) {
		return bsearch(&d, bar_handlers,
				sizeof(bar_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_BAR_COLORS) {
		return bsearch(&d, bar_colors_handlers,
				sizeof(bar_colors_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_INPUT) {
		return bsearch(&d, input_handlers,
				sizeof(input_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);
	} else if (block == CMD_BLOCK_SEAT) {
		return bsearch(&d, seat_handlers,
				sizeof(seat_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);
	}

	if (!config_loading) {
		res = bsearch(&d, command_handlers,
				sizeof(command_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);

		if (res) {
			return res;
		}
	}

	if (config->reading) {
		res = bsearch(&d, config_handlers,
				sizeof(config_handlers) / sizeof(struct cmd_handler),
				sizeof(struct cmd_handler), handler_compare);

		if (res) {
			return res;
		}
	}

	res = bsearch(&d, handlers,
			sizeof(handlers) / sizeof(struct cmd_handler),
			sizeof(struct cmd_handler), handler_compare);

	return res;
}

struct cmd_results *execute_command(char *_exec, struct sway_seat *seat) {
	// Even though this function will process multiple commands we will only
	// return the last error, if any (for now). (Since we have access to an
	// error string we could e.g. concatenate all errors there.)
	struct cmd_results *results = NULL;
	char *exec = strdup(_exec);
	char *head = exec;
	char *cmdlist;
	char *cmd;
	list_t *containers = NULL;

	if (seat == NULL) {
		// passing a NULL seat means we just pick the default seat
		seat = input_manager_get_default_seat(input_manager);
		if (!sway_assert(seat, "could not find a seat to run the command on")) {
			return NULL;
		}
	}

	config->handler_context.seat = seat;

	head = exec;
	do {
		// Extract criteria (valid for this command list only).
		bool has_criteria = false;
		if (*head == '[') {
			has_criteria = true;
			++head;
			char *criteria_string = argsep(&head, "]");
			if (head) {
				++head;
				list_t *tokens = create_list();
				char *error;

				if ((error = extract_crit_tokens(tokens, criteria_string))) {
					wlr_log(L_DEBUG, "criteria string parse error: %s", error);
					results = cmd_results_new(CMD_INVALID, criteria_string,
						"Can't parse criteria string: %s", error);
					free(error);
					free(tokens);
					goto cleanup;
				}
				containers = container_for_crit_tokens(tokens);

				free(tokens);
			} else {
				if (!results) {
					results = cmd_results_new(CMD_INVALID, criteria_string, "Unmatched [");
				}
				goto cleanup;
			}
			// Skip leading whitespace
			head += strspn(head, whitespace);
		}
		// Split command list
		cmdlist = argsep(&head, ";");
		cmdlist += strspn(cmdlist, whitespace);
		do {
			// Split commands
			cmd = argsep(&cmdlist, ",");
			cmd += strspn(cmd, whitespace);
			if (strcmp(cmd, "") == 0) {
				wlr_log(L_INFO, "Ignoring empty command.");
				continue;
			}
			wlr_log(L_INFO, "Handling command '%s'", cmd);
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

			if (!has_criteria) {
				// without criteria, the command acts upon the focused
				// container
				config->handler_context.current_container =
					seat_get_focus_inactive(seat, &root_container);
				if (!sway_assert(config->handler_context.current_container,
						"could not get focus-inactive for root container")) {
					return NULL;
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
				free_cmd_results(res);
			} else {
				for (int i = 0; i < containers->length; ++i) {
					config->handler_context.current_container = containers->items[i];
					struct cmd_results *res = handler->handle(argc-1, argv+1);
					if (res->status != CMD_SUCCESS) {
						free_argv(argc, argv);
						if (results) {
							free_cmd_results(results);
						}
						results = res;
						goto cleanup;
					}
					free_cmd_results(res);
				}
			}
			free_argv(argc, argv);
		} while(cmdlist);
	} while(head);
cleanup:
	free(exec);
	if (!results) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return results;
}

// this is like execute_command above, except:
// 1) it ignores empty commands (empty lines)
// 2) it does variable substitution
// 3) it doesn't split commands (because the multiple commands are supposed to
//	  be chained together)
// 4) execute_command handles all state internally while config_command has
// some state handled outside (notably the block mode, in read_config)
struct cmd_results *config_command(char *exec, enum cmd_status block) {
	struct cmd_results *results = NULL;
	int argc;
	char **argv = split_args(exec, &argc);
	if (!argc) {
		results = cmd_results_new(CMD_SUCCESS, NULL, NULL);
		goto cleanup;
	}

	wlr_log(L_INFO, "handling config command '%s'", exec);
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
	// TODO commands
	for (i = handler->handle == cmd_set ? 2 : 1; i < argc; ++i) {
		argv[i] = do_var_replacement(argv[i]);
		unescape_string(argv[i]);
	}
	// Strip quotes for first argument.
	// TODO This part needs to be handled much better
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
		sway_assert(policy, "Unable to allocate security policy");
		if (policy) {
			list_add(config->command_policies, policy);
		}
	}
	policy->context = context;

	wlr_log(L_INFO, "Set command policy for %s to %d",
			policy->command, policy->context);

	results = cmd_results_new(CMD_SUCCESS, NULL, NULL);

cleanup:
	free_argv(argc, argv);
	return results;
}

struct cmd_results *cmd_results_new(enum cmd_status status,
		const char *input, const char *format, ...) {
	struct cmd_results *results = malloc(sizeof(struct cmd_results));
	if (!results) {
		wlr_log(L_ERROR, "Unable to allocate command results");
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
	json_object_object_add(root, "success",
			json_object_new_boolean(results->status == CMD_SUCCESS));
	if (results->input) {
		json_object_object_add(
				root, "input", json_object_new_string(results->input));
	}
	if (results->error) {
		json_object_object_add(
				root, "error", json_object_new_string(results->error));
	}
	json_object_array_add(result_array, root);
	const char *json = json_object_to_json_string(result_array);
	free(result_array);
	free(root);
	return json;
}

/**
 * Check and add color to buffer.
 *
 * return error object, or NULL if color is valid.
 */
struct cmd_results *add_color(const char *name,
		char *buffer, const char *color) {
	int len = strlen(color);
	if (len != 7 && len != 9) {
		return cmd_results_new(CMD_INVALID, name,
				"Invalid color definition %s", color);
	}
	if (color[0] != '#') {
		return cmd_results_new(CMD_INVALID, name,
				"Invalid color definition %s", color);
	}
	for (int i = 1; i < len; ++i) {
		if (!isxdigit(color[i])) {
			return cmd_results_new(CMD_INVALID, name,
					"Invalid color definition %s", color);
		}
	}
	strcpy(buffer, color);
	// add default alpha channel if color was defined without it
	if (len == 7) {
		buffer[7] = 'f';
		buffer[8] = 'f';
	}
	buffer[9] = '\0';
	return NULL;
}
