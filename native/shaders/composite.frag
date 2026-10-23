#version 330 core
// HDR resolve: bloom, exposure, white balance/grade, ACES filmic, contrast, vignette, grain -> sRGB.
// Writes luma to alpha for FXAA.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_hdr;
uniform sampler2D u_bloom;
uniform int u_hasBloom;
uniform float u_bloomStrength;
uniform float u_exposure;
uniform float u_warmth;        // -1 cool .. +1 warm
uniform float u_saturation;
uniform float u_contrast;
uniform float u_vignette;
uniform float u_grain;
uniform float u_time;
uniform vec2 u_resolution;
uniform float u_flash;         // 0..1 climax bloom flash

vec3 acesFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }

void main() {
    vec3 c = texture(u_hdr, v_uv).rgb;
    if (u_hasBloom == 1) c += texture(u_bloom, v_uv).rgb * (u_bloomStrength + u_flash * 0.6);
    c *= u_exposure * (1.0 + u_flash * 0.5);
    c *= vec3(1.0 + 0.07 * u_warmth, 1.0 + 0.01 * u_warmth, 1.0 - 0.09 * u_warmth);
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, u_saturation), vec3(0.0));
    c = acesFilm(c);
    c = clamp((c - 0.5) * u_contrast + 0.5, 0.0, 1.0);
    vec2 q = v_uv - 0.5;
    q.x *= u_resolution.x / u_resolution.y;
    float v = smoothstep(0.95, 0.25, length(q) * (0.9 + u_vignette * 0.6));
    c *= mix(1.0, v, u_vignette);
    c = pow(c, vec3(1.0 / 2.2));
    float n = hash12(gl_FragCoord.xy + fract(u_time * 7.13) * 917.0) - 0.5;
    c += n * u_grain * (1.0 - c * 0.6);
    o_color = vec4(clamp(c, 0.0, 1.0), dot(clamp(c, 0.0, 1.0), vec3(0.299, 0.587, 0.114)));
}
