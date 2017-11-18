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
			int index;
			if ((index = list_seq_find(view->marks, (int (*)(const void *, const void *))strcmp, mark)) != -1) {
				free(view->marks->items[index]);
				list_del(view->marks, index);

				if (view->marks->length == 0) {
					list_free(view->marks);
					view->marks = NULL;
				}
			}
			free(mark);
		} else {
			list_foreach(view->marks, free);
			list_free(view->marks);
			view->marks = NULL;
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
