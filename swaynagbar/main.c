#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <signal.h>
#include "log.h"
#include "list.h"
#include "readline.h"
#include "swaynagbar/nagbar.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static struct sway_nagbar nagbar;

void sig_handler(int signal) {
	nagbar_destroy(&nagbar);
	exit(EXIT_FAILURE);
}

void sway_terminate(int code) {
	nagbar_destroy(&nagbar);
	exit(code);
}

static void set_nagbar_colors() {
	if (nagbar.type == NAGBAR_ERROR) {
		nagbar.colors.button_background = 0x680A0AFF;
		nagbar.colors.background = 0x900000FF;
		nagbar.colors.text = 0xFFFFFFFF;
		nagbar.colors.border = 0xD92424FF;
		nagbar.colors.border_bottom = 0x470909FF;
	} else if (nagbar.type == NAGBAR_WARNING) {
		nagbar.colors.button_background = 0xFFC100FF;
		nagbar.colors.background = 0xFFA800FF;
		nagbar.colors.text = 0x000000FF;
		nagbar.colors.border = 0xAB7100FF;
		nagbar.colors.border_bottom = 0xAB7100FF;
	}
}

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

int main(int argc, char **argv) {
	int exit_code = EXIT_SUCCESS;
	bool debug = false;

	memset(&nagbar, 0, sizeof(nagbar));
	nagbar.anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	nagbar.type = NAGBAR_ERROR;
	set_nagbar_colors();
	nagbar.font = strdup("pango:monospace 8");
	nagbar.buttons = create_list();

	struct sway_nagbar_button *button_close =
		calloc(sizeof(struct sway_nagbar_button), 1);
	button_close->text = strdup("X");
	button_close->type = NAGBAR_ACTION_DISMISS;
	list_add(nagbar.buttons, button_close);

	struct sway_nagbar_button *button_details =
		calloc(sizeof(struct sway_nagbar_button), 1);
	button_details->text = strdup("Toggle Details");
	button_details->type = NAGBAR_ACTION_EXPAND;

	static struct option opts[] = {
		{"button", required_argument, NULL, 'b'},
		{"debug", no_argument, NULL, 'd'},
		{"edge", required_argument, NULL, 'e'},
		{"font", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"detailed-message", required_argument, NULL, 'l'},
		{"detailed-button", required_argument, NULL, 'L'},
		{"message", required_argument, NULL, 'm'},
		{"output", required_argument, NULL, 'o'},
		{"dismiss-button", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaynagbar [options...]\n"
		"\n"
		"  -b, --button <text> <action>  Create a button with text that "
			"executes action when pressed. Multiple buttons can be defined.\n"
		"  -d, --debug                   Enable debugging.\n"
		"  -e, --edge top|bottom         Set the edge to use.\n"
		"  -f, --font <font>             Set the font to use.\n"
		"  -h, --help                    Show help message and quit.\n"
		"  -l, --detailed-message <msg>  Set a detailed message.\n"
		"  -L, --detailed-button <text>  Set the text of the detail button.\n"
		"  -m, --message <msg>           Set the message text.\n"
		"  -o, --output <output>         Set the output to use.\n"
		"  -s, --dismiss-button <text>   Set the dismiss button text.\n"
		"  -t, --type error|warning      Set the message type.\n"
		"  -v, --version                 Show the version number and quit.\n";

	while (1) {
		int c = getopt_long(argc, argv, "b:de:f:hl:L:m:o:s:t:v", opts, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'b': // Button
			if (optind >= argc) {
				fprintf(stderr, "Missing action for button %s\n", optarg);
				exit_code = EXIT_FAILURE;
				goto cleanup;
			}
			struct sway_nagbar_button *button;
			button = calloc(sizeof(struct sway_nagbar_button), 1);
			button->text = strdup(optarg);
			button->type = NAGBAR_ACTION_COMMAND;
			button->action = strdup(argv[optind]);
			optind++;
			list_add(nagbar.buttons, button);
			break;
		case 'd': // Debug
			debug = true;
			break;
		case 'e': // Edge
			if (strcmp(optarg, "top") == 0) {
				nagbar.anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			} else if (strcmp(optarg, "bottom") == 0) {
				nagbar.anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
					| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			} else {
				fprintf(stderr, "Invalid edge: %s\n", optarg);
				exit_code = EXIT_FAILURE;
				goto cleanup;
			}
			break;
		case 'f': // Font
			free(nagbar.font);
			nagbar.font = strdup(optarg);
			break;
		case 'l': // Detailed Message
			free(nagbar.details.message);
			if (strcmp(optarg, "-") == 0) {
				nagbar.details.message = read_from_stdin();
			} else {
				nagbar.details.message = strdup(optarg);
			}
			nagbar.details.button_up.text = strdup("▲");
			nagbar.details.button_down.text = strdup("▼");
			break;
		case 'L': // Detailed Button Text
			free(button_details->text);
			button_details->text = strdup(optarg);
			break;
		case 'm': // Message
			free(nagbar.message);
			nagbar.message = strdup(optarg);
			break;
		case 'o': // Output
			free(nagbar.output.name);
			nagbar.output.name = strdup(optarg);
			break;
		case 's': // Dismiss Button Text
			free(button_close->text);
			button_close->text = strdup(optarg);
			break;
		case 't': // Type
			if (strcmp(optarg, "error") == 0) {
				nagbar.type = NAGBAR_ERROR;
			} else if (strcmp(optarg, "warning") == 0) {
				nagbar.type = NAGBAR_WARNING;
			} else {
				fprintf(stderr, "Type must be either 'error' or 'warning'\n");
				exit_code = EXIT_FAILURE;
				goto cleanup;
			}
			set_nagbar_colors();
			break;
		case 'v': // Version
			fprintf(stdout, "sway version " SWAY_VERSION "\n");
			exit_code = EXIT_SUCCESS;
			goto cleanup;
		default: // Help or unknown flag
			fprintf(c == 'h' ? stdout : stderr, "%s", usage);
			exit_code = c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
			goto cleanup;
		}
	}

	wlr_log_init(debug ? WLR_DEBUG : WLR_ERROR, NULL);

	if (!nagbar.message) {
		wlr_log(WLR_ERROR, "No message passed. Please provide --message/-m");
		exit_code = EXIT_FAILURE;
		goto cleanup;
	}

	if (nagbar.details.message) {
		list_add(nagbar.buttons, button_details);
	} else {
		free(button_details->text);
		free(button_details);
	}

	wlr_log(WLR_DEBUG, "Output: %s", nagbar.output.name);
	wlr_log(WLR_DEBUG, "Anchors: %d", nagbar.anchors);
	wlr_log(WLR_DEBUG, "Type: %d", nagbar.type);
	wlr_log(WLR_DEBUG, "Message: %s", nagbar.message);
	wlr_log(WLR_DEBUG, "Font: %s", nagbar.font);
	wlr_log(WLR_DEBUG, "Buttons");
	for (int i = 0; i < nagbar.buttons->length; i++) {
		struct sway_nagbar_button *button = nagbar.buttons->items[i];
		wlr_log(WLR_DEBUG, "\t[%s] `%s`", button->text, button->action);
	}

	signal(SIGTERM, sig_handler);

	nagbar_setup(&nagbar);
	nagbar_run(&nagbar);
	return exit_code;

cleanup:
	free(button_details->text);
	free(button_details);
	nagbar_destroy(&nagbar);
	return exit_code;
}

