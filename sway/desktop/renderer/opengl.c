/*
  primarily stolen from:
  - https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render/gles2
  - https://github.com/vaxerski/Hyprland/blob/main/src/render/OpenGL.cpp
*/

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <wlr/backend.h> // TODO: remove if removed from renderer_init
#include <wlr/render/egl.h> // TODO: remove if removed from renderer_init
#include <wlr/types/wlr_matrix.h>
#include <wlr/util/box.h>
#include "log.h"
#include "sway/desktop/renderer/opengl.h"

// TODO: update to hyprland shaders (add sup for rounded corners + add blur shaders

static const GLfloat flip_180[] = {
	1.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, 0.0f, 1.0f,
};

static const GLfloat verts[] = {
	1, 0, // top right
	0, 0, // top left
	1, 1, // bottom right
	0, 1, // bottom left
};

static GLuint compile_shader(GLuint type, const GLchar *src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok == GL_FALSE) {
		glDeleteShader(shader);
		shader = 0;
	}

	return shader;
}

static GLuint link_program(const GLchar *vert_src, const GLchar *frag_src) {
	GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
	if (!vert) {
		goto error;
	}

	GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
	if (!frag) {
		glDeleteShader(vert);
		goto error;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	glDetachShader(prog, vert);
	glDetachShader(prog, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok == GL_FALSE) {
		glDeleteProgram(prog);
		goto error;
	}

	return prog;

error:
	return 0;
}

struct gles2_renderer *gles2_renderer_create(struct wlr_backend *backend) {
	// TODO: Hyprland way?
	// TODO: handle case of no drm_fd?
	int drm_fd = wlr_backend_get_drm_fd(backend);
	struct wlr_egl *egl = wlr_egl_create_with_drm_fd(drm_fd);
	if (egl == NULL) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not initialize EGL");
		return NULL;
	}
	if (!wlr_egl_make_current(egl)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not make EGL current");
		return NULL;
	}

	// get extensions
	const char *exts_str = (const char *)glGetString(GL_EXTENSIONS);
	if (exts_str == NULL) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Failed to get GL_EXTENSIONS");
		return NULL;
	}

	struct gles2_renderer *renderer = calloc(1, sizeof(struct gles2_renderer));
	if (renderer == NULL) {
		return NULL;
	}

	// TODO: needed?
	renderer->egl = egl;

	sway_log(SWAY_INFO, "Creating GLES2 renderer");
	sway_log(SWAY_INFO, "Using %s", glGetString(GL_VERSION));
	sway_log(SWAY_INFO, "GL vendor: %s", glGetString(GL_VENDOR));
	sway_log(SWAY_INFO, "GL renderer: %s", glGetString(GL_RENDERER));
	sway_log(SWAY_INFO, "Supported GLES2 extensions: %s", exts_str);

	// TODO: gl checks

	// init shaders
	GLuint prog;

	prog = link_program(renderer, quad_vertex_src, quad_fragment_src);
	renderer->shaders.quad.program = prog;
	if (!renderer->shaders.quad.program) {
		goto error;
	}
	renderer->shaders.quad.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.quad.color = glGetUniformLocation(prog, "color");
	renderer->shaders.quad.pos_attrib = glGetAttribLocation(prog, "pos");

	prog = link_program(renderer, tex_vertex_src, tex_fragment_src_rgba);
	renderer->shaders.tex_rgba.program = prog;
	if (!renderer->shaders.tex_rgba.program) {
		goto error;
	}
	renderer->shaders.tex_rgba.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgba.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgba.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgba.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_rgba.tex_attrib = glGetAttribLocation(prog, "texcoord");

	prog = link_program(renderer, tex_vertex_src, tex_fragment_src_rgbx);
	renderer->shaders.tex_rgbx.program = prog;
	if (!renderer->shaders.tex_rgbx.program) {
		goto error;
	}
	renderer->shaders.tex_rgbx.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgbx.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgbx.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgbx.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_rgbx.tex_attrib = glGetAttribLocation(prog, "texcoord");

	prog = link_program(renderer, tex_vertex_src, tex_fragment_src_external);
	renderer->shaders.tex_ext.program = prog;
	if (!renderer->shaders.tex_ext.program) {
		goto error;
	}
	renderer->shaders.tex_ext.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_ext.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_ext.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_ext.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_ext.tex_attrib = glGetAttribLocation(prog, "texcoord");

	wlr_egl_unset_current(renderer->egl);

	sway_log(SWAY_INFO, "GLES2 RENDERER: Shaders Initialized Successfully");
	return renderer;

error:
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);

	wlr_egl_unset_current(renderer->egl);

	free(renderer);

	sway_log(SWAY_ERROR, "GLES2 RENDERER: Error Initializing Shaders");
	return NULL;
}

// TODO: is gles2_get_renderer_in_context(wlr_renderer) implementation needed?
static void gles2_begin(struct gles2_renderer *renderer, uint32_t width, uint32_t height) {
	glViewport(0, 0, width, height);
	renderer->viewport_width = width;
	renderer->viewport_height = height;

	// refresh projection matrix
	wlr_matrix_projection(renderer->projection, width, height, WL_OUTPUT_TRANSFORM_NORMAL);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

/************************
  Rendering Functions
*************************/

static void gles2_render_rect(struct gles2_renderer *renderer, const struct wlr_box *box, const float color[static 4], const float projection[static 9]) {
	if (box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);
	float matrix[9];
	wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0, projection);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);
	wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	// OpenGL ES 2 requires the glUniformMatrix3fv transpose parameter to be set
	// to GL_FALSE
	wlr_matrix_transpose(gl_matrix, gl_matrix);

	if (color[3] == 1.0) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	glUseProgram(renderer->shaders.quad.program);

	glUniformMatrix3fv(renderer->shaders.quad.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(renderer->shaders.quad.color, color[0], color[1], color[2], color[3]);

	glVertexAttribPointer(renderer->shaders.quad.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(renderer->shaders.quad.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(renderer->shaders.quad.pos_attrib);
}
