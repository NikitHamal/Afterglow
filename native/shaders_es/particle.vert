#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Camera-facing particle quads; optional stretch along velocity (droplets/streaks).
layout(location = 0) in vec3 a_center;
layout(location = 1) in vec2 a_corner;
layout(location = 2) in float a_size;
layout(location = 3) in vec4 a_color;
layout(location = 4) in vec3 a_dir;
layout(location = 5) in float a_stretch;
uniform mat4 u_viewProj;
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_camPos;
out vec2 v_corner;
out vec4 v_color;

void main() {
    vec3 r = u_right, u = u_up;
    if (a_stretch > 1.01) {
        vec3 view = normalize(a_center - u_camPos);
        vec3 d = normalize(a_dir - view * dot(a_dir, view));
        u = d * a_stretch;
        r = normalize(cross(d, view));
    }
    vec3 p = a_center + (r * a_corner.x + u * a_corner.y) * a_size;
    v_corner = a_corner;
    v_color = a_color;
    gl_Position = u_viewProj * vec4(p, 1.0);
}
