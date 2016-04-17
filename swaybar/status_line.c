#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

#include "log.h"
#include "bar/config.h"
#include "bar/status_line.h"

#define I3JSON_MAXDEPTH 4
#define I3JSON_UNKNOWN 0
#define I3JSON_ARRAY 1
#define I3JSON_STRING 2

struct {
	int bufsize;
	char *buffer;
	char *line_start;
	char *parserpos;
	bool escape;
	int depth;
	int bar[I3JSON_MAXDEPTH+1];
} i3json_state = { 0, NULL, NULL, NULL, false, 0, { I3JSON_UNKNOWN } };

static char line[1024];
static char line_rest[1024];

static void free_status_block(void *item) {
	if (!item) {
		return;
	}
	struct status_block *sb = (struct status_block*)item;
	if (sb->full_text) {
		free(sb->full_text);
	}
	if (sb->short_text) {
		free(sb->short_text);
	}
	if (sb->align) {
		free(sb->align);
	}
	if (sb->name) {
		free(sb->name);
	}
	if (sb->instance) {
		free(sb->instance);
	}
	free(sb);
}

static void parse_json(struct bar *bar, const char *text) {
	json_object *results = json_tokener_parse(text);
	if (!results) {
		sway_log(L_DEBUG, "Failed to parse json");
		return;
	}

	if (json_object_array_length(results) < 1) {
		return;
	}

	if (bar->status->block_line) {
		list_foreach(bar->status->block_line, free_status_block);
		list_free(bar->status->block_line);
	}

	bar->status->block_line = create_list();

	int i;
	for (i = 0; i < json_object_array_length(results); ++i) {
		json_object *full_text, *short_text, *color, *min_width, *align, *urgent;
		json_object *name, *instance, *separator, *separator_block_width;
		json_object *background, *border, *border_top, *border_bottom;
		json_object *border_left, *border_right, *markup;

		json_object *json = json_object_array_get_idx(results, i);
		if (!json) {
			continue;
		}

		json_object_object_get_ex(json, "full_text", &full_text);
		json_object_object_get_ex(json, "short_text", &short_text);
		json_object_object_get_ex(json, "color", &color);
		json_object_object_get_ex(json, "min_width", &min_width);
		json_object_object_get_ex(json, "align", &align);
		json_object_object_get_ex(json, "urgent", &urgent);
		json_object_object_get_ex(json, "name", &name);
		json_object_object_get_ex(json, "instance", &instance);
		json_object_object_get_ex(json, "markup", &markup);
		json_object_object_get_ex(json, "separator", &separator);
		json_object_object_get_ex(json, "separator_block_width", &separator_block_width);
		json_object_object_get_ex(json, "background", &background);
		json_object_object_get_ex(json, "border", &border);
		json_object_object_get_ex(json, "border_top", &border_top);
		json_object_object_get_ex(json, "border_bottom", &border_bottom);
		json_object_object_get_ex(json, "border_left", &border_left);
		json_object_object_get_ex(json, "border_right", &border_right);

		struct status_block *new = calloc(1, sizeof(struct status_block));

		if (full_text) {
			new->full_text = strdup(json_object_get_string(full_text));
		}

		if (short_text) {
			new->short_text = strdup(json_object_get_string(short_text));
		}

		if (color) {
			new->color = parse_color(json_object_get_string(color));
		} else {
			new->color = bar->config->colors.statusline;
		}

		if (min_width) {
			json_type type = json_object_get_type(min_width);
			if (type == json_type_int) {
				new->min_width = json_object_get_int(min_width);
			} else if (type == json_type_string) {
				/* the width will be calculated when rendering */
				new->min_width = 0;
			}
		}

		if (align) {
			new->align = strdup(json_object_get_string(align));
		} else {
			new->align = strdup("left");
		}

		if (urgent) {
			new->urgent = json_object_get_int(urgent);
		}

		if (name) {
			new->name = strdup(json_object_get_string(name));
		}

		if (instance) {
			new->instance = strdup(json_object_get_string(instance));
		}

		if (markup) {
			new->markup = json_object_get_boolean(markup);
		}

		if (separator) {
			new->separator = json_object_get_int(separator);
		} else {
			new->separator = true; // i3bar spec
		}

		if (separator_block_width) {
			new->separator_block_width = json_object_get_int(separator_block_width);
		} else {
			new->separator_block_width = 9; // i3bar spec
		}

		// Airblader features
		if (background) {
			new->background = parse_color(json_object_get_string(background));
		} else {
			new->background = 0x0; // transparent
		}

		if (border) {
			new->border = parse_color(json_object_get_string(border));
		} else {
			new->border = 0x0; // transparent
		}

		if (border_top) {
			new->border_top = json_object_get_int(border_top);
		} else {
			new->border_top = 1;
		}

		if (border_bottom) {
			new->border_bottom = json_object_get_int(border_bottom);
		} else {
			new->border_bottom = 1;
		}

		if (border_left) {
			new->border_left = json_object_get_int(border_left);
		} else {
			new->border_left = 1;
		}

		if (border_right) {
			new->border_right = json_object_get_int(border_right);
		} else {
			new->border_right = 1;
		}

		list_add(bar->status->block_line, new);
	}

	json_object_put(results);
}

