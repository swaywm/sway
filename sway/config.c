#include <stdio.h>
#include <stdlib.h>
#include "readline.h"
#include "stringop.h"
#include "list.h"
#include "config.h"

struct sway_config *read_config(FILE *file) {
	struct sway_config *config = malloc(sizeof(struct sway_config));
	config->symbols = create_list();
	config->modes = create_list();

	while (!feof(file)) {
		int _;
		char *line = read_line(file);
		line = strip_whitespace(line, &_);
		line = strip_comments(line);
		if (!line[0]) {
			goto _continue;
		}
		printf("Parsing config line %s\n", line);
_continue:
		free(line);
	}

	return config;
}
