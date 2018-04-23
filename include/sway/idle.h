#ifndef _SWAY_IDLE_H
#define _SWAY_IDLE_H
#include <sway/server.h>

void idle_setup_seat(struct sway_server *server, struct sway_seat *seat);
bool idle_init(struct sway_server *server);
#endif
