#version 330 core
// Shadow-map / depth pass: depth only, alpha-tested materials respect their cutoff, garments being taken off
// follow the same undress wipe as the colour pass.
in vec2 v_uv0;
in vec3 v_opos;
in float v_colA;                 // vertex colour alpha (a hair shell's hairline fringe)
uniform sampler2D u_texBase;
uniform int u_hasBase;
uniform int u_alphaMode;
uniform float u_cutoff;
uniform vec4 u_baseColor;
uniform int u_hairCards;          // hair cards: the long sides fade like the colour pass (no depth where no colour)
uniform float u_dissolve;        // 1 worn .. 0 taken off
uniform vec4 u_wipe;             // x,y bind-space height span, z 1 = comes off from the top

float h3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y);
    float b = mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y);
    return mix(a, b, f.z);
}
// garment fit: skin under a worn garment is never drawn (x: the outer layer is worn, y: the under layer is);
// a garment trims along its smoothed border (x: trim in m) and hides under the worn outer layer (y)
in vec2 v_fit;
uniform int u_fitMode;
uniform vec2 u_fitHide;
bool fitHidden() {
    if (u_fitMode == 1) return v_fit.x * u_fitHide.x > 0.5 || v_fit.y * u_fitHide.y > 0.5;
    if (u_fitMode == 2) return v_fit.x < u_fitHide.x || v_fit.y * u_fitHide.y > 0.5;
    return false;
}
float undressMargin(vec3 op) {
    if (u_dissolve >= 0.999) return 1.0;
    float h = clamp((op.y - u_wipe.x) / max(u_wipe.y - u_wipe.x, 1e-3), 0.0, 1.0);
    float s = u_wipe.z > 0.5 ? 1.0 - h : h;
    float n = vnoise3(op * 38.0) * 0.6 + vnoise3(op * 110.0) * 0.4;
    float e = 0.16;
    return s - ((1.0 - u_dissolve) * (1.0 + e) - e * n);
}

void main() {
    if (fitHidden() || undressMargin(v_opos) < 0.0) discard;
    if (u_alphaMode == 1) {
        float a = u_baseColor.a * (u_hasBase == 1 ? texture(u_texBase, v_uv0).a : 1.0) * v_colA;
        if (u_hairCards == 1) {
            float xc = fract(v_uv0.x * 8.0);
            a *= smoothstep(0.0, 0.2, clamp(min(xc - 0.03, 0.97 - xc) / 0.94, 0.0, 0.5));
        }
        if (a < u_cutoff) discard;
    }
}
