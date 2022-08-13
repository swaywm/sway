/*
  primarily stolen from:
  - https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render/gles2
  - https://github.com/vaxerski/Hyprland/blob/main/src/render/OpenGL.cpp
*/

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/util/box.h>
#include "log.h"
#include "sway/desktop/fx_renderer.h"
#include "sway/output.h"
#include "sway/server.h"

// TODO: update to hyprland shaders (add sup for rounded corners + add blur shaders)

/************************
  Shaders
*************************/

// Colored quads
const GLchar quad_vertex_src[] =
"uniform mat3 proj;\n"
"uniform vec4 color;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"	v_color = color;\n"
"	v_texcoord = texcoord;\n"
"}\n";

const GLchar quad_fragment_src[] =
"precision mediump float;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_FragColor = v_color;\n"
"}\n";

// Textured quads
const GLchar tex_vertex_src[] =
"uniform mat3 proj;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"	v_texcoord = texcoord;\n"
"}\n";

const GLchar tex_fragment_src_rgba[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"\n"
"void main() {\n"
"	gl_FragColor = texture2D(tex, v_texcoord) * alpha;\n"
"}\n";

const GLchar tex_fragment_src_rgbx[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"\n"
"void main() {\n"
"	gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;\n"
"}\n";

const GLchar tex_fragment_src_external[] =
"#extension GL_OES_EGL_image_external : require\n\n"
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform samplerExternalOES texture0;\n"
"uniform float alpha;\n"
"\n"
"void main() {\n"
"	gl_FragColor = texture2D(texture0, v_texcoord) * alpha;\n"
"}\n";

/************************
  Matrix Consts
*************************/
/*
static const GLfloat flip_180[] = {
	1.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, 0.0f, 1.0f,
};
*/

static const GLfloat verts[] = {
	1, 0, // top right
	0, 0, // top left
	1, 1, // bottom right
	0, 1, // bottom left
};

/************************
  General Functions
*************************/

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

// TODO: Hyprland way?
// TODO: instead of server, have param be server->backend like wlr_renderer_autocreate
struct fx_renderer *fx_renderer_create(struct wlr_egl *egl) {
	struct fx_renderer *renderer = calloc(1, sizeof(struct fx_renderer));
	if (renderer == NULL) {
		return NULL;
	}

	// TODO: wlr_egl_make_current or eglMakeCurrent?
	// TODO: assert instead of conditional statement?
	if (!wlr_egl_make_current(egl)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not make EGL current");
		return NULL;
	}
	// TODO: needed?
	renderer->egl = egl;

	// get extensions
	const char *exts_str = (const char *)glGetString(GL_EXTENSIONS);
	if (exts_str == NULL) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Failed to get GL_EXTENSIONS");
		return NULL;
	}

	sway_log(SWAY_INFO, "Creating swayfx GLES2 renderer");
	sway_log(SWAY_INFO, "Using %s", glGetString(GL_VERSION));
	sway_log(SWAY_INFO, "GL vendor: %s", glGetString(GL_VENDOR));
	sway_log(SWAY_INFO, "GL renderer: %s", glGetString(GL_RENDERER));
	sway_log(SWAY_INFO, "Supported GLES2 extensions: %s", exts_str);

	// TODO: gl checks

	// init shaders
	GLuint prog;

	prog = link_program(quad_vertex_src, quad_fragment_src);
	renderer->shaders.quad.program = prog;
	if (!renderer->shaders.quad.program) {
		goto error;
	}
	renderer->shaders.quad.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.quad.color = glGetUniformLocation(prog, "color");
	renderer->shaders.quad.pos_attrib = glGetAttribLocation(prog, "pos");

	prog = link_program(tex_vertex_src, tex_fragment_src_rgba);
	renderer->shaders.tex_rgba.program = prog;
	if (!renderer->shaders.tex_rgba.program) {
		goto error;
	}
	renderer->shaders.tex_rgba.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgba.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgba.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgba.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_rgba.tex_attrib = glGetAttribLocation(prog, "texcoord");

	prog = link_program(tex_vertex_src, tex_fragment_src_rgbx);
	renderer->shaders.tex_rgbx.program = prog;
	if (!renderer->shaders.tex_rgbx.program) {
		goto error;
	}
	renderer->shaders.tex_rgbx.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_rgbx.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_rgbx.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_rgbx.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_rgbx.tex_attrib = glGetAttribLocation(prog, "texcoord");

	prog = link_program(tex_vertex_src, tex_fragment_src_external);
	renderer->shaders.tex_ext.program = prog;
	if (!renderer->shaders.tex_ext.program) {
		goto error;
	}
	renderer->shaders.tex_ext.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.tex_ext.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.tex_ext.alpha = glGetUniformLocation(prog, "alpha");
	renderer->shaders.tex_ext.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.tex_ext.tex_attrib = glGetAttribLocation(prog, "texcoord");

	// TODO: if remove renderer->egl, replace below with r->egl
	wlr_egl_unset_current(renderer->egl);

	sway_log(SWAY_INFO, "GLES2 RENDERER: Shaders Initialized Successfully");
	return renderer;

error:
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);

	wlr_egl_unset_current(renderer->egl);

	// TODO: more freeing?
	free(renderer);

	sway_log(SWAY_ERROR, "GLES2 RENDERER: Error Initializing Shaders");
	return NULL;
}

