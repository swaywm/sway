#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/commands.h"

/** Killing x from either A[x B[y]] or A[C[x] B[y]]
 * leaves A[B[y]], which should be flattened to B[y]. 
 * obsolete_if_killed(x) returns B.
 */
struct sway_container *obsolete_if_killed(struct sway_container *con) {
    if (container_get_siblings(con)->length==1) {
      if (con->pending.parent) {
        con = con->pending.parent;
      } else {
        return con;
      }
    }
    if (container_get_siblings(con)->length !=2) {
      return con;
    }

    list_t *siblings = container_get_siblings(con);
    int idX = list_find(siblings, con);
    struct sway_container *sibling = siblings->items[1 - idX];
    if (sibling->view || !sibling->pending.layout) {
      return con;
    }
  
    sway_log(SWAY_DEBUG, "Killing %p will obsolete %p", con, sibling);
    return sibling;
}

void flatten_obsolete(struct sway_container *obs) {
  enum sway_container_layout layout = obs->pending.layout;
  if (obs->pending.parent) {
    obs->pending.parent->pending.layout = layout;
  } else if (obs->pending.workspace) {
    obs->pending.workspace->layout = layout;
  }
  while (obs->pending.children->length) {
    struct sway_container *current = obs->pending.children->items[0];
    container_detach(current);
    container_add_sibling(obs, current, 0);
  }
  container_detach(obs);
  container_begin_destroy(obs);
  return;
}

static void close_container_iterator(struct sway_container *con, void *data) {
	if (con->view) {
    struct sway_container *obs = obsolete_if_killed(con);
		view_close(con->view);
    if (obs && obs != con) {
      flatten_obsolete(obs);
    }
  }
}

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *con = config->handler_context.container;
	struct sway_workspace *ws = config->handler_context.workspace;

	if (con) {
		close_container_iterator(con, NULL);
		container_for_each_child(con, close_container_iterator, NULL);
	} else {
		workspace_for_each_container(ws, close_container_iterator, NULL);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
