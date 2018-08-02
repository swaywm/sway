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
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static char *read_from_stdin() {
	char *buffer = NULL;
	while (!feof(stdin)) {
		char *line = read_line(stdin);
		if (!line) {
			continue;
		}

		size_t curlen = buffer ? strlen(buffer) : 0;
		buffer = realloc(buffer, curlen + strlen(line) + 2);
		snprintf(buffer + curlen, strlen(line) + 2, "%s\n", line);

		free(line);
	}

	while (buffer && buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer) - 1] = '\0';
	}

	return buffer;
}

int swaynag_parse_options(int argc, char **argv, struct swaynag *swaynag,
		list_t *types, struct swaynag_type *type, char **config, bool *debug) {
	enum type_options {
		TO_COLOR_BACKGROUND = 256,
		TO_COLOR_BORDER,
		TO_COLOR_BORDER_BOTTOM,
		TO_COLOR_BUTTON,
		TO_COLOR_TEXT,
		TO_THICK_BAR_BORDER,
		TO_PADDING_MESSAGE,
		TO_THICK_DET_BORDER,
		TO_THICK_BTN_BORDER,
		TO_GAP_BTN,
		TO_GAP_BTN_DISMISS,
		TO_MARGIN_BTN_RIGHT,
		TO_PADDING_BTN,
	};

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

		{"background", required_argument, NULL, TO_COLOR_BACKGROUND},
		{"border", required_argument, NULL, TO_COLOR_BORDER},
		{"border-bottom", required_argument, NULL, TO_COLOR_BORDER_BOTTOM},
		{"button-background", required_argument, NULL, TO_COLOR_BUTTON},
		{"text", required_argument, NULL, TO_COLOR_TEXT},
		{"border-bottom-size", required_argument, NULL, TO_THICK_BAR_BORDER},
		{"message-padding", required_argument, NULL, TO_PADDING_MESSAGE},
		{"details-border-size", required_argument, NULL, TO_THICK_DET_BORDER},
		{"button-border-size", required_argument, NULL, TO_THICK_BTN_BORDER},
		{"button-gap", required_argument, NULL, TO_GAP_BTN},
		{"button-dismiss-gap", required_argument, NULL, TO_GAP_BTN_DISMISS},
		{"button-margin-right", required_argument, NULL, TO_MARGIN_BTN_RIGHT},
		{"button-padding", required_argument, NULL, TO_PADDING_BTN},

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
		"  -v, --version                 Show the version number and quit.\n"
		"\n"
		"The following appearance options can also be given:\n"
		"  --background RRGGBB[AA]       Background color.\n"
		"  --border RRGGBB[AA]           Border color.\n"
		"  --border-bottom RRGGBB[AA]    Bottom border color.\n"
		"  --button-background RRGGBB[AA]           Button background color.\n"
		"  --text RRGGBB[AA]             Text color.\n"
		"  --border-bottom-size size     Thickness of the bar border.\n"
		"  --message-padding padding     Padding for the message.\n"
		"  --details-border-size size    Thickness for the details border.\n"
		"  --button-border-size size     Thickness for the button border.\n"
		"  --button-gap gap              Size of the gap between buttons\n"
		"  --button-dismiss-gap gap      Size of the gap for dismiss button.\n"
		"  --button-margin-right margin  Margin from dismiss button to edge.\n"
		"  --button-padding padding      Padding for the button text.\n";

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
			if (type) {
				if (strcmp(optarg, "top") == 0) {
					type->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
				} else if (strcmp(optarg, "bottom") == 0) {
					type->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
						| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
				} else {
					fprintf(stderr, "Invalid edge: %s\n", optarg);
					return EXIT_FAILURE;
				}
			}
			break;
		case 'f': // Font
			if (type) {
				free(type->font);
				type->font = strdup(optarg);
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
			if (type) {
				free(type->output);
				type->output = strdup(optarg);
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
		case TO_COLOR_BACKGROUND: // Background color
			if (type) {
				type->background = parse_color(optarg);
			}
			break;
		case TO_COLOR_BORDER: // Border color
			if (type) {
				type->border = parse_color(optarg);
			}
			break;
		case TO_COLOR_BORDER_BOTTOM: // Bottom border color
			if (type) {
				type->border_bottom = parse_color(optarg);
			}
			break;
		case TO_COLOR_BUTTON:  // Button background color
			if (type) {
				type->button_background = parse_color(optarg);
			}
			break;
		case TO_COLOR_TEXT:  // Text color
			if (type) {
				type->text = parse_color(optarg);
			}
			break;
		case TO_THICK_BAR_BORDER:  // Bottom border thickness
			if (type) {
				type->bar_border_thickness = strtol(optarg, NULL, 0);
			}
			break;
		case TO_PADDING_MESSAGE:  // Message padding
			if (type) {
				type->message_padding = strtol(optarg, NULL, 0);
			}
			break;
		case TO_THICK_DET_BORDER:  // Details border thickness
			if (type) {
				type->details_border_thickness = strtol(optarg, NULL, 0);
			}
			break;
		case TO_THICK_BTN_BORDER:  // Button border thickness
			if (type) {
				type->button_border_thickness = strtol(optarg, NULL, 0);
			}
			break;
		case TO_GAP_BTN: // Gap between buttons
			if (type) {
				type->button_gap = strtol(optarg, NULL, 0);
			}
			break;
		case TO_GAP_BTN_DISMISS:  // Gap between dismiss button
			if (type) {
				type->button_gap_close = strtol(optarg, NULL, 0);
			}
			break;
		case TO_MARGIN_BTN_RIGHT:  // Margin on the right side of button area
			if (type) {
				type->button_margin_right = strtol(optarg, NULL, 0);
			}
			break;
		case TO_PADDING_BTN:  // Padding for the button text
			if (type) {
				type->button_padding = strtol(optarg, NULL, 0);
			}
			break;
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

	struct swaynag_type *type;
	type = calloc(1, sizeof(struct swaynag_type));
	type->name = strdup("<config>");
	list_add(types, type);

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
			result = swaynag_parse_options(2, argv, swaynag, types, type,
					NULL, NULL);
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

