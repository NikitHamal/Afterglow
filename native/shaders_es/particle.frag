#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Soft round sprite. Additive particles output premultiplied energy; alpha particles blend normally.
in vec2 v_corner;
in vec4 v_color;
out vec4 o_color;
uniform int u_additive;

void main() {
    float r = length(v_corner);
    if (r > 1.0) discard;
    float a = (1.0 - r * r);
    a *= a;
    if (u_additive == 1) o_color = vec4(v_color.rgb * v_color.a * a, 1.0);
    else o_color = vec4(v_color.rgb, v_color.a * a);
}
