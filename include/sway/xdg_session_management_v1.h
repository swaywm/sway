#ifndef _SWAY_XDG_SESSION_MANAGEMENT_V1_H
#define _SWAY_XDG_SESSION_MANAGEMENT_V1_H

struct sway_xdg_session_v1;

bool init_xdg_session_management_v1(struct sway_server *server);
void finish_xdg_session_management_v1(struct sway_server *server);

void notify_xdg_session_management_v1_toplevel_initial_configure(struct sway_xdg_shell_view *view);
void notify_xdg_session_management_v1_toplevel_update(struct sway_xdg_shell_view *view);

#endif
