#include <string.h>
#include <wlr/render/wlr_renderer.h>
#include "cairo.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/container.h"

char* strcat_copy(const char *a, const char *b) {
    char *out;
    int a_len = strlen(a);
    int b_len = strlen(b);

    out = malloc(a_len + b_len + 1);

    memcpy(out, a, a_len);
    memcpy(out + a_len, b, b_len + 1);
    return out;
}

struct wlr_texture* wlr_texture_from_png(struct sway_output *output, char* folder_path,
		char* filename) {
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			output->wlr_output->backend);
	cairo_surface_t *image = cairo_image_surface_create_from_png(strcat_copy(
			folder_path, filename));
	return wlr_texture_from_pixels(renderer, WL_SHM_FORMAT_ARGB8888,
			cairo_image_surface_get_width(image) * 4,
			cairo_image_surface_get_width(image),
			cairo_image_surface_get_height(image),
			cairo_image_surface_get_data(image));
}

static struct cmd_results *handle_command(int argc, char **argv, char *cmd_name,
		struct border_textures *class) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	struct sway_output *output = root->outputs->items[0];
	class->top_left_corner = wlr_texture_from_png(output, argv[0], "0.png");
	class->top_edge = wlr_texture_from_png(output, argv[0], "1.png");
	class->top_right_corner = wlr_texture_from_png(output, argv[0], "2.png");
	class->right_edge = wlr_texture_from_png(output, argv[0], "3.png");
	class->bottom_right_corner = wlr_texture_from_png(output, argv[0], "4.png");
	class->bottom_edge = wlr_texture_from_png(output, argv[0], "5.png");
	class->bottom_left_corner = wlr_texture_from_png(output, argv[0], "6.png");
	class->left_edge = wlr_texture_from_png(output, argv[0], "7.png");
	sway_log(SWAY_DEBUG, "Assigned all textures.");

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_border_images_focused(int argc, char **argv) {
	return handle_command(argc, argv, "border_images.focused",
			&config->border_textures.focused);
}

struct cmd_results *cmd_border_images_focused_inactive(int argc, char **argv) {
	return handle_command(argc, argv, "border_images.focused_inactive",
			&config->border_textures.focused_inactive);
}

struct cmd_results *cmd_border_images_unfocused(int argc, char **argv) {
	return handle_command(argc, argv, "border_images.unfocused",
			&config->border_textures.unfocused);
}

struct cmd_results *cmd_border_images_urgent(int argc, char **argv) {
	return handle_command(argc, argv, "border_images.urgent",
			&config->border_textures.urgent);
}
