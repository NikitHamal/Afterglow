#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Depth of field for directed shots: a scatter-as-gather bokeh on the sharp frame (golden-angle spiral, one pass).
// A sample only lands on this pixel when its own circle of confusion reaches it, so an in-focus actor never bleeds
// into the blurred room behind (no halo), and the blurred room is clamped where it lies behind a sharp edge.
// Luma in alpha for FXAA.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_sharp;
uniform sampler2D u_depth;
uniform float u_near;
uniform float u_far;
uniform float u_focus;      // metres
uniform float u_amount;     // 0 sharp .. ~1.5 very shallow
uniform vec2 u_texel;       // 1 / frame size
uniform float u_maxPx;      // largest blur radius in pixels (scales with the frame height)

const int TAPS = 40;
const float GOLDEN = 2.39996323;

float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
float linearDepth(float d) { float z = d * 2.0 - 1.0; return 2.0 * u_near * u_far / (u_far + u_near - z * (u_far - u_near)); }
// circle of confusion radius in pixels (thin-lens shaped: grows with the distance from the focus plane, relative to depth)
float cocPx(float z) { return clamp(abs(z - u_focus) / max(z, 0.05) * u_amount * 2.4, 0.0, 1.0) * u_maxPx; }

void main() {
    vec3 c0 = texture(u_sharp, v_uv).rgb;
    float z0 = linearDepth(texture(u_depth, v_uv).r);
    float r0 = cocPx(z0);
    vec3 acc = c0;
    float tot = 1.0;
    float ring = u_maxPx / sqrt(float(TAPS));   // spacing between successive tap radii
    float spin = ign(gl_FragCoord.xy) * 6.2831853;   // per-pixel rotation: sparse taps read as fine grain, not ghosts
    for (int i = 0; i < TAPS; ++i) {
        float r = u_maxPx * sqrt((float(i) + 0.5) / float(TAPS));
        float a = float(i) * GOLDEN + spin;
        vec2 uv = v_uv + vec2(cos(a), sin(a)) * r * u_texel;
        vec3 cs = texture(u_sharp, uv).rgb;
        float zs = linearDepth(texture(u_depth, uv).r);
        float rs = cocPx(zs);
        // the blurred background behind a sharp pixel may only spread as far as this pixel's own blur allows
        if (zs > z0) rs = min(rs, r0 * 2.0);
        float m = smoothstep(r - ring * 0.5, r + ring * 0.5, rs);
        acc += mix(acc / tot, cs, m);
        tot += 1.0;
    }
    vec3 c = acc / tot;
    o_color = vec4(c, dot(c, vec3(0.299, 0.587, 0.114)));
}
