# Contributing to sway

Contributing just involves sending a pull request. You will probably be more
successful with your contribution if you visit the [IRC
channel](http://webchat.freenode.net/?channels=sway&uio=d4) upfront and discuss
your plans.

## Pull Requests

If you already have your own pull request habits, feel free to use them. If you
don't, however, allow me to make a suggestion: feature branches pulled from
upstream. Try this:

1. Fork sway
2. Clone your fork
3. git remote add upstream git://github.com/SirCmpwn/sway.git

You only need to do this once. You're never going to use your fork's master
branch. Instead, when you start working on a feature, do this:

1. git fetch upstream
2. git checkout -b add-so-and-so-feature upstream/master
3. work
4. git push -u origin add-so-and-so-feature
5. Make pull request from your feature branch

## Writing Tests

Tests are driven by [CMocka](https://cmocka.org/). When testing a given
function, we can "mock" out the functions it relies on to program their behavior
explicitly and test the function in isolation. The directory layout of `test/`
is identical to the global directory layout, but each C file in the parent tree
has its own directory in the test tree, with its own CMakeLists.txt that wires
things up. To add a test, make the appropriate directory in `test/` and add a
CMakeLists.txt that looks something like this made-up example:

```cmake
configure_test(
    SUBPROJECT swaymsg
    NAME main
    SOURCES
        ${PROJECT_SOURCE_DIR}/swaymsg/main.c
        swaymsg.c
    WRAPPERS
        ipc_open_socket
    LIBRARIES
        ${WLC_LIBRARIES}
    INCLUDES
        ${WLC_INCLUDES}
)
```

This defines a test suite in the swaymsg subproject that tests main. This file
would live at `test/swaymsg/main/CMakeLists.txt`. It specifies that it requires
`swaymsg/main.c` and `test/swaymsg/main/swaymsg.c`, the former being the actual
swaymsg source and the latter being the test suite. It mocks ipc_open_socket and
links against openssl. See the cmocka documentation or read existing tests to
learn more about how mocks work.

## Coding Style

Sway is written in C. The style guidelines is [kernel
style](https://www.kernel.org/doc/Documentation/CodingStyle), but all braces go
on the same line (*"but K&R says so!" is a silly way of justifying something*).
Some points to note:

* Do not use typedefs unless you have a good reason
* Do not use macros unless you have a *really* good reason
* Align `case` with `switch`
* Tabs, not spaces
* `char *pointer` - note position of `*`
* Use logging with reckless abandon
* Always include braces for if/for/while/etc, even for one-liners

An example of well formatted code:

```C
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "example.h"

struct foobar {
	char *foo;
	int bar;
	long baz;
}; // Do not typedef without a good reason

int main(int argc, const char **argv) {
	if (argc != 4) {
		sway_abort("Do not run this program manually. See man 5 sway and look for output options.");
	}

	if (!registry->desktop_shell) {
		sway_abort("swaybg requires the compositor to support the desktop-shell extension.");
	}

	int desired_output = atoi(argv[1]);
	sway_log(L_INFO, "Using output %d of %d", desired_output, registry->outputs->length);
	int i;
	struct output_state *output = registry->outputs->items[desired_output];
	struct window *window = window_setup(registry, 100, 100, false);
	if (!window) {
		sway_abort("Failed to create surfaces.");
	}
	window->width = output->width;
	window->height = output->height;
	desktop_shell_set_background(registry->desktop_shell, output->output, window->surface);
	list_add(surfaces, window);

	cairo_surface_t *image = cairo_image_surface_create_from_png(argv[2]);
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	const char *scaling_mode_str = argv[3];
	enum scaling_mode scaling_mode;
	if (strcmp(scaling_mode_str, "stretch") == 0) {
		scaling_mode = SCALING_MODE_STRETCH;
	} else if (strcmp(scaling_mode_str, "fill") == 0) {
		scaling_mode = SCALING_MODE_FILL;
	} else if (strcmp(scaling_mode_str, "fit") == 0) {
		scaling_mode = SCALING_MODE_FIT;
	} else if (strcmp(scaling_mode_str, "center") == 0) {
		scaling_mode = SCALING_MODE_CENTER;
	} else if (strcmp(scaling_mode_str, "tile") == 0) {
		scaling_mode = SCALING_MODE_TILE;
	} else {
		sway_abort("Unsupported scaling mode: %s", scaling_mode_str);
	}

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		if (window_prerender(window) && window->cairo) {
			switch (scaling_mode) {
			case SCALING_MODE_STRETCH:
				cairo_scale(window->cairo,
						(double) window->width / width,
						(double) window->height / height);
				cairo_set_source_surface(window->cairo, image, 0, 0);
				break;
			case SCALING_MODE_FILL:
			{
				double window_ratio = (double) window->width / window->height;
				double bg_ratio = width / height;

				if (window_ratio > bg_ratio) {
					double scale = (double) window->width / width;
					cairo_scale(window->cairo, scale, scale);
					cairo_set_source_surface(window->cairo, image,
							0,
							(double) window->height/2 / scale - height/2);
				} else {
					double scale = (double) window->height / height;
					cairo_scale(window->cairo, scale, scale);
					cairo_set_source_surface(window->cairo, image,
							(double) window->width/2 / scale - width/2,
							0);
				}
				break;
			}
			case SCALING_MODE_FIT:
			{
				double window_ratio = (double) window->width / window->height;
				double bg_ratio = width / height;

				if (window_ratio > bg_ratio) {
					double scale = (double) window->height / height;
					cairo_scale(window->cairo, scale, scale);
					cairo_set_source_surface(window->cairo, image,
							(double) window->width/2 / scale - width/2,
							0);
				} else {
					double scale = (double) window->width / width;
					cairo_scale(window->cairo, scale, scale);
					cairo_set_source_surface(window->cairo, image,
							0,
							(double) window->height/2 / scale - height/2);
				}
				break;
			}
			case SCALING_MODE_CENTER:
				cairo_set_source_surface(window->cairo, image,
						(double) window->width/2 - width/2,
						(double) window->height/2 - height/2);
				break;
			case SCALING_MODE_TILE:
			{
				cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
				cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
				cairo_set_source(window->cairo, pattern);
				break;
			}
			default:
				sway_abort("Scaling mode '%s' not implemented yet!", scaling_mode_str);
			}

			cairo_paint(window->cairo);

			window_render(window);
		}
	}

	while (wl_display_dispatch(registry->display) != -1);

	for (i = 0; i < surfaces->length; ++i) {
		struct window *window = surfaces->items[i];
		window_teardown(window);
	}
	list_free(surfaces);
	registry_teardown(registry);
	return 0;
}
```
