#version 330 core
// 3x3 tent upsample, additively blended into the next larger level.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_src;
uniform vec2 u_texel;
uniform float u_radius;

void main() {
    vec4 d = u_texel.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * u_radius;
    vec3 s = texture(u_src, v_uv - d.xy).rgb;
    s += texture(u_src, v_uv - d.wy).rgb * 2.0;
    s += texture(u_src, v_uv - d.zy).rgb;
    s += texture(u_src, v_uv + d.zw).rgb * 2.0;
    s += texture(u_src, v_uv).rgb * 4.0;
    s += texture(u_src, v_uv + d.xw).rgb * 2.0;
    s += texture(u_src, v_uv + d.zy).rgb;
    s += texture(u_src, v_uv + d.wy).rgb * 2.0;
    s += texture(u_src, v_uv + d.xy).rgb;
    o_color = vec4(s * (1.0 / 16.0), 1.0);
}
