precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

uniform vec2 size;
uniform vec2 position;
uniform float radius;
uniform bool has_titlebar;
uniform float saturation;
const vec3 saturation_weight = vec3(0.2125, 0.7154, 0.0721);

void main() {
    // Saturation
    if (saturation != 1.0) {
        vec3 irgb = texture2D(tex, v_texcoord).rgb;
        vec3 target = vec3(dot(irgb, saturation_weight));
        gl_FragColor = vec4(mix(target, irgb, saturation), 1.0) * alpha;
    } else {
        gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;
    }

    if (!has_titlebar || gl_FragCoord.y - position.y > radius) {
        vec2 corner_distance = min(gl_FragCoord.xy - position, size + position - gl_FragCoord.xy);
        if (max(corner_distance.x, corner_distance.y) < radius) {
            float d = radius - distance(corner_distance, vec2(radius));
            float smooth = smoothstep(-1.0f, 0.5f, d);
            gl_FragColor = mix(vec4(0), gl_FragColor, smooth);
        }
    }
}
