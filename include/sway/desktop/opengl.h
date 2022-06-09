#ifndef _SWAY_OPENGL_H
#define _SWAY_OPENGL_H

#include <GLES2/gl2.h>

struct gles2_tex_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint alpha;
	GLint pos_attrib;
	GLint tex_attrib;
};

struct gles2_renderer {
	struct wlr_egl *egl;

	float projection[9];

	uint32_t viewport_width;
	uint32_t viewport_height;

	// Shaders
	struct {
		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
		} quad;
		struct gles2_tex_shader tex_rgba;
		struct gles2_tex_shader tex_rgbx;
		struct gles2_tex_shader tex_ext;
	} shaders;
};

struct gles2_renderer *gles2_renderer_create(struct wlr_egl *egl);

#endif
