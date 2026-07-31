#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <json_object.h>
#include <json_util.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_xdg_session_management_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "log.h"
#include "stringop.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/xdg_session_management_v1.h"

struct sway_xdg_session_v1 {
	struct wlr_xdg_session_v1 *wlr; // may be NULL
	char *path;
	json_object *restorable_toplevels;
	struct wl_list toplevels; // sway_xdg_shell_view.xdg_session_v1_link

	bool save_pending;
	struct wl_event_source *save_timer;

	struct wl_listener destroy;
	struct wl_listener remove;
	struct wl_listener add_toplevel;
	struct wl_listener restore_toplevel;
	struct wl_listener remove_toplevel;
};

static char *get_directory(void) {
	const char *home = getenv("HOME");
	const char *xdg_state_home = getenv("XDG_STATE_HOME");
	char *xdg_state_home_default = NULL;
	if (xdg_state_home == NULL && home != NULL) {
		xdg_state_home_default = format_str("%s/.local/state", home);
		xdg_state_home = xdg_state_home_default;
	}
	if (xdg_state_home == NULL) {
		return NULL;
	}

	char *path = format_str("%s/sway", xdg_state_home);
	free(xdg_state_home_default);
	return path;
}

static char *get_session_path(struct wl_client *client, const char *session_id) {
	char *prefix = NULL;
	const struct wlr_security_context_v1_state *security_context =
		wlr_security_context_manager_v1_lookup_client(server.security_context_manager_v1, client);
	if (security_context != NULL &&
			strchr(security_context->sandbox_engine, '/') == NULL &&
			strchr(security_context->app_id, '/') == NULL) {
		prefix = format_str("%s_%s_", security_context->sandbox_engine, security_context->app_id);
	}

	char *path = format_str("%s/%s%s.json", server.xdg_session_manager_v1.dir,
		prefix ? prefix : "", session_id);
	free(prefix);
	return path;
}

static FILE *open_urandom(void) {
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		sway_log_errno(SWAY_ERROR, "Failed to open /dev/urandom");
		return NULL;
	}
	FILE *f = fdopen(fd, "r");
	if (f == NULL) {
		sway_log_errno(SWAY_ERROR, "fdopen() failed");
		close(fd);
		return NULL;
	}
	return f;
}

#define TOKEN_SIZE 33

static bool generate_token(char out[static TOKEN_SIZE]) {
	FILE *urandom = server.xdg_session_manager_v1.urandom;
	uint64_t data[2];

	if (fread(data, sizeof(data), 1, urandom) != 1) {
		sway_log_errno(SWAY_ERROR, "Failed to read from random device");
		return false;
	}
	if (snprintf(out, TOKEN_SIZE, "%016" PRIx64 "%016" PRIx64, data[0], data[1]) != TOKEN_SIZE - 1) {
		sway_log_errno(SWAY_ERROR, "Failed to format hex string token");
		return false;
	}

	return true;
}

static json_object *session_to_json(struct sway_xdg_session_v1 *session) {
	json_object *toplevels_obj = json_object_new_object();
	struct sway_xdg_shell_view *view;
	wl_list_for_each(view, &session->toplevels, xdg_session_v1.link) {
		struct sway_container *container = view->view.container;
		json_object *toplevel_obj = json_object_new_object();
		json_object_object_add(toplevel_obj, "floating",
			json_object_new_boolean(container_is_floating(container)));
		json_object_object_add(toplevel_obj, "floating",
			json_object_new_string(container->pending.workspace->name));
		// TODO: more
		json_object_object_add(toplevels_obj, view->xdg_session_v1.name, toplevel_obj);
	}

	json_object *session_obj = json_object_new_object();
	json_object_object_add(session_obj, "toplevels", toplevels_obj);
	return session_obj;
}

static void session_save(struct sway_xdg_session_v1 *session) {
	json_object *session_obj = session_to_json(session);
	int ret = json_object_to_file(session->path, session_obj);
	json_object_put(session_obj);
	if (ret < 0) {
		sway_log(SWAY_ERROR, "Failed to save XDG session to '%s'", session->path);
	}
}

int session_handle_save_timer(void *data) {
	struct sway_xdg_session_v1 *session = data;
	session->save_pending = false;
	session_save(session);
	return 0;
}

static void session_schedule_save(struct sway_xdg_session_v1 *session) {
	if (!session->save_pending) {
		wl_event_source_timer_update(session->save_timer, 30 * 1000);
	}
}

