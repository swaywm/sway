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
"uniform vec2 position;\n"
"uniform float radius;\n"
"uniform vec2 half_size;\n"
"uniform float half_thickness;\n"
"\n"
"float roundedBoxSDF(vec2 center, vec2 size, float radius) {\n"
"	return length(max(abs(center) - size + radius, 0.0)) - radius;\n"
"}\n"
"\n"
"void main() {\n"
"	gl_FragColor = v_color;\n"
"	vec2 center = gl_FragCoord.xy - position - half_size;\n"
"	float distance = roundedBoxSDF(center, half_size - half_thickness, radius + half_thickness);\n"
"	float smoothedAlphaOuter = 1.0 - smoothstep(-1.0, 1.0, distance - half_thickness);\n"
// Create an inner circle that isn't as anti-aliased as the outer ring
"	float smoothedAlphaInner = 1.0 - smoothstep(-1.0, 0.5, distance + half_thickness);\n"
"	gl_FragColor = mix(vec4(0), gl_FragColor, smoothedAlphaOuter - smoothedAlphaInner);\n"
"\n"
"	if (is_top_left && (center.y > 0.0 || center.x > 0.0)) {\n"
"		discard;\n"
"	}\n"
"	else if (is_top_right && (center.y > 0.0 || center.x < 0.0)) {\n"
"		discard;\n"
"	}\n"
"	else if (is_bottom_left && (center.y < 0.0 || center.x > 0.0)) {\n"
"		discard;\n"
"	}\n"
"	else if (is_bottom_right && (center.y < 0.0 || center.x < 0.0)) {\n"
"		discard;\n"
"	}\n"
"}\n";

#endif
