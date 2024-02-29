#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_disable_titlebar(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "disable_titlebar", EXPECTED_EQUAL_TO, 1))) {
        return error;
    }

    config->disable_titlebar = parse_boolean(argv[0], config->disable_titlebar);

    return cmd_results_new(CMD_SUCCESS, NULL);
}