static json_object *load_session(const char *path) {
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (errno != ENOENT) {
			sway_log_errno(SWAY_ERROR, "Failed to read XDG session from '%s'", path);
		}
		return NULL;
	}

	json_object *session_obj = json_object_from_fd(fd);
	close(fd);
	if (session_obj == NULL) {
		sway_log(SWAY_ERROR, "Failed to load XDG session from '%s'", path);
	}
	return session_obj;
}

static void session_consider_destroy(struct sway_xdg_session_v1 *session) {
	// TODO: call this function on toplevel destroy
	if (session->wlr || !wl_list_empty(&session->toplevels)) {
		return;
	}
	if (session->save_pending) {
		session_save(session);
	}
	wl_event_source_remove(session->save_timer);
	json_object_put(session->restorable_toplevels);
	free(session->path);
	free(session);
}

static void session_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_session_v1 *session = wl_container_of(listener, session, destroy);
	wl_list_remove(&session->destroy.link);
	wl_list_remove(&session->remove.link);
	wl_list_remove(&session->add_toplevel.link);
	wl_list_remove(&session->restore_toplevel.link);
	wl_list_remove(&session->remove_toplevel.link);
	session->wlr = NULL;
	session_consider_destroy(session);
}

static void session_handle_remove(struct wl_listener *listener, void *data) {
	struct sway_xdg_session_v1 *session = wl_container_of(listener, session, remove);

	if (unlink(session->path) != 0 && errno != ENOENT) {
		sway_log_errno(SWAY_ERROR, "Failed to delete XDG session '%s'", session->path);
	}
}

static void session_add_toplevel(struct sway_xdg_session_v1 *session,
		struct wlr_xdg_toplevel_session_v1 *toplevel_session, bool restore) {
	struct sway_xdg_shell_view *view = toplevel_session->toplevel->base->data;

	char *name = strdup(toplevel_session->name);
	if (name == NULL) {
		wl_resource_post_no_memory(toplevel_session->resource);
		return;
	}

	// TODO: send error on duplicate session or name

	view->xdg_session_v1.session = session;
	view->xdg_session_v1.name = name;
	wl_list_remove(&view->xdg_session_v1.link);
	wl_list_insert(&session->toplevels, &view->xdg_session_v1.link);

	// TODO: listen to rename event

	if (!restore) {
		session_schedule_save(session);
	}
}

static void session_handle_add_toplevel(struct wl_listener *listener, void *data) {
	struct sway_xdg_session_v1 *session = wl_container_of(listener, session, add_toplevel);
	struct wlr_xdg_toplevel_session_v1 *toplevel_session = data;
	session_add_toplevel(session, toplevel_session, false);
}

static void session_handle_restore_toplevel(struct wl_listener *listener, void *data) {
	struct sway_xdg_session_v1 *session = wl_container_of(listener, session, restore_toplevel);
	struct wlr_xdg_toplevel_session_v1 *toplevel_session = data;
	session_add_toplevel(session, toplevel_session, true);
}

static void session_handle_remove_toplevel(struct wl_listener *listener, void *data) {
	struct sway_xdg_session_v1 *session = wl_container_of(listener, session, remove_toplevel);
	const struct wlr_xdg_session_v1_remove_toplevel_event *event = data;

	json_object_object_del(session->restorable_toplevels, event->name);

	bool found;
	struct sway_xdg_shell_view *view;
	wl_list_for_each(view, &session->toplevels, xdg_session_v1.link) {
		if (strcmp(view->xdg_session_v1.name, event->name) == 0) {
			found = true;
			break;
		}
	}
	if (!found) {
		return;
	}

	// TODO: deduplicate
	view->xdg_session_v1.session = NULL;
	free(view->xdg_session_v1.name);
	view->xdg_session_v1.name = NULL;
	wl_list_remove(&view->xdg_session_v1.link);
	wl_list_init(&view->xdg_session_v1.link);

	session_schedule_save(session);
}

