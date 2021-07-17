#define _POSIX_C_SOURCE 200809L
#include <wlr/render/wlr_renderer.h>
#include <drm_fourcc.h>
#include <string.h>
#include "cairo.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

static void apply_border_textures_for_class(struct border_textures *class) {
	struct sway_output *output = root->outputs->items[0];
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			output->wlr_output->backend);
	class->texture = wlr_texture_from_pixels(renderer, DRM_FORMAT_ARGB8888,
		cairo_image_surface_get_width(class->image_surface) * 4,
		cairo_image_surface_get_width(class->image_surface),
		cairo_image_surface_get_height(class->image_surface),
		cairo_image_surface_get_data(class->image_surface));
}

static struct cmd_results *handle_command(int argc, char **argv, char *cmd_name,
		struct border_textures *class) {
	if (!config->active) return cmd_results_new(CMD_DEFER, NULL);

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *src = strdup(argv[0]);
	if (!expand_path(&src)) {
		error = cmd_results_new(CMD_INVALID, "Invalid syntax (%s)", src);
		free(src);
		src = NULL;
		return error;
	}
	if (!src) {
		sway_log(SWAY_ERROR, "Failed to allocate expanded path");
		return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
	}

	bool can_access = access(src, F_OK) != -1;
	if (!can_access) {
		sway_log_errno(SWAY_ERROR, "Unable to access border images file '%s'",
				src);
		config_add_swaynag_warning("Unable to access border images file '%s'",
				src);
	}

	class->image_surface = cairo_image_surface_create_from_png(src);
	apply_border_textures_for_class(class);

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
