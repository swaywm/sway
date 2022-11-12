#ifndef _SWAY_OPENGL_H
#define _SWAY_OPENGL_H

#include <GLES2/gl2.h>
#include <stdbool.h>

enum corner_location { NONE, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

struct gles2_tex_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint alpha;
	GLint pos_attrib;
	GLint tex_attrib;
	GLint size;
	GLint position;
	GLint radius;
	GLint saturation;
	GLint has_titlebar;
};

struct rounded_quad_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint size;
	GLint position;
	GLint radius;
};

struct fx_renderer {
	struct wlr_egl *egl;

	float projection[9];

	// Shaders
	struct {
		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
		} quad;

		struct rounded_quad_shader rounded_tl_quad;
		struct rounded_quad_shader rounded_tr_quad;

		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
			GLint is_top_left;
			GLint is_top_right;
			GLint is_bottom_left;
			GLint is_bottom_right;
			GLint position;
			GLint radius;
			GLint half_size;
			GLint half_thickness;
		} corner;

		struct gles2_tex_shader tex_rgba;
		struct gles2_tex_shader tex_rgbx;
		struct gles2_tex_shader tex_ext;
	} shaders;
};

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl);

void fx_renderer_begin(struct fx_renderer *renderer, uint32_t width, uint32_t height);

void fx_renderer_end();

void fx_renderer_clear(const float color[static 4]);

void fx_renderer_scissor(struct wlr_box *box);

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct wlr_texture *wlr_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		float alpha, int radius, float saturation, const bool has_titlebar);

bool fx_render_texture_with_matrix(struct fx_renderer *renderer, struct wlr_texture *wlr_texture,
		const struct wlr_box *dst_box, const float matrix[static 9], float alpha, int radius, float saturation,
		const bool has_titlebar);

void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9]);

void fx_render_rounded_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		int radius, enum corner_location corner_location);

void fx_render_border_corner(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		enum corner_location corner_location, int radius, int border_thickness);

#endif
