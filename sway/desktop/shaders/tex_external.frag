#extension GL_OES_EGL_image_external : require

precision mediump float;
varying vec2 v_texcoord;
uniform samplerExternalOES texture0;
uniform float alpha;

uniform vec2 size;
uniform vec2 position;
uniform float radius;

void main() {
    gl_FragColor = texture2D(texture0, v_texcoord) * alpha;
   vec2 corner_distance = min(gl_FragCoord.xy - position, position + size - gl_FragCoord.xy);
   if (max(corner_distance.x, corner_distance.y) < radius) {
        float d = radius - distance(corner_distance, vec2(radius));
        float smooth = smoothstep(-1.0f, 0.5f, d);
        gl_FragColor = mix(vec4(0), gl_FragColor, smooth);
   }
}
