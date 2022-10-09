#ifndef _SWAY_SHADERS_H
#define _SWAY_SHADERS_H

#include <GLES2/gl2.h>

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
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 position;\n"
"uniform float radius;\n"
"\n"
"void main() {\n"
"   gl_FragColor = texture2D(tex, v_texcoord) * alpha;\n"
"   vec2 corner_distance = min(gl_FragCoord.xy - position, position + vec2(width, height) - gl_FragCoord.xy);\n"
"   if (max(corner_distance.x, corner_distance.y) < radius) {\n"
"		float d = radius - distance(corner_distance, vec2(radius, radius));\n"
"		float smooth = smoothstep(-1.0f, 0.5f, d);\n"
"		gl_FragColor = mix(vec4(0), gl_FragColor, smooth);\n"
"   }\n"
"}\n";

const GLchar tex_fragment_src_rgbx[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 position;\n"
"uniform float radius;\n"
"\n"
"void main() {\n"
"	gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;\n"
"   vec2 corner_distance = min(gl_FragCoord.xy - position, position + vec2(width, height) - gl_FragCoord.xy);\n"
"   if (max(corner_distance.x, corner_distance.y) < radius) {\n"
"		float d = radius - distance(corner_distance, vec2(radius, radius));\n"
"		float smooth = smoothstep(-1.0f, 0.5f, d);\n"
"		gl_FragColor = mix(vec4(0), gl_FragColor, smooth);\n"
"   }\n"
"}\n";

const GLchar tex_fragment_src_external[] =
"#extension GL_OES_EGL_image_external : require\n\n"
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform samplerExternalOES texture0;\n"
"uniform float alpha;\n"
"\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 position;\n"
"uniform float radius;\n"
"\n"
"void main() {\n"
"	gl_FragColor = texture2D(texture0, v_texcoord) * alpha;\n"
"   vec2 corner_distance = min(gl_FragCoord.xy - position, position + vec2(width, height) - gl_FragCoord.xy);\n"
"   if (max(corner_distance.x, corner_distance.y) < radius) {\n"
"		float d = radius - distance(corner_distance, vec2(radius, radius));\n"
"		float smooth = smoothstep(-1.0f, 0.5f, d);\n"
"		gl_FragColor = mix(vec4(0), gl_FragColor, smooth);\n"
"   }\n"
"}\n";

const GLchar corner_fragment_src[] =
"precision mediump float;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord;\n"
"\n"
"uniform bool is_top_left;\n"
"uniform bool is_top_right;\n"
"uniform bool is_bottom_left;\n"
"uniform bool is_bottom_right;\n"
"\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 position;\n"
"uniform float radius;\n"
"uniform float thickness;\n"
"\n"
"float roundedBoxSDF(vec2 center, vec2 size, float radius) {\n"
"	return length(max(abs(center) - size + radius, 0.0)) - radius;\n"
"}\n"
"\n"
"void main() {\n"
"	gl_FragColor = v_color;\n"
"	vec2 size = vec2(width, height);\n"
"	vec2 pos = vec2(position.x - (width + thickness) * 0.5, position.y -\n"
"       (width + thickness) * 0.5);\n"
"	vec2 rel_pos = gl_FragCoord.xy - pos - size - thickness * 0.5;\n"
"\n"
"	float distance = roundedBoxSDF(\n"
"       rel_pos,\n" // Center
"		(size - thickness) * 0.5,\n" // Size
"		radius + thickness * 0.5\n" // Radius
"    );\n"
"\n"
"	float smoothedAlphaOuter = 1.0 - smoothstep(-1.0, 1.0, distance - thickness * 0.5);\n"
	// Creates a inner circle that isn't as anti-aliased as the outer ring
"	float smoothedAlphaInner = 1.0 - smoothstep(-1.0, 0.5, distance + thickness * 0.5);\n"
"	gl_FragColor = mix(vec4(0), gl_FragColor, smoothedAlphaOuter - smoothedAlphaInner);\n"
"\n"
// top left
"	if (is_top_left && (rel_pos.y > 0.0 || rel_pos.x > 0.0)) {\n"
"		discard;\n"
"	}\n"
// top right
"	else if (is_top_right && (rel_pos.y > 0.0 || rel_pos.x < 0.0)) {\n"
"		discard;\n"
"	}\n"
// bottom left
"	else if (is_bottom_left && (rel_pos.y < 0.0 || rel_pos.x > 0.0)) {\n"
"		discard;\n"
"	}\n"
// bottom right
"	else if (is_bottom_right && (rel_pos.y < 0.0 || rel_pos.x < 0.0)) {\n"
"		discard;\n"
"	}\n"
"}\n";

#endif
