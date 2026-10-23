#version 330 core
// Shadow-map pass: depth only, alpha-tested materials respect their cutoff.
in vec2 v_uv0;
uniform sampler2D u_texBase;
uniform int u_hasBase;
uniform int u_alphaMode;
uniform float u_cutoff;
uniform vec4 u_baseColor;

void main() {
    if (u_alphaMode == 1) {
        float a = u_baseColor.a * (u_hasBase == 1 ? texture(u_texBase, v_uv0).a : 1.0);
        if (a < u_cutoff) discard;
    }
}
