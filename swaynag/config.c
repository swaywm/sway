#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200112L
#include <getopt.h>
#include <stdlib.h>
#include <wordexp.h>
#include "log.h"
#include "list.h"
#include "readline.h"
#include "swaynag/swaynag.h"
#include "swaynag/types.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static char *read_from_stdin() {
	char *buffer = NULL;
	while (!feof(stdin)) {
		char *line = read_line(stdin);
		if (!line) {
			continue;
		}

		if (!buffer) {
			buffer = strdup(line);
		} else {
			buffer = realloc(buffer, strlen(buffer) + strlen(line) + 2);
			strcat(buffer, line);
			strcat(buffer, "\n");
		}

		free(line);
	}

	if (buffer && buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = '\0';
	}

	return buffer;
}

int swaynag_parse_options(int argc, char **argv, struct swaynag *swaynag,
		list_t *types, char **config, bool *debug) {
	static struct option opts[] = {
		{"button", required_argument, NULL, 'b'},
		{"config", required_argument, NULL, 'c'},
		{"debug", no_argument, NULL, 'd'},
		{"edge", required_argument, NULL, 'e'},
		{"font", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"detailed-message", no_argument, NULL, 'l'},
		{"detailed-button", required_argument, NULL, 'L'},
		{"message", required_argument, NULL, 'm'},
		{"output", required_argument, NULL, 'o'},
		{"dismiss-button", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaynag [options...]\n"
		"\n"
		"  -b, --button <text> <action>  Create a button with text that "
			"executes action when pressed. Multiple buttons can be defined.\n"
		"  -c, --config <path>           Path to config file.\n"
		"  -d, --debug                   Enable debugging.\n"
		"  -e, --edge top|bottom         Set the edge to use.\n"
		"  -f, --font <font>             Set the font to use.\n"
		"  -h, --help                    Show help message and quit.\n"
		"  -l, --detailed-message        Read a detailed message from stdin.\n"
		"  -L, --detailed-button <text>  Set the text of the detail button.\n"
		"  -m, --message <msg>           Set the message text.\n"
		"  -o, --output <output>         Set the output to use.\n"
		"  -s, --dismiss-button <text>   Set the dismiss button text.\n"
		"  -t, --type <type>             Set the message type.\n"
		"  -v, --version                 Show the version number and quit.\n";

	optind = 1;
	while (1) {
		int c = getopt_long(argc, argv, "b:c:de:f:hlL:m:o:s:t:v", opts, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'b': // Button
			if (swaynag) {
				if (optind >= argc) {
					fprintf(stderr, "Missing action for button %s\n", optarg);
					return EXIT_FAILURE;
				}
				struct swaynag_button *button;
				button = calloc(sizeof(struct swaynag_button), 1);
				button->text = strdup(optarg);
				button->type = SWAYNAG_ACTION_COMMAND;
				button->action = strdup(argv[optind]);
				list_add(swaynag->buttons, button);
			}
			optind++;
			break;
		case 'c': // Config
			if (config) {
				*config = strdup(optarg);
			}
			break;
		case 'd': // Debug
			if (debug) {
				*debug = true;
			}
			break;
		case 'e': // Edge
			if (swaynag) {
				if (strcmp(optarg, "top") == 0) {
					swaynag->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
				} else if (strcmp(optarg, "bottom") == 0) {
					swaynag->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
				} else {
					fprintf(stderr, "Invalid edge: %s\n", optarg);
					return EXIT_FAILURE;
				}
			}
			break;
		case 'f': // Font
			if (swaynag) {
				free(swaynag->font);
				swaynag->font = strdup(optarg);
			}
			break;
		case 'l': // Detailed Message
			if (swaynag) {
				free(swaynag->details.message);
				swaynag->details.message = read_from_stdin();
				swaynag->details.button_up.text = strdup("▲");
				swaynag->details.button_down.text = strdup("▼");
			}
			break;
		case 'L': // Detailed Button Text
			if (swaynag) {
				free(swaynag->details.button_details.text);
				swaynag->details.button_details.text = strdup(optarg);
			}
			break;
		case 'm': // Message
			if (swaynag) {
				free(swaynag->message);
				swaynag->message = strdup(optarg);
			}
			break;
		case 'o': // Output
			if (swaynag) {
				free(swaynag->output.name);
				swaynag->output.name = strdup(optarg);
			}
			break;
		case 's': // Dismiss Button Text
			if (swaynag) {
				struct swaynag_button *button_close;
				button_close = swaynag->buttons->items[0];
				free(button_close->text);
				button_close->text = strdup(optarg);
			}
			break;
		case 't': // Type
			if (swaynag) {
				swaynag->type = swaynag_type_get(types, optarg);
				if (!swaynag->type) {
					fprintf(stderr, "Unknown type %s\n", optarg);
					return EXIT_FAILURE;
				}
			}
			break;
		case 'v': // Version
			fprintf(stdout, "swaynag version " SWAY_VERSION "\n");
			return -1;
		default: // Help or unknown flag
			fprintf(c == 'h' ? stdout : stderr, "%s", usage);
			return -1;
		}
	}

	return 0;
}

static bool file_exists(const char *path) {
	return path && access(path, R_OK) != -1;
}

char *swaynag_get_config_path(void) {
	static const char *config_paths[] = {
		"$HOME/.swaynag/config",
		"$XDG_CONFIG_HOME/swaynag/config",
		SYSCONFDIR "/swaynag/config",
	};

	if (!getenv("XDG_CONFIG_HOME")) {
		char *home = getenv("HOME");
		char *config_home = malloc(strlen(home) + strlen("/.config") + 1);
		if (!config_home) {
			wlr_log(WLR_ERROR, "Unable to allocate $HOME/.config");
		} else {
			strcpy(config_home, home);
			strcat(config_home, "/.config");
			setenv("XDG_CONFIG_HOME", config_home, 1);
			wlr_log(WLR_DEBUG, "Set XDG_CONFIG_HOME to %s", config_home);
			free(config_home);
		}
	}

	wordexp_t p;
	char *path;
	for (size_t i = 0; i < sizeof(config_paths) / sizeof(char *); ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			path = strdup(p.we_wordv[0]);
			wordfree(&p);
			if (file_exists(path)) {
				return path;
			}
			free(path);
		}
	}

	return NULL;
}

int swaynag_load_config(char *path, struct swaynag *swaynag, list_t *types) {
	FILE *config = fopen(path, "r");
	if (!config) {
		fprintf(stderr, "Failed to read config. Running without it.\n");
		return 0;
	}
	struct swaynag_type *type = NULL;
	char *line;
	int line_number = 0;
	while (!feof(config)) {
		line = read_line(config);
		if (!line) {
			continue;
		}

		line_number++;
		if (line[0] == '#') {
			free(line);
			continue;
		}
		if (strlen(line) == 0) {
			free(line);
			continue;
		}

		if (line[0] == '[') {
			char *close = strchr(line, ']');
			if (!close) {
				free(line);
				fclose(config);
				fprintf(stderr, "Closing bracket not found on line %d\n",
						line_number);
				return 1;
			}
			char *name = calloc(1, close - line);
			strncat(name, line + 1, close - line - 1);
			type = swaynag_type_get(types, name);
			if (!type) {
				type = calloc(1, sizeof(struct swaynag_type));
				type->name = strdup(name);
				list_add(types, type);
			}
			free(name);
		} else {
			char flag[strlen(line) + 3];
			sprintf(flag, "--%s", line);
			char *argv[] = {"swaynag", flag};
			int result;
			if (type) {
				result = swaynag_parse_type(2, argv, type);
			} else {
				result = swaynag_parse_options(2, argv, swaynag, types,
						NULL, NULL);
			}
			if (result != 0) {
				free(line);
				fclose(config);
				return result;
			}
		}

		free(line);
	}
	fclose(config);
	return 0;
}

