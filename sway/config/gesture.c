#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "sway/config.h"
#include "log.h"
#include <libtouch.h>

struct gesture_config *get_gesture_config(const char* identifier) {
  int i = list_seq_find(config->gesture_configs, gesture_identifier_cmp, identifier);
  
  struct gesture_config *cfg = NULL;
  if(i >= 0) {
    sway_log(SWAY_DEBUG, "Retrieving existing gesture");
    cfg = config->gesture_configs->items[i];
  } else {
    sway_log(SWAY_DEBUG, "Adding new gesture");
    cfg = new_gesture_config(identifier);
    list_add(config->gesture_configs, cfg);
  }

  return cfg;
  
}

struct gesture_config *new_gesture_config(const char *identifier) {
  struct gesture_config *cfg = calloc(sizeof(struct gesture_config), 1);
  cfg->identifier = strdup(identifier);
  cfg->gesture = libtouch_gesture_create(config->gesture_engine);

  return cfg;
}

int gesture_identifier_cmp(const void *item, const void *data) {
  const struct gesture_config *gc = item;
  const char *identifier = data;
  return strcmp(gc->identifier, identifier);
}

int gesture_libtouch_cmp(const void *item, const void *data) {
  const struct gesture_config *gc = item;
  const struct libtouch_gesture *g = data;
  return gc->gesture == g;
}
