#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "list.h"
#include "stringop.h"

struct cmd_results *cmd_unmark(int argc, char **argv) {
	swayc_t *view = current_container;

	if (view->marks) {
		if (argc) {
			char *mark = join_args(argv, argc);
			char *item;
			ssize_t index;
			if ((index = list_lsearch(view->marks, strcmp_ptr, &mark, &item)) != -1) {
				free(item);
				list_delete(view->marks, index);

				if (view->marks->length == 0) {
					list_free(view->marks);
					view->marks = NULL;
				}
			}
			free(mark);
		} else {
			list_free_with(view->marks, free);
			view->marks = NULL;
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
