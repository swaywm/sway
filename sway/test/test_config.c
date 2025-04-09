#include "sway/config.h"
#include "sway/output.h"
#include "sway/commands.h"
#include "sway/server.h"
#include "sway/criteria.h"
#include "pango.h"

void free_gesture_binding(struct sway_gesture_binding *binding) {
  return;
}

void free_bar_config(struct bar_config *bar) {
    return;
}

void free_sway_binding(struct sway_binding *binding) {
    return;
}

void free_switch_binding(struct sway_switch_binding *binding) {
    return;
}

void free_sway_variable(struct sway_variable *var) {
    return;
}

bool translate_binding(struct sway_binding *binding) {
    return true;
}

void input_config_fill_rule_names(struct input_config *ic,
    struct xkb_rule_names *rules) {
    return;
}

void binding_add_translated(struct sway_binding *binding,
    list_t *mode_bindings) {
        return;
}

int seat_name_cmp(const void *item, const void *data) {
    return 0;
}

void seat_execute_command(struct sway_seat *seat, struct sway_binding *binding) {
    return;
}

void swaynag_log(const char *swaynag_command, struct swaynag_instance *swaynag,
    const char *fmt, ...) {
        return;
}

void swaynag_show(struct swaynag_instance *swaynag) {
    return;
}

struct cmd_results *config_commands_command(char *exec) {
    struct cmd_results *results = NULL;
    results = cmd_results_new(CMD_SUCCESS, NULL);
    return results;
}

list_t *execute_command(char *_exec, struct sway_seat *seat,
    struct sway_container *con) {
        list_t *res_list = create_list();

	if (!res_list) {
		return NULL;
	}
    return res_list;
}

struct cmd_results *config_command(char *exec, char **new_block) {
	struct cmd_results *results = NULL;
	results = cmd_results_new(CMD_SUCCESS, NULL);
    return results;
}

struct cmd_results *cmd_results_new(enum cmd_status status,
    const char *format, ...) {
struct cmd_results *results = malloc(sizeof(struct cmd_results));
return results;
}

void free_cmd_results(struct cmd_results *results) {
	free(results);
}

void get_text_metrics(const PangoFontDescription *description, int *height, int *baseline) {
    return;
}

void request_modeset(void) {
    return;
}


void sway_switch_retrigger_bindings_for_all(void) {
    return;
}

void input_manager_reset_all_inputs(void) {
    return;
}

struct sway_seat *input_manager_get_seat(const char *seat_name, bool create) {
    struct sway_seat *seat = NULL;
    return seat;
}

void input_manager_verify_fallback_seat(void) {
    return;
}

void seat_destroy(struct sway_seat *seat) {
    return;
}

void free_workspace_config(struct workspace_config *wsc) {
    return;
}

void free_output_config(struct output_config *oc) {
    return;
}

void free_input_config(struct input_config *ic) {
    return;
}

void free_seat_config(struct seat_config *seat) {
    return;
}

void criteria_destroy(struct criteria *criteria) {
    return;
}

void input_manager_apply_input_config(struct input_config *input_config) {
    return;
}

void input_manager_apply_seat_config(struct seat_config *seat_config) {
    return;
}

void arrange_root(void) {
    return;
}

bool spawn_swaybg(void) {
    return true;
}

struct sway_server server = {0};


#include <getopt.h>

static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", required_argument, NULL, 'c'},
	{"validate", no_argument, NULL, 'C'},
	{"active", no_argument, NULL, 'A'},
	{0, 0, 0, 0}
};

static const char usage[] =
	"Usage: sway_test_config [options]\n"
	"\n"
	"  -h, --help             Show help message and quit.\n"
	"  -c, --config <config>  Specify a config file.\n"
	"  -C, --validate         Check the validity of the config file, then exit.\n"
	"  -A, --active           Set is_active to true.\n"
	"\n";

int main(int argc, char **argv) {
    bool active = false, validate = false, res;

	char *config_path = NULL;

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hCAc:", long_options, &option_index);
		if (c == -1) {
			break;
		}
        switch (c) {
            case 'h': // help
                printf("%s", usage);
                exit(EXIT_SUCCESS);
                break;
            case 'c':
                //free(config_path);
                config_path = strdup(optarg);
                break;
            case 'C':
                validate = true;
                break;
            case 'A':
                active = true;
                break;
            default:
                fprintf(stderr, "%s", usage);
                exit(EXIT_FAILURE);
            }
    }

    res = load_main_config(config_path, active, validate);
    return res ? 0 : 1;
}