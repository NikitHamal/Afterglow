#version 330 core
// Depth-aware 4x4 blur of the half-resolution AO (keeps silhouettes crisp).
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_ao;
uniform sampler2D u_depth;
uniform vec2 u_texel;        // AO texel
uniform vec2 u_near_far;     // camera near / far

float linZ(float d) { float n = u_near_far.x, f = u_near_far.y; return 2.0 * n * f / (f + n - (d * 2.0 - 1.0) * (f - n)); }

void main() {
    float z0 = linZ(texture(u_depth, v_uv).r);
    float sum = 0.0, wsum = 0.0;
    for (int y = -2; y < 2; ++y)
        for (int x = -2; x < 2; ++x) {
            vec2 uv = v_uv + (vec2(x, y) + 0.5) * u_texel;
            float z = linZ(texture(u_depth, uv).r);
            float w = 1.0 / (1e-3 + abs(z - z0) * 18.0);
            sum += texture(u_ao, uv).r * w;
            wsum += w;
        }
    float ao = sum / max(wsum, 1e-5);
    o_color = vec4(ao, ao, ao, 1.0);
}