// continue parsing from last parserpos
static int i3json_parse(struct bar *bar) {
	char *c = i3json_state.parserpos;
	int handled = 0;
	while (*c) {
		if (i3json_state.bar[i3json_state.depth] == I3JSON_STRING) {
			if (!i3json_state.escape && *c == '"') {
				--i3json_state.depth;
			}
			i3json_state.escape = !i3json_state.escape && *c == '\\';
		} else {
			switch (*c) {
			case '[':
				++i3json_state.depth;
				if (i3json_state.depth > I3JSON_MAXDEPTH) {
					sway_abort("JSON too deep");
				}
				i3json_state.bar[i3json_state.depth] = I3JSON_ARRAY;
				if (i3json_state.depth == 2) {
					i3json_state.line_start = c;
				}
				break;
			case ']':
				if (i3json_state.bar[i3json_state.depth] != I3JSON_ARRAY) {
					sway_abort("JSON malformed");
				}
				--i3json_state.depth;
				if (i3json_state.depth == 1) {
					// c[1] is valid since c[0] != '\0'
					char p = c[1];
					c[1] = '\0';
					parse_json(bar, i3json_state.line_start);
					c[1] = p;
					++handled;
					i3json_state.line_start = c+1;
				}
				break;
			case '"':
				++i3json_state.depth;
				if (i3json_state.depth > I3JSON_MAXDEPTH) {
					sway_abort("JSON too deep");
				}
				i3json_state.bar[i3json_state.depth] = I3JSON_STRING;
				break;
			}
		}
		++c;
	}
	i3json_state.parserpos = c;
	return handled;
}

// Read line from file descriptor, only show the line tail if it is too long.
// In non-blocking mode treat "no more data" as a linebreak.
// If data after a line break has been read, return it in rest.
// If rest is non-empty, then use that as the start of the next line.
static int read_line_tail(int fd, char *buf, int nbyte, char *rest) {
	if (fd < 0 || !buf || !nbyte) {
		return -1;
	}
	int l;
	char *buffer = malloc(nbyte*2+1);
	char *readpos = buffer;
	char *lf;
	// prepend old data to new line if necessary
	if (rest) {
		l = strlen(rest);
		if (l > nbyte) {
			strcpy(buffer, rest + l - nbyte);
			readpos += nbyte;
		} else if (l) {
			strcpy(buffer, rest);
			readpos += l;
		}
	}
	// read until a linefeed is found or no more data is available
	while ((l = read(fd, readpos, nbyte)) > 0) {
		readpos[l] = '\0';
		lf = strchr(readpos, '\n');
		if (lf) {
			// linefeed found, replace with \0
			*lf = '\0';
			// give data from the end of the line, try to fill the buffer
			if (lf-buffer > nbyte) {
				strcpy(buf, lf - nbyte + 1);
			} else {
				strcpy(buf, buffer);
			}
			// we may have read data from the next line, save it to rest
			if (rest) {
				rest[0] = '\0';
				strcpy(rest, lf + 1);
			}
			free(buffer);
			return strlen(buf);
		} else {
			// no linefeed found, slide data back.
			int overflow = readpos - buffer + l - nbyte;
			if (overflow > 0) {
				memmove(buffer, buffer + overflow , nbyte + 1);
			}
		}
	}
	if (l < 0) {
		free(buffer);
		return l;
	}
	readpos[l]='\0';
	if (rest) {
		rest[0] = '\0';
	}
	if (nbyte < readpos - buffer + l - 1) {
		memcpy(buf, readpos - nbyte + l + 1, nbyte);
	} else {
		strncpy(buf, buffer, nbyte);
	}
	buf[nbyte-1] = '\0';
	free(buffer);
	return strlen(buf);
}

