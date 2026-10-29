#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Soft hair composite (weighted blended OIT): the weighted average colour of every soft hair layer at this pixel,
// over the frame by the product of their transparencies. Blend: dst * reveal + avg * (1 - reveal).
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_accum;    // rgb = sum(c * a * w), a = prod(1 - a)
uniform sampler2D u_weight;   // r = sum(a * w)

void main() {
    vec4 acc = texture(u_accum, v_uv);
    float reveal = acc.a;
    if (reveal > 0.9995) discard;   // no soft hair here
    float wsum = texture(u_weight, v_uv).r;
    vec3 avg = acc.rgb / max(wsum, 1e-5);
    o_color = vec4(max(avg, vec3(0.0)), reveal);
}
