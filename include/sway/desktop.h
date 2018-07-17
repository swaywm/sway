#include <wlr/types/wlr_surface.h>

struct sway_container;

void desktop_damage_surface(struct wlr_surface *surface, double lx, double ly,
	bool whole);

void desktop_damage_whole_container(struct sway_container *con);