static void handle_new_session(struct wl_listener *listener, void *data) {
	struct wlr_xdg_session_v1 *wlr_session = data;
	struct wl_client *client = wl_resource_get_client(wlr_session->resource);

	char *path = NULL;
	json_object *restorable_toplevels = NULL;
	if (wlr_session->id != NULL) {
		path = get_session_path(client, wlr_session->id);
		if (path == NULL) {
			wl_resource_post_no_memory(wlr_session->resource);
			return;
		}

		json_object *session_obj = load_session(path);
		restorable_toplevels = json_object_get(json_object_object_get(session_obj, "toplevels"));
		json_object_put(session_obj);
	}

	char new_session_id[TOKEN_SIZE];
	if (restorable_toplevels == NULL) {
		free(path);
		path = NULL;

		if (!generate_token(new_session_id)) {
			wl_resource_post_no_memory(wlr_session->resource);
			return;
		}

		path = get_session_path(client, new_session_id);
		if (path == NULL) {
			wl_resource_post_no_memory(wlr_session->resource);
			return;
		}
	}

	struct sway_xdg_session_v1 *session = calloc(1, sizeof(*session));
	if (session == NULL) {
		wl_resource_post_no_memory(wlr_session->resource);
		free(path);
		return;
	}

	session->save_timer = wl_event_loop_add_timer(server.wl_event_loop,
		session_handle_save_timer, session);
	if (session->save_timer == NULL) {
		wl_resource_post_no_memory(wlr_session->resource);
		free(session);
		free(path);
		return;
	}

	session->wlr = wlr_session;
	session->path = path;
	session->restorable_toplevels = restorable_toplevels;

	session->destroy.notify = session_handle_destroy;
	wl_signal_add(&session->wlr->events.destroy, &session->destroy);

	session->remove.notify = session_handle_remove;
	wl_signal_add(&session->wlr->events.remove, &session->remove);

	session->add_toplevel.notify = session_handle_add_toplevel;
	wl_signal_add(&session->wlr->events.add_toplevel, &session->add_toplevel);

	session->restore_toplevel.notify = session_handle_restore_toplevel;
	wl_signal_add(&session->wlr->events.restore_toplevel, &session->restore_toplevel);

	session->remove_toplevel.notify = session_handle_remove_toplevel;
	wl_signal_add(&session->wlr->events.remove_toplevel, &session->remove_toplevel);

	if (restorable_toplevels != NULL) {
		wlr_xdg_session_v1_notify_restored(session->wlr);
	} else {
		wlr_xdg_session_v1_notify_created(session->wlr, new_session_id);
	}
}

bool init_xdg_session_management_v1(struct sway_server *server) {
	char *dir = get_directory();
	if (dir == NULL) {
		sway_log(SWAY_ERROR, "Failed to pick XDG session management directory");
		return false;
	}
	server->xdg_session_manager_v1.dir = dir;

	FILE *urandom = open_urandom();
	if (urandom == NULL) {
		return false;
	}
	server->xdg_session_manager_v1.urandom = urandom;

	struct wlr_xdg_session_manager_v1 *xdg_session_manager_v1 =
		wlr_xdg_session_manager_v1_create(server->wl_display, 1);
	if (xdg_session_manager_v1 == NULL) {
		return false;
	}

	server->xdg_session_manager_v1.new_session.notify = handle_new_session;
	wl_signal_add(&xdg_session_manager_v1->events.new_session,
		&server->xdg_session_manager_v1.new_session);

	// TODO: regularly clean up stale files

	return true;
}

void finish_xdg_session_management_v1(struct sway_server *server) {
	wl_list_remove(&server->xdg_session_manager_v1.new_session.link);
	free(server->xdg_session_manager_v1.dir);
	fclose(server->xdg_session_manager_v1.urandom);
}

void notify_xdg_session_management_v1_toplevel_update(struct sway_xdg_shell_view *view) {
	if (view->xdg_session_v1.session) {
		session_schedule_save(view->xdg_session_v1.session);
	}
}

void notify_xdg_session_management_v1_toplevel_initial_configure(struct sway_xdg_shell_view *view) {
	struct sway_xdg_session_v1 *session = view->xdg_session_v1.session;
	if (session == NULL) {
		return;
	}

	json_object *toplevel_obj = json_object_get(json_object_object_get(
		session->restorable_toplevels, view->xdg_session_v1.name));
	json_object_object_del(session->restorable_toplevels, view->xdg_session_v1.name);
	if (toplevel_obj == NULL) {
		return;
	}

	if (view->view.session_restore.pending) {
		return;
	}

	view->view.session_restore.pending = true;
	view->view.session_restore.floating =
		json_object_get_boolean(json_object_object_get(toplevel_obj, "floating"));
	view->view.session_restore.workspace =
		strdup(json_object_get_string(json_object_object_get(toplevel_obj, "workspace")));

	if (session->wlr != NULL) {
		struct wlr_xdg_toplevel_session_v1 *toplevel_session;
		wl_list_for_each(toplevel_session, &session->wlr->toplevels, link) {
			if (toplevel_session->toplevel == view->view.wlr_xdg_toplevel) {
				wlr_xdg_toplevel_session_v1_notify_restored(toplevel_session);
				break;
			}
		}
	}

	json_object_put(toplevel_obj);
}
