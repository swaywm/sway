#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "sway/commands.h"
#include "list.h"
#include "stringop.h"

static void find_marks_callback(swayc_t *container, void *_mark) {
	char *mark = (char *)_mark;

	if (!container->marks) {
		return;
	}

	ssize_t index = list_lsearch(container->marks, strcmp_ptr, &mark, NULL);
	if (index != -1) {
		list_delete(container->marks, index);
	}
}

struct cmd_results *cmd_mark(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "mark", "Can't be used in config file.");
	if ((error = checkarg(argc, "mark", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	swayc_t *view = current_container;
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

		// Remove all existing marks of this type
		container_map(&root_container, find_marks_callback, mark);

		if (view->marks) {
			if (add) {
				ssize_t index;
				char *item;
				if ((index = list_lsearch(view->marks, strcmp_ptr, &mark, &item)) != -1) {
					if (toggle) {
						free(item);
						list_delete(view->marks, index);

						if (0 == view->marks->length) {
							list_free(view->marks);
							view->marks = NULL;
						}
					}
					free(mark);
				} else {
					list_add(view->marks, &mark);
				}
			} else {
				if (toggle && list_lsearch(view->marks, strcmp_ptr, &mark, NULL) != -1) {
					// Delete the list
					list_free_withp(view->marks, free);
					view->marks = NULL;
				} else {
					// Delete and replace with a new list
					list_free_withp(view->marks, free);

					view->marks = list_new(sizeof(char *), 0);
					list_add(view->marks, &mark);
				}
			}
		} else {
			view->marks = list_new(sizeof(char *), 0);
			list_add(view->marks, &mark);
		}
	} else {
		return cmd_results_new(CMD_FAILURE, "mark",
			"Expected 'mark [--add|--replace] [--toggle] <mark>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
