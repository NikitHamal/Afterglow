#version 330
// Afterglow UI: anti-aliased rounded rectangle with optional frosted-glass fill (blurred scene),
// vertical/horizontal gradient, hairline border and soft outer shadow/glow - one quad per shape.
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 u_rect;          // x, y, w, h in framebuffer pixels (top-left origin)
uniform float u_radius;
uniform float u_screenH;      // framebuffer height in pixels
uniform vec4 u_fill;          // straight alpha
uniform vec4 u_fill2;         // gradient end colour
uniform int u_grad;           // 0 none, 1 left->right, 2 top->bottom
uniform int u_glass;          // 1: base = blurred scene from texture0, tinted
uniform vec4 u_glassTint;     // rgb tint, a = tint amount
uniform float u_border;       // px
uniform vec4 u_borderCol;
uniform float u_shadow;       // px (0 = none)
uniform vec4 u_shadowCol;
uniform float u_time;

float sdRound(vec2 p, vec2 c, vec2 hs, float r) {
    vec2 q = abs(p - c) - hs + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }

void main() {
    vec2 p = vec2(gl_FragCoord.x, u_screenH - gl_FragCoord.y);
    vec2 hs = 0.5 * u_rect.zw;
    vec2 c = u_rect.xy + hs;
    float r = min(u_radius, min(hs.x, hs.y));
    float d = sdRound(p, c, hs, r);
    float inside = clamp(0.5 - d, 0.0, 1.0);

    vec4 body = u_fill;
    if (u_grad == 1) body = mix(u_fill, u_fill2, clamp((p.x - u_rect.x) / max(u_rect.z, 1.0), 0.0, 1.0));
    else if (u_grad == 2) body = mix(u_fill, u_fill2, clamp((p.y - u_rect.y) / max(u_rect.w, 1.0), 0.0, 1.0));
    if (u_glass == 1) {
        vec3 scene = texture(texture0, fragTexCoord).rgb;
        // frosted glass diffuses: highlights roll off so a lamp behind a panel is a warm glow, not a white smudge
        float l = dot(scene, vec3(0.2126, 0.7152, 0.0722));
        scene *= (1.0 / (1.0 + 1.6 * l));
        vec3 g = mix(scene, u_glassTint.rgb, u_glassTint.a);
        g += (hash12(p + fract(u_time) * 17.0) - 0.5) * 0.012;   // fine grain keeps the glass from banding
        body = vec4(mix(g, body.rgb, body.a), 1.0);
    }

    vec4 col = vec4(0.0);
    if (u_shadow > 0.0) {
        float a = u_shadowCol.a * (1.0 - smoothstep(-u_shadow * 0.35, u_shadow, d));
        col = vec4(u_shadowCol.rgb * a, a);
    }
    float ba = body.a * inside;
    col = vec4(body.rgb * ba, ba) + col * (1.0 - ba);
    if (u_border > 0.0) {
        float bm = inside * clamp(d + u_border + 0.5, 0.0, 1.0);
        float a = u_borderCol.a * bm;
        col = vec4(u_borderCol.rgb * a, a) + col * (1.0 - a);
    }
    col *= fragColor.a;
    finalColor = vec4(col.rgb / max(col.a, 1e-5), col.a);
}
