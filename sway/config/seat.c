#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "sway/config.h"
#include "log.h"

struct seat_config *new_seat_config(const char* name) {
	struct seat_config *seat = calloc(1, sizeof(struct seat_config));
	if (!seat) {
		wlr_log(WLR_DEBUG, "Unable to allocate seat config");
		return NULL;
	}

	seat->name = strdup(name);
	if (!sway_assert(seat->name, "could not allocate name for seat")) {
		free(seat);
		return NULL;
	}

	seat->fallback = -1;
	seat->attachments = create_list();
	if (!sway_assert(seat->attachments,
				"could not allocate seat attachments list")) {
		free(seat->name);
		free(seat);
		return NULL;
	}

	return seat;
}

static void merge_wildcard_on_all(struct seat_config *wildcard) {
	for (int i = 0; i < config->seat_configs->length; i++) {
		struct seat_config *sc = config->seat_configs->items[i];
		if (strcmp(wildcard->name, sc->name) != 0) {
			wlr_log(WLR_DEBUG, "Merging seat * config on %s", sc->name);
			merge_seat_config(sc, wildcard);
		}
	}
}

struct seat_config *store_seat_config(struct seat_config *sc) {
	bool wildcard = strcmp(sc->name, "*") == 0;
	if (wildcard) {
		merge_wildcard_on_all(sc);
	}

	int i = list_seq_find(config->seat_configs, seat_name_cmp, sc->name);
	if (i >= 0) {
		wlr_log(WLR_DEBUG, "Merging on top of existing seat config");
		struct seat_config *current = config->seat_configs->items[i];
		merge_seat_config(current, sc);
		free_seat_config(sc);
		sc = current;
	} else if (!wildcard) {
		wlr_log(WLR_DEBUG, "Adding non-wildcard seat config");
		i = list_seq_find(config->seat_configs, seat_name_cmp, "*");
		if (i >= 0) {
			wlr_log(WLR_DEBUG, "Merging on top of seat * config");
			struct seat_config *current = new_seat_config(sc->name);
			merge_seat_config(current, config->seat_configs->items[i]);
			merge_seat_config(current, sc);
			free_seat_config(sc);
			sc = current;
		}
		list_add(config->seat_configs, sc);
	} else {
		// New wildcard config. Just add it
		wlr_log(WLR_DEBUG, "Adding seat * config");
		list_add(config->seat_configs, sc);
	}

	wlr_log(WLR_DEBUG, "Config stored for seat %s", sc->name);

	return sc;
}

struct seat_attachment_config *seat_attachment_config_new(void) {
	struct seat_attachment_config *attachment =
		calloc(1, sizeof(struct seat_attachment_config));
	if (!attachment) {
		wlr_log(WLR_DEBUG, "cannot allocate attachment config");
		return NULL;
	}
	return attachment;
}

static void seat_attachment_config_free(
		struct seat_attachment_config *attachment) {
	free(attachment->identifier);
	free(attachment);
	return;
}

static struct seat_attachment_config *seat_attachment_config_copy(
		struct seat_attachment_config *attachment) {
	struct seat_attachment_config *copy = seat_attachment_config_new();
	if (!copy) {
		return NULL;
	}

	copy->identifier = strdup(attachment->identifier);

	return copy;
}

static void merge_seat_attachment_config(struct seat_attachment_config *dest,
		struct seat_attachment_config *source) {
	// nothing to merge yet, but there will be some day
}

void merge_seat_config(struct seat_config *dest, struct seat_config *source) {
	if (source->fallback != -1) {
		dest->fallback = source->fallback;
	}

	for (int i = 0; i < source->attachments->length; ++i) {
		struct seat_attachment_config *source_attachment =
			source->attachments->items[i];
		bool found = false;
		for (int j = 0; j < dest->attachments->length; ++j) {
			struct seat_attachment_config *dest_attachment =
				dest->attachments->items[j];
			if (strcmp(source_attachment->identifier,
						dest_attachment->identifier) == 0) {
				merge_seat_attachment_config(dest_attachment,
					source_attachment);
				found = true;
			}
		}

		if (!found) {
			struct seat_attachment_config *copy =
				seat_attachment_config_copy(source_attachment);
			if (copy) {
				list_add(dest->attachments, copy);
			}
		}
	}
}

struct seat_config *copy_seat_config(struct seat_config *seat) {
	struct seat_config *copy = new_seat_config(seat->name);
	if (copy == NULL) {
		return NULL;
	}

	merge_seat_config(copy, seat);

	return copy;
}

void free_seat_config(struct seat_config *seat) {
	if (!seat) {
		return;
	}

	free(seat->name);
	for (int i = 0; i < seat->attachments->length; ++i) {
		seat_attachment_config_free(seat->attachments->items[i]);
	}
	list_free(seat->attachments);
	free(seat);
}

int seat_name_cmp(const void *item, const void *data) {
	const struct seat_config *sc = item;
	const char *name = data;
	return strcmp(sc->name, name);
}

struct seat_attachment_config *seat_config_get_attachment(
		struct seat_config *seat_config, char *identifier) {
	for (int i = 0; i < seat_config->attachments->length; ++i) {
		struct seat_attachment_config *attachment =
			seat_config->attachments->items[i];
		if (strcmp(attachment->identifier, identifier) == 0) {
			return attachment;
		}
	}

	return NULL;
}
