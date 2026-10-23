#version 330 core
// Depth of field for directed shots: blends the sharp frame with a blurred copy by the circle of confusion
// (thin-lens shaped: grows with the distance from the focus plane, relative to depth). Luma in alpha for FXAA.
in vec2 v_uv;
out vec4 o_color;
uniform sampler2D u_sharp;
uniform sampler2D u_blur;
uniform sampler2D u_depth;
uniform float u_near;
uniform float u_far;
uniform float u_focus;      // metres
uniform float u_amount;     // 0 sharp .. ~1.5 very shallow

float linearDepth(float d) { float z = d * 2.0 - 1.0; return 2.0 * u_near * u_far / (u_far + u_near - z * (u_far - u_near)); }

void main() {
    vec3 s = texture(u_sharp, v_uv).rgb;
    vec3 b = texture(u_blur, v_uv).rgb;
    float z = linearDepth(texture(u_depth, v_uv).r);
    float coc = clamp(abs(z - u_focus) / max(z, 0.05) * u_amount * 2.4, 0.0, 1.0);
    vec3 c = mix(s, b, smoothstep(0.08, 0.75, coc));
    o_color = vec4(c, dot(c, vec3(0.299, 0.587, 0.114)));
}