void fx_renderer_begin(struct fx_renderer *renderer, uint32_t width, uint32_t height) {
	//push_gles2_debug(renderer);

	glViewport(0, 0, width, height);

	// refresh projection matrix
	wlr_matrix_projection(renderer->projection, width, height,
		WL_OUTPUT_TRANSFORM_FLIPPED_180);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	//pop_gles2_debug(renderer);

}

void fx_renderer_end() {

}

void fx_renderer_clear(const float color[static 4]) {
		glClearColor(color[0], color[1], color[2], color[3]);
		glClear(GL_COLOR_BUFFER_BIT);
}

void fx_renderer_scissor(struct wlr_box *box) {
		if (box) {
				glScissor(box->x, box->y, box->width, box->height);
				glEnable(GL_SCISSOR_TEST);
		} else {
				glDisable(GL_SCISSOR_TEST);
		}
}

/************************
  Rendering Functions
*************************/

/*
static void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box
*box, const float color[static 4], const float projection[static 9]) { if
(box->width == 0 || box->height == 0) { return;
		}
		assert(box->width > 0 && box->height > 0);
		float matrix[9];
		wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
projection);

		float gl_matrix[9];
		wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);
		wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

		// OpenGL ES 2 requires the glUniformMatrix3fv transpose parameter to be
set to GL_FALSE wlr_matrix_transpose(gl_matrix, gl_matrix);

		if (color[3] == 1.0) {
				glDisable(GL_BLEND);
		} else {
				glEnable(GL_BLEND);
		}

		glUseProgram(renderer->shaders.quad.program);

		glUniformMatrix3fv(renderer->shaders.quad.proj, 1, GL_FALSE, gl_matrix);
		glUniform4f(renderer->shaders.quad.color, color[0], color[1], color[2],
color[3]);

		glVertexAttribPointer(renderer->shaders.quad.pos_attrib, 2, GL_FLOAT,
GL_FALSE, 0, verts);

		glEnableVertexAttribArray(renderer->shaders.quad.pos_attrib);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray(renderer->shaders.quad.pos_attrib);
}
*/

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer,
		struct wlr_texture *wlr_texture, const struct wlr_fbox *box,
		const float matrix[static 9], float alpha) {

	assert(wlr_texture_is_gles2(wlr_texture));
	struct wlr_gles2_texture_attribs texture_attrs;
	wlr_gles2_texture_get_attribs(wlr_texture, &texture_attrs);

	struct gles2_tex_shader *shader = NULL;

	switch (texture_attrs.target) {
	case GL_TEXTURE_2D:
		if (texture_attrs.has_alpha) {
			shader = &renderer->shaders.tex_rgba;
		} else {
			shader = &renderer->shaders.tex_rgbx;
		}
		break;
	/*
	case GL_TEXTURE_EXTERNAL_OES:
		shader = &renderer->shaders.tex_ext;

		// TODO: ADD ME ONCE EXTS ADDED TO RENDERER
		if (!renderer->exts.OES_egl_image_external) {
			sway_log(SWAY_ERROR, "Failed to render texture: "
				"GL_TEXTURE_EXTERNAL_OES not supported");
			return false;
		}
		break;
		*/
	default:
		sway_log(SWAY_ERROR, "Aborting render");
		abort();
	}

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// OpenGL ES 2 requires the glUniformMatrix3fv transpose parameter to be set
	// to GL_FALSE
	wlr_matrix_transpose(gl_matrix, gl_matrix);

	// push_gles2_debug(renderer);

	if (!texture_attrs.has_alpha && alpha == 1.0) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture_attrs.target, texture_attrs.tex);

	glTexParameteri(texture_attrs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glUseProgram(shader->program);

	glUniformMatrix3fv(shader->proj, 1, GL_FALSE, gl_matrix);
	glUniform1i(shader->tex, 0);
	glUniform1f(shader->alpha, alpha);

	const GLfloat x1 = box->x / wlr_texture->width;
	const GLfloat y1 = box->y / wlr_texture->height;
	const GLfloat x2 = (box->x + box->width) / wlr_texture->width;
	const GLfloat y2 = (box->y + box->height) / wlr_texture->height;
	const GLfloat texcoord[] = {
		x2, y1, // top right
		x1, y1, // top left
		x2, y2, // bottom right
		x1, y2, // bottom left
	};

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, texcoord);

	glEnableVertexAttribArray(shader->pos_attrib);
	glEnableVertexAttribArray(shader->tex_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader->pos_attrib);
	glDisableVertexAttribArray(shader->tex_attrib);

	glBindTexture(texture_attrs.target, 0);

	// pop_gles2_debug(renderer);
	return true;
}

bool fx_render_texture_with_matrix(struct fx_renderer *renderer,
		struct wlr_texture *wlr_texture, const float matrix[static 9], float alpha) {
	struct wlr_fbox box = {
		.x = 0,
		.y = 0,
		.width = wlr_texture->width,
		.height = wlr_texture->height,
	};
	return fx_render_subtexture_with_matrix(renderer, wlr_texture, &box, matrix, alpha);
}
