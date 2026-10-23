#version 330 core
// Screen-space ambient occlusion (half resolution) from the depth prepass. Normals are reconstructed from
// neighbouring depth samples (smallest-difference rule), hemisphere kernel rotated per pixel.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_depth;
uniform mat4 u_proj;
uniform mat4 u_invProj;
uniform vec2 u_texel;        // full-res depth texel
uniform float u_radius;      // view-space metres
uniform float u_intensity;
uniform int u_samples;

vec3 viewPos(vec2 uv) {
    float d = texture(u_depth, uv).r;
    vec4 p = u_invProj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }

void main() {
    float d0 = texture(u_depth, v_uv).r;
    if (d0 >= 0.99999) { o_color = vec4(1.0); return; }
    vec3 P = viewPos(v_uv);
    vec3 pr = viewPos(v_uv + vec2(u_texel.x * 2.0, 0.0)), pl = viewPos(v_uv - vec2(u_texel.x * 2.0, 0.0));
    vec3 pu = viewPos(v_uv + vec2(0.0, u_texel.y * 2.0)), pd = viewPos(v_uv - vec2(0.0, u_texel.y * 2.0));
    vec3 dx = abs(pr.z - P.z) < abs(P.z - pl.z) ? pr - P : P - pl;
    vec3 dy = abs(pu.z - P.z) < abs(P.z - pd.z) ? pu - P : P - pd;
    vec3 N = normalize(cross(dx, dy));
    if (dot(N, P) > 0.0) N = -N;                       // face the camera
    float a = hash12(gl_FragCoord.xy) * 6.2831853;
    vec3 rnd = vec3(cos(a), sin(a), 0.0);
    vec3 T = normalize(rnd - N * dot(rnd, N));
    vec3 B = cross(N, T);
    float occ = 0.0;
    int n = clamp(u_samples, 4, 32);
    for (int i = 0; i < 32; ++i) {
        if (i >= n) break;
        float fi = float(i) + 0.5;
        float z = sqrt(1.0 - fi / float(n));            // cosine-weighted hemisphere (Fibonacci spiral)
        float r = sqrt(fi / float(n));
        float th = fi * 2.39996323;
        vec3 k = vec3(r * cos(th), r * sin(th), z);
        float scale = mix(0.12, 1.0, (fi / float(n)) * (fi / float(n)));
        vec3 S = P + (T * k.x + B * k.y + N * k.z) * (u_radius * scale);
        vec4 c = u_proj * vec4(S, 1.0);
        vec2 uv = c.xy / c.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) continue;
        float sz = viewPos(uv).z;
        float range = smoothstep(0.0, 1.0, u_radius / max(abs(P.z - sz), 1e-4));
        occ += (sz >= S.z + 0.012 * u_radius ? 1.0 : 0.0) * range;
    }
    float ao = clamp(1.0 - occ / float(n) * u_intensity, 0.0, 1.0);
    o_color = vec4(ao, ao, ao, 1.0);
}
