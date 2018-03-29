#ifndef _SWAYBAR_RENDER_H
#define _SWAYBAR_RENDER_H

struct swaybar;
struct swaybar_output;
struct swaybar_config;

void render_frame(struct swaybar *bar, struct swaybar_output *output);

#endif