// make sure that enough buffer space is available starting from parserpos
static void i3json_ensure_free(int min_free) {
	int _step = 10240;
	int r = min_free % _step;
	if (r) {
		min_free += _step - r;
	}
	if (!i3json_state.buffer) {
		i3json_state.buffer = malloc(min_free);
		i3json_state.bufsize = min_free;
		i3json_state.parserpos = i3json_state.buffer;
	} else {
		int len = 0;
		int pos = 0;
		if (i3json_state.line_start) {
			len = strlen(i3json_state.line_start);
			pos = i3json_state.parserpos - i3json_state.line_start;
			if (i3json_state.line_start != i3json_state.buffer) {
				memmove(i3json_state.buffer, i3json_state.line_start, len+1);
			}
		} else {
			len = strlen(i3json_state.buffer);
		}
		if (i3json_state.bufsize < len+min_free) {
			i3json_state.bufsize += min_free;
			if (i3json_state.bufsize > 1024000) {
				sway_abort("Status line json too long or malformed.");
			}
			i3json_state.buffer = realloc(i3json_state.buffer, i3json_state.bufsize);
			if (!i3json_state.buffer) {
				sway_abort("Could not allocate json buffer");
			}
		}
		if (i3json_state.line_start) {
			i3json_state.line_start = i3json_state.buffer;
			i3json_state.parserpos = i3json_state.buffer + pos;
		} else {
			i3json_state.parserpos = i3json_state.buffer;
		}
	}
	if (!i3json_state.buffer) {
		sway_abort("Could not allocate buffer.");
	}
}

// append data and parse it.
static int i3json_handle_data(struct bar *bar, char *data) {
	int len = strlen(data);
	i3json_ensure_free(len);
	strcpy(i3json_state.parserpos, data);
	return i3json_parse(bar);
}

// read data from fd and parse it.
static int i3json_handle_fd(struct bar *bar) {
	i3json_ensure_free(10240);
	// get fresh data at the end of the buffer
	int readlen = read(bar->status_read_fd, i3json_state.parserpos, 10239);
	if (readlen < 0) {
		return readlen;
	}
	i3json_state.parserpos[readlen] = '\0';
	return i3json_parse(bar);
}

bool handle_status_line(struct bar *bar) {
	bool dirty = false;

	switch (bar->status->protocol) {
	case I3BAR:
		sway_log(L_DEBUG, "Got i3bar protocol.");
		if (i3json_handle_fd(bar) > 0) {
			dirty = true;
		}
		break;
	case TEXT:
		sway_log(L_DEBUG, "Got text protocol.");
		read_line_tail(bar->status_read_fd, line, sizeof(line), line_rest);
		dirty = true;
		bar->status->text_line = line;
		break;
	case UNDEF:
		sway_log(L_DEBUG, "Detecting protocol...");
		if (read_line_tail(bar->status_read_fd, line, sizeof(line), line_rest) < 0) {
			break;
		}
		dirty = true;
		bar->status->text_line = line;
		bar->status->protocol = TEXT;
		if (line[0] == '{') {
			// detect i3bar json protocol
			json_object *proto = json_tokener_parse(line);
			json_object *version;
			if (proto) {
				if (json_object_object_get_ex(proto, "version", &version)
							&& json_object_get_int(version) == 1
				) {
					sway_log(L_DEBUG, "Switched to i3bar protocol.");
					bar->status->protocol = I3BAR;
					i3json_handle_data(bar, line_rest);
				}
				json_object_put(proto);
			}
		}
		break;
	}

	return dirty;
}

struct status_line *init_status_line() {
	struct status_line *line = malloc(sizeof(struct status_line));
	line->block_line = create_list();
	line->text_line = NULL;
	line->protocol = UNDEF;

	return line;
}

void free_status_line(struct status_line *line) {
	if (line->block_line) {
		list_foreach(line->block_line, free_status_block);
		list_free(line->block_line);
	}
}
