#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <wordexp.h>
#include <unistd.h>
#include "log.h"
#include "list.h"
#include "swaynag/swaynag.h"
#include "swaynag/types.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static char *read_and_trim_stdin(void) {
	char *buffer = NULL, *line = NULL;
	size_t buffer_len = 0, line_size = 0;
	while (1) {
		ssize_t nread = getline(&line, &line_size, stdin);
		if (nread == -1) {
			if (feof(stdin)) {
				break;
			} else {
				perror("getline");
				goto freeline;
			}
		}
		buffer = realloc(buffer, buffer_len + nread + 1);
		if (!buffer) {
			perror("realloc");
			goto freebuf;
		}
		memcpy(&buffer[buffer_len], line, nread + 1);
		buffer_len += nread;
	}
	free(line);

	while (buffer_len && buffer[buffer_len - 1] == '\n') {
		buffer[--buffer_len] = '\0';
	}

	return buffer;

freeline:
	free(line);
freebuf:
	free(buffer);
	return NULL;
}

int swaynag_parse_options(int argc, char **argv, struct swaynag *swaynag,
		list_t *types, struct swaynag_type *type, char **config, bool *debug) {
	enum type_options {
		TO_COLOR_BACKGROUND = 256,
		TO_COLOR_BORDER,
		TO_COLOR_BORDER_BOTTOM,
		TO_COLOR_BUTTON,
		TO_COLOR_DETAILS,
		TO_COLOR_TEXT,
		TO_COLOR_BUTTON_TEXT,
		TO_THICK_BAR_BORDER,
		TO_PADDING_MESSAGE,
		TO_THICK_DET_BORDER,
		TO_THICK_BTN_BORDER,
		TO_GAP_BTN,
		TO_GAP_BTN_DISMISS,
		TO_MARGIN_BTN_RIGHT,
		TO_PADDING_BTN,
	};

	static const struct option opts[] = {
		{"button", required_argument, NULL, 'b'},
		{"button-no-terminal", required_argument, NULL, 'B'},
		{"button-dismiss", required_argument, NULL, 'z'},
		{"button-dismiss-no-terminal", required_argument, NULL, 'Z'},
		{"config", required_argument, NULL, 'c'},
		{"debug", no_argument, NULL, 'd'},
		{"edge", required_argument, NULL, 'e'},
		{"layer", required_argument, NULL, 'y'},
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
		{"button-text", required_argument, NULL, TO_COLOR_BUTTON_TEXT},
		{"border-bottom-size", required_argument, NULL, TO_THICK_BAR_BORDER},
		{"message-padding", required_argument, NULL, TO_PADDING_MESSAGE},
		{"details-border-size", required_argument, NULL, TO_THICK_DET_BORDER},
		{"details-background", required_argument, NULL, TO_COLOR_DETAILS},
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
			"executes action in a terminal when pressed. Multiple buttons can "
			"be defined.\n"
		"  -B, --button-no-terminal <text> <action>  Like --button, but does"
			"not run the action in a terminal.\n"
		"  -z, --button-dismiss <text> <action>  Create a button with text that "
			"dismisses swaynag, and executes action in a terminal when pressed. "
			"Multiple buttons can be defined.\n"
		"  -Z, --button-dismiss-no-terminal <text> <action>  Like "
			"--button-dismiss, but does not run the action in a terminal.\n"
		"  -c, --config <path>             Path to config file.\n"
		"  -d, --debug                     Enable debugging.\n"
		"  -e, --edge top|bottom           Set the edge to use.\n"
		"  -y, --layer overlay|top|bottom|background\n"
	    "                                  Set the layer to use.\n"
		"  -f, --font <font>               Set the font to use.\n"
		"  -h, --help                      Show help message and quit.\n"
		"  -l, --detailed-message          Read a detailed message from stdin.\n"
		"  -L, --detailed-button <text>    Set the text of the detail button.\n"
		"  -m, --message <msg>             Set the message text.\n"
		"  -o, --output <output>           Set the output to use.\n"
		"  -s, --dismiss-button <text>     Set the dismiss button text.\n"
		"  -t, --type <type>               Set the message type.\n"
		"  -v, --version                   Show the version number and quit.\n"
		"\n"
		"The following appearance options can also be given:\n"
		"  --background RRGGBB[AA]         Background color.\n"
		"  --border RRGGBB[AA]             Border color.\n"
		"  --border-bottom RRGGBB[AA]      Bottom border color.\n"
		"  --button-background RRGGBB[AA]  Button background color.\n"
		"  --text RRGGBB[AA]               Text color.\n"
		"  --button-text RRGGBB[AA]        Button text color.\n"
		"  --border-bottom-size size       Thickness of the bar border.\n"
		"  --message-padding padding       Padding for the message.\n"
		"  --details-border-size size      Thickness for the details border.\n"
		"  --details-background RRGGBB[AA] Details background color.\n"
		"  --button-border-size size       Thickness for the button border.\n"
		"  --button-gap gap                Size of the gap between buttons\n"
		"  --button-dismiss-gap gap        Size of the gap for dismiss button.\n"
		"  --button-margin-right margin    Margin from dismiss button to edge.\n"
		"  --button-padding padding        Padding for the button text.\n";

	optind = 1;
	while (1) {
		int c = getopt_long(argc, argv, "b:B:z:Z:c:de:y:f:hlL:m:o:s:t:v", opts, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'b': // Button
		case 'B': // Button (No Terminal)
		case 'z': // Button (Dismiss)
		case 'Z': // Button (Dismiss, No Terminal)
			if (swaynag) {
				if (optind >= argc) {
					fprintf(stderr, "Missing action for button %s\n", optarg);
					return EXIT_FAILURE;
				}
				struct swaynag_button *button = calloc(sizeof(struct swaynag_button), 1);
				if (!button) {
					perror("calloc");
					return EXIT_FAILURE;
				}
				button->text = strdup(optarg);
				button->type = SWAYNAG_ACTION_COMMAND;
				button->action = strdup(argv[optind]);
				button->terminal = c == 'b';
				button->dismiss = c == 'z' || c == 'Z';
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
		case 'y': // Layer
			if (type) {
				if (strcmp(optarg, "background") == 0) {
					type->layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
				} else if (strcmp(optarg, "bottom") == 0) {
					type->layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
				} else if (strcmp(optarg, "top") == 0) {
					type->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
				} else if (strcmp(optarg, "overlay") == 0) {
					type->layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
				} else {
					fprintf(stderr, "Invalid layer: %s\n"
							"Usage: --layer overlay|top|bottom|background\n",
							optarg);
					return EXIT_FAILURE;
				}
			}
			break;
		case 'f': // Font
			if (type) {
				pango_font_description_free(type->font_description);
				type->font_description = pango_font_description_from_string(optarg);
			}
			break;
		case 'l': // Detailed Message
			if (swaynag) {
				free(swaynag->details.message);
				swaynag->details.message = read_and_trim_stdin();
				if (!swaynag->details.message) {
					return EXIT_FAILURE;
				}
				swaynag->details.button_up.text = strdup("▲");
				swaynag->details.button_down.text = strdup("▼");
			}
			break;
		case 'L': // Detailed Button Text
			if (swaynag) {
				free(swaynag->details.details_text);
				swaynag->details.details_text = strdup(optarg);
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
				struct swaynag_button *button_close = swaynag->buttons->items[0];
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
			printf("swaynag version " SWAY_VERSION "\n");
			return -1;
		case TO_COLOR_BACKGROUND: // Background color
			if (type && !parse_color(optarg, &type->background)) {
				fprintf(stderr, "Invalid background color: %s", optarg);
			}
			break;
		case TO_COLOR_BORDER: // Border color
			if (type && !parse_color(optarg, &type->border)) {
				fprintf(stderr, "Invalid border color: %s", optarg);
			}
			break;
		case TO_COLOR_BORDER_BOTTOM: // Bottom border color
			if (type && !parse_color(optarg, &type->border_bottom)) {
				fprintf(stderr, "Invalid border bottom color: %s", optarg);
			}
			break;
		case TO_COLOR_BUTTON:  // Button background color
			if (type && !parse_color(optarg, &type->button_background)) {
				fprintf(stderr, "Invalid button background color: %s", optarg);
			}
			break;
		case TO_COLOR_DETAILS:  // Details background color
			if (type && !parse_color(optarg, &type->details_background)) {
				fprintf(stderr, "Invalid details background color: %s", optarg);
			}
			break;
		case TO_COLOR_TEXT:  // Text color
			if (type && !parse_color(optarg, &type->text)) {
				fprintf(stderr, "Invalid text color: %s", optarg);
			}
			break;
		case TO_COLOR_BUTTON_TEXT:  // Button text color
			if (type && !parse_color(optarg, &type->button_text)) {
				fprintf(stderr, "Invalid button text color: %s", optarg);
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

	char *config_home = getenv("XDG_CONFIG_HOME");
	if (!config_home || config_home[0] == '\0') {
		config_paths[1] = "$HOME/.config/swaynag/config";
	}

	wordexp_t p;
	for (size_t i = 0; i < sizeof(config_paths) / sizeof(char *); ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			char *path = strdup(p.we_wordv[0]);
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

	struct swaynag_type *type = swaynag_type_new("<config>");
	list_add(types, type);

	char *line = NULL;
	size_t line_size = 0;
	ssize_t nread;
	int line_number = 0;
	int result = 0;
	while ((nread = getline(&line, &line_size, config)) != -1) {
		line_number++;
		if (!*line || line[0] == '\n' || line[0] == '#') {
			continue;
		}

		if (line[nread - 1] == '\n') {
			line[nread - 1] = '\0';
		}

		if (line[0] == '[') {
			char *close = strchr(line, ']');
			if (!close || close != &line[nread - 2] || nread <= 3) {
				fprintf(stderr, "Line %d is malformed\n", line_number);
				result = 1;
				break;
			}
			*close = '\0';
			type = swaynag_type_get(types, &line[1]);
			if (!type) {
				type = swaynag_type_new(&line[1]);
				list_add(types, type);
			}
		} else {
			char *flag = malloc(nread + 3);
			if (!flag) {
				perror("calloc");
				return EXIT_FAILURE;
			}
			snprintf(flag, nread + 3, "--%s", line);
			char *argv[] = {"swaynag", flag};
			result = swaynag_parse_options(2, argv, swaynag, types, type,
					NULL, NULL);
			free(flag);
			if (result != 0) {
				break;
			}
		}
	}
	free(line);
	fclose(config);
	return result;
}
