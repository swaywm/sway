#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "sway/commands.h"
#include "list.h"
#include "stringop.h"

struct cmd_results *cmd_mark(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "mark", "Can't be used in config file.");
	if ((error = checkarg(argc, "floating", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	swayc_t *view = get_focused_container(&root_container);
	bool add = false;
	bool toggle = false;

	if (strcmp(argv[0], "--add") == 0) {
		--argc; ++argv;
		add = true;
	} else if (strcmp(argv[0], "--replace") == 0) {
		--argc; ++argv;
	}

	if (argc && strcmp(argv[0], "--toggle") == 0) {
		--argc; ++argv;
		toggle = true;
	}

	if (argc) {
		char *mark = join_args(argv, argc);
		if (view->marks) {
			if (add) {
				int index;
				if ((index = list_seq_find(view->marks, (int (*)(const void *, const void *))strcmp, mark)) != -1) {
					if (toggle) {
						free(view->marks->items[index]);
						list_del(view->marks, index);

						if (0 == view->marks->length) {
							list_free(view->marks);
							view->marks = NULL;
						}
					}
					free(mark);
				} else {
					list_add(view->marks, mark);
				}
			} else {
				if (toggle && list_seq_find(view->marks, (int (*)(const void *, const void *))strcmp, mark) != -1) {
					// Delete the list
					list_foreach(view->marks, free);
					list_free(view->marks);
					view->marks = NULL;
				} else {
					// Delete and replace with a new list
					list_foreach(view->marks, free);
					list_free(view->marks);

					view->marks = create_list();
					list_add(view->marks, mark);
				}
			}
		} else {
			view->marks = create_list();
			list_add(view->marks, mark);
		}
	} else {
		return cmd_results_new(CMD_FAILURE, "mark",
			"Expected 'mark [--add|--replace] [--toggle] <mark>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
