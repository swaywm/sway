#ifndef _SWAY_SHADERS_H
#define _SWAY_SHADERS_H

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
"		float smooth = smoothstep(-1.0f, 1.0f, d);\n"
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
"		float smooth = smoothstep(-1.0f, 1.0f, d);\n"
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
"		float smooth = smoothstep(-1.0f, 1.0f, d);\n"
"		gl_FragColor = mix(vec4(0), gl_FragColor, smooth);\n"
"   }\n"
"}\n";

const GLchar frag_blur_1[] =
"precision mediump float;\n"
"varying mediump vec2 v_texcoord; // is in 0-1\n"
"uniform sampler2D tex;\n"
"\n"
"uniform float radius;\n"
"uniform vec2 halfpixel;\n"
"\n"
"void main() {\n"
"	vec2 uv = v_texcoord * 2.0;\n"
"\n"
"    vec4 sum = texture2D(tex, uv) * 4.0;\n"
"    sum += texture2D(tex, uv - halfpixel.xy * radius);\n"
"    sum += texture2D(tex, uv + halfpixel.xy * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius);\n"
"    sum += texture2D(tex, uv - vec2(halfpixel.x, -halfpixel.y) * radius);\n"
"    gl_FragColor = sum / 8.0;\n"
"}\n";

const GLchar frag_blur_2[] =
"precision mediump float;\n"
"varying mediump vec2 v_texcoord; // is in 0-1\n"
"uniform sampler2D tex;\n"
"\n"
"uniform float radius;\n"
"uniform vec2 halfpixel;\n"
"\n"
"void main() {\n"
"	vec2 uv = v_texcoord / 2.0;\n"
"\n"
"    vec4 sum = texture2D(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);\n"
"\n"
"    sum += texture2D(tex, uv + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(0.0, halfpixel.y * 2.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(0.0, -halfpixel.y * 2.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;\n"
"\n"
"    gl_FragColor = sum / 12.0;\n"
"}\n";

#endif
