#include <wlc/wlc.h>
#include <unistd.h>
#include <string.h>
#include "sway/commands.h"
#include "stringop.h"

static void send_clipboard(void *data, const char *type, int fd) {
	if (strcmp(type, "text/plain") != 0
			&& strcmp(type, "text/plain;charset=utf-8") != 0) {
		close(fd);
		return;
	}

	const char *str = data;
	write(fd, str, strlen(str));
	close(fd);
}

struct cmd_results *cmd_clipboard(int argc, char **argv) {
	static char *current_data = NULL;

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "clipboard", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	static const char *types[2] = {
		"text/plain",
		"text/plain;charset=utf-8"
	};

	char *str = join_args(argv, argc);
	wlc_set_selection(str, types, 2, &send_clipboard);

	free(current_data);
	current_data = str;
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
