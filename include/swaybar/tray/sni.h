#ifndef _SWAYBAR_SNI_H
#define _SWAYBAR_SNI_H

#include <stdbool.h>
#include <client/cairo.h>

struct StatusNotifierItem {
	/* Name registered to sni watcher */
	char *name;
	/* Unique bus name, needed for determining signal origins */
	char *unique_name;
	bool kde_special_snowflake;

	cairo_surface_t *image;
	bool dirty;
};

/* Each output holds an sni_icon_ref of each item to render */
struct sni_icon_ref {
	cairo_surface_t *icon;
	struct StatusNotifierItem *ref;
};

struct sni_icon_ref *sni_icon_ref_create(struct StatusNotifierItem *item,
		int height);

void sni_icon_ref_free(struct sni_icon_ref *sni_ref);

/**
 * Will return a new item and get its icon. (see warning below)
 * May return `NULL` if `name` is not valid.
 */
struct StatusNotifierItem *sni_create(const char *name);

/**
 * `item` must be a struct StatusNotifierItem *
 * `str` must be a NUL terminated char *
 *
 * Returns 0 if `item` has a name of `str`
 */
int sni_str_cmp(const void *item, const void *str);

/**
 * Returns 0 if `item` has a unique name of `str` or if
 * `item->unique_name == NULL`
 */
int sni_uniq_cmp(const void *item, const void *str);

/**
 * Gets an icon for the given item if found.
 *
 * XXX
 * This function keeps a reference to the item until it gets responses, make
 * sure that the reference and item are valid during this time.
 */
void get_icon(struct StatusNotifierItem *item);

/**
 * Calls the "activate" method on the given StatusNotifierItem
 *
 * x and y should be where the item was clicked
 */
void sni_activate(struct StatusNotifierItem *item, uint32_t x, uint32_t y);

/**
 * Asks the item to draw a context menu at the given x and y coords
 */
void sni_context_menu(struct StatusNotifierItem *item, uint32_t x, uint32_t y);

/**
 * Calls the "secondary activate" method on the given StatusNotifierItem
 *
 * x and y should be where the item was clicked
 */
void sni_secondary(struct StatusNotifierItem *item, uint32_t x, uint32_t y);

/**
 * Deconstructs `item`
 */
void sni_free(struct StatusNotifierItem *item);

#endif /* _SWAYBAR_SNI_H */
