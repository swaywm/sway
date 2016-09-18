#include <json-c/json.h>
#include "wlc/wlc.h"

void init_json_tree(int socketfd);
void free_json_tree();
char *get_focused_output();
char *create_payload(const char *output, struct wlc_geometry *g);
struct wlc_geometry *get_container_geometry(json_object *container);
json_object *get_focused_container();
json_object *get_output_container(const char *output);
