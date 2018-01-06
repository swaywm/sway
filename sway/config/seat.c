#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <string.h>
#include "sway/config.h"
#include "log.h"

struct seat_config *new_seat_config(const char* name) {
	struct seat_config *seat = calloc(1, sizeof(struct seat_config));
	if (!seat) {
		wlr_log(L_DEBUG, "Unable to allocate seat config");
		return NULL;
	}

	wlr_log(L_DEBUG, "new_seat_config(%s)", name);
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

struct seat_attachment_config *seat_attachment_config_new() {
	struct seat_attachment_config *attachment =
		calloc(1, sizeof(struct seat_attachment_config));
	if (!attachment) {
		wlr_log(L_DEBUG, "cannot allocate attachment config");
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
	if (source->name) {
		free(dest->name);
		dest->name = strdup(source->name);
	}

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

void free_seat_config(struct seat_config *seat) {
	if (!seat) {
		return;
	}

	free(seat->name);
	for (int i = 0; i < seat->attachments->length; ++i) {
		struct seat_attachment_config *attachment =
			seat->attachments->items[i];
		seat_attachment_config_free(attachment);
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
