#ifndef _SWAYBAR_DESKTOP_H
#define _SWAYBAR_DESKTOP_H

char *load_desktop_entry_from_xdgdirs(const char *app_name);

char *get_icon_name_from_desktop_entry(const char *desktop_entry);

#endif
