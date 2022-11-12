precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 size;
uniform vec2 position;
uniform float radius;

void main() {
    vec2 half_size = size / 2.0;
    vec2 q = abs(gl_FragCoord.xy - position - half_size) - half_size + radius;
    float distance = min(max(q.x,q.y),0.0) + length(max(q,0.0)) - radius;
    float smoothedAlpha = 1.0 - smoothstep(-1.0, 1.0, distance);
    gl_FragColor = mix(vec4(0), v_color, smoothedAlpha);
}
