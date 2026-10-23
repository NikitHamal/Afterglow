#version 330 core
// Shared vertex shader: static or GPU-skinned meshes (bone matrices in an RGBA32F texture, 4 texels per joint).
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec2 a_uv1;
layout(location = 4) in vec4 a_tan;
layout(location = 5) in vec4 a_joints;
layout(location = 6) in vec4 a_weights;

uniform mat4 u_model;
uniform mat4 u_viewProj;
uniform int u_skinned;
uniform sampler2D u_bones;
uniform int u_skinRow;

out vec3 v_wpos;
out vec3 v_nrm;
out vec4 v_tan;
out vec2 v_uv0;
out vec2 v_uv1;

mat4 boneMat(float j) {
    int x = int(j + 0.5) * 4;
    return mat4(texelFetch(u_bones, ivec2(x, u_skinRow), 0),
                texelFetch(u_bones, ivec2(x + 1, u_skinRow), 0),
                texelFetch(u_bones, ivec2(x + 2, u_skinRow), 0),
                texelFetch(u_bones, ivec2(x + 3, u_skinRow), 0));
}

void main() {
    mat4 M = u_model;
    if (u_skinned == 1) {
        mat4 S = a_weights.x * boneMat(a_joints.x) + a_weights.y * boneMat(a_joints.y) +
                 a_weights.z * boneMat(a_joints.z) + a_weights.w * boneMat(a_joints.w);
        M = u_model * S;
    }
    vec4 wp = M * vec4(a_pos, 1.0);
    mat3 N = mat3(M);
    v_wpos = wp.xyz;
    v_nrm = normalize(N * a_nrm);
    vec3 t = N * a_tan.xyz;
    v_tan = vec4(length(t) > 1e-6 ? normalize(t) : vec3(1.0, 0.0, 0.0), a_tan.w == 0.0 ? 1.0 : a_tan.w);
    v_uv0 = a_uv0;
    v_uv1 = a_uv1;
    gl_Position = u_viewProj * wp;
}
