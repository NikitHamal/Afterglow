#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Bright-pass with soft knee into the first (half resolution) bloom level.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_src;
uniform vec2 u_texel;       // source texel size
uniform float u_threshold;
uniform float u_knee;

vec3 sampleBox(vec2 uv) {
    vec4 o = u_texel.xyxy * vec4(-1.0, -1.0, 1.0, 1.0);
    return 0.25 * (texture(u_src, uv + o.xy).rgb + texture(u_src, uv + o.zy).rgb + texture(u_src, uv + o.xw).rgb + texture(u_src, uv + o.zw).rgb);
}

void main() {
    vec3 c = sampleBox(v_uv);
    c = min(c, vec3(64.0));                  // fireflies
    float br = max(c.r, max(c.g, c.b));
    float rq = clamp(br - u_threshold + u_knee, 0.0, 2.0 * u_knee);
    rq = rq * rq / (4.0 * u_knee + 1e-5);
    float contrib = max(rq, br - u_threshold) / max(br, 1e-5);
    o_color = vec4(c * contrib, 1.0);
}
