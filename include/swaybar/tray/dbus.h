#ifndef _SWAYBAR_DBUS_H
#define _SWAYBAR_DBUS_H

#include <stdbool.h>
#include <systemd/sd-bus.h>

void process_request(int fd, short mask, void *data);

bool dbus_init();

void finish_dbus(sd_bus_slot *slot, sd_bus *bus);

int dbus_name_has_owner(sd_bus *bus, const char *name, sd_bus_error *error);

#endif /* _SWAYBAR_DBUS_H */
