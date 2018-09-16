#ifndef _SWAYBAR_SNI_WATCHER_H
#define _SWAYBAR_SNI_WATCHER_H

#include <stdbool.h>
#include <systemd/sd-bus.h>

#include "list.h"

struct sni_watcher {
	sd_bus *bus;
	const char *iface;
	const char *path;
	list_t *hosts;
	list_t *items;
	int version;
};

/**
 * Starts the sni_watcher, the watcher is practically a black box and should
 * only be accessed though functions described in its spec
 */

static int method_register_sni(sd_bus_message *msg, void *userdata,
		sd_bus_error *ret_error);

static int property_registered_sni(sd_bus *bus, const char *path,
		const char *interface, const char *property, sd_bus_message *reply,
		void *userdata, sd_bus_error *error);

bool sni_watcher_vtable_init(struct sni_watcher *watcher, sd_bus_slot *slot);

bool sni_watcher_init(struct sni_watcher *watcher);

static int handle_new_icon(sd_bus_message *msg, void *userdata,
		sd_bus_error *ret_error);

int add_sni_signal_matches(struct sni_watcher *watcher);

int sni_item_cmp(const void *item, const void *data);

#endif /* _SWAYBAR_SNI_WATCHER_H */
