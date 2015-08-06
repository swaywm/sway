#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stringop.h"
#include "commands.h"

int cmd_set(struct sway_config *config, int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Invalid set command (expected 2 arguments, got %d)\n", argc);
		return 1;
	}
	struct sway_variable *var = malloc(sizeof(struct sway_variable));
	var->name = malloc(strlen(argv[0]) + 1);
	strcpy(var->name, argv[0]);
	var->value = malloc(strlen(argv[1]) + 1);
	strcpy(var->value, argv[1]);
	list_add(config->symbols, var);
	return 0;
}

int cmd_bindsym(struct sway_config *config, int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Invalid bindsym command (expected 2 arguments, got %d)\n", argc);
		return 1;
	}
	// TODO: parse out keybindings
	return 0;
}

/* Keep alphabetized */
struct cmd_handler handlers[] = {
	{ "bindsym", cmd_bindsym }
	{ "set", cmd_set }
};

char **split_directive(char *line, int *argc) {
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

int handler_compare(const void *_a, const void *_b) {
	const struct cmd_handler *a = _a;
	const struct cmd_handler *b = _b;
	return strcasecmp(a->command, b->command);
}

struct cmd_handler *find_handler(struct cmd_handler handlers[], int l, char *line) {
	struct cmd_handler d = { .command=line };
	struct cmd_handler *res = bsearch(&d, handlers, l, sizeof(struct cmd_handler), handler_compare);
	return res;
}

int handle_command(struct sway_config *config, char *exec) {
	char *ptr, *cmd;
	if ((ptr = strchr(exec, ' ')) == NULL) {
		cmd = malloc(strlen(exec) + 1);
		strcpy(exec, cmd);
	} else {
		int index = ptr - exec;
		cmd = malloc(index + 1);
		strncpy(cmd, exec, index);
		cmd[index] = '\0';
	}
	struct cmd_handler *handler = find_handler(handlers, sizeof(handlers) / sizeof(struct cmd_handler), cmd);
	if (handler == NULL) {
		return 1;
	}
	int argc;
	char **argv = split_directive(exec + strlen(handler->command), &argc);
	int ret = handler->handle(config, argc, argv);
	int i;
	for (i = 0; i < argc; ++i) {
		free(argv[i]);
	}
	free(argv);
	return ret;
}
