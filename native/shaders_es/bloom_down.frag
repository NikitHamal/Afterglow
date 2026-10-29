#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// 13-tap downsample (Jimenez, "Next Generation Post Processing in Call of Duty").
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_src;
uniform vec2 u_texel;

void main() {
    vec2 t = u_texel;
    vec3 a = texture(u_src, v_uv + t * vec2(-2.0, 2.0)).rgb;
    vec3 b = texture(u_src, v_uv + t * vec2(0.0, 2.0)).rgb;
    vec3 c = texture(u_src, v_uv + t * vec2(2.0, 2.0)).rgb;
    vec3 d = texture(u_src, v_uv + t * vec2(-2.0, 0.0)).rgb;
    vec3 e = texture(u_src, v_uv).rgb;
    vec3 f = texture(u_src, v_uv + t * vec2(2.0, 0.0)).rgb;
    vec3 g = texture(u_src, v_uv + t * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(u_src, v_uv + t * vec2(0.0, -2.0)).rgb;
    vec3 i = texture(u_src, v_uv + t * vec2(2.0, -2.0)).rgb;
    vec3 j = texture(u_src, v_uv + t * vec2(-1.0, 1.0)).rgb;
    vec3 k = texture(u_src, v_uv + t * vec2(1.0, 1.0)).rgb;
    vec3 l = texture(u_src, v_uv + t * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(u_src, v_uv + t * vec2(1.0, -1.0)).rgb;
    vec3 o = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    o_color = vec4(o, 1.0);
}
