#version 330 core
// FXAA (Lottes 3.11 "quality" style, compact). Input alpha holds luma from the composite pass.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_src;
uniform vec2 u_texel;
uniform int u_enabled;

float luma(vec2 uv) { return texture(u_src, uv).a; }

void main() {
    vec4 rgbM = texture(u_src, v_uv);
    if (u_enabled == 0) { o_color = vec4(rgbM.rgb, 1.0); return; }
    float lM = rgbM.a;
    float lN = luma(v_uv + vec2(0.0, u_texel.y));
    float lS = luma(v_uv - vec2(0.0, u_texel.y));
    float lE = luma(v_uv + vec2(u_texel.x, 0.0));
    float lW = luma(v_uv - vec2(u_texel.x, 0.0));
    float lMin = min(lM, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lM, max(max(lN, lS), max(lE, lW)));
    float range = lMax - lMin;
    if (range < max(0.0312, lMax * 0.125)) { o_color = vec4(rgbM.rgb, 1.0); return; }
    float lNW = luma(v_uv + vec2(-u_texel.x, u_texel.y));
    float lNE = luma(v_uv + u_texel);
    float lSW = luma(v_uv - u_texel);
    float lSE = luma(v_uv + vec2(u_texel.x, -u_texel.y));
    float edgeH = abs(lNW + lNE - 2.0 * lN) + 2.0 * abs(lW + lE - 2.0 * lM) + abs(lSW + lSE - 2.0 * lS);
    float edgeV = abs(lNW + lSW - 2.0 * lW) + 2.0 * abs(lN + lS - 2.0 * lM) + abs(lNE + lSE - 2.0 * lE);
    bool horz = edgeH >= edgeV;
    float l1 = horz ? lS : lW, l2 = horz ? lN : lE;
    float g1 = abs(l1 - lM), g2 = abs(l2 - lM);
    float stepLen = horz ? u_texel.y : u_texel.x;
    float lLocal;
    float grad;
    if (g1 >= g2) { stepLen = -stepLen; lLocal = 0.5 * (l1 + lM); grad = g1; }
    else { lLocal = 0.5 * (l2 + lM); grad = g2; }
    vec2 uv = v_uv;
    if (horz) uv.y += stepLen * 0.5; else uv.x += stepLen * 0.5;
    vec2 off = horz ? vec2(u_texel.x, 0.0) : vec2(0.0, u_texel.y);
    vec2 uv1 = uv - off, uv2 = uv + off;
    float e1 = luma(uv1) - lLocal, e2 = luma(uv2) - lLocal;
    bool r1 = abs(e1) >= grad * 0.25, r2 = abs(e2) >= grad * 0.25;
    const float Q[10] = float[](1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
    for (int i = 0; i < 10 && !(r1 && r2); ++i) {
        if (!r1) { uv1 -= off * Q[i]; e1 = luma(uv1) - lLocal; r1 = abs(e1) >= grad * 0.25; }
        if (!r2) { uv2 += off * Q[i]; e2 = luma(uv2) - lLocal; r2 = abs(e2) >= grad * 0.25; }
    }
    float d1 = horz ? (v_uv.x - uv1.x) : (v_uv.y - uv1.y);
    float d2 = horz ? (uv2.x - v_uv.x) : (uv2.y - v_uv.y);
    bool dir1 = d1 < d2;
    float dmin = min(d1, d2);
    float span = d1 + d2;
    bool lMSmaller = lM < lLocal;
    bool good = ((dir1 ? e1 : e2) < 0.0) != lMSmaller;
    float px = good ? (-dmin / span + 0.5) : 0.0;
    float lAvg = (1.0 / 12.0) * (2.0 * (lN + lS + lE + lW) + lNW + lNE + lSW + lSE);
    float sub = clamp(abs(lAvg - lM) / range, 0.0, 1.0);
    float subPx = (-2.0 * sub + 3.0) * sub * sub;
    px = max(px, subPx * subPx * 0.75);
    vec2 fuv = v_uv;
    if (horz) fuv.y += px * stepLen; else fuv.x += px * stepLen;
    o_color = vec4(texture(u_src, fuv).rgb, 1.0);
}
