#include <wlr/types/wlr_surface.h>

void desktop_damage_whole_surface(struct wlr_surface *surface, double lx,
	double ly);

void desktop_damage_from_surface(struct wlr_surface *surface, double lx,
	double ly);
