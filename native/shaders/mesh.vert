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
uniform int u_numContacts;        // soft tissue: finger/palm spheres dent the skin
uniform vec4 u_contacts[8];       // xyz centre (world), w radius
uniform vec4 u_noDent;            // skin joint ranges [x..y], [z..w] that never dent (the touching hands)
uniform int u_fitMode;            // garment fit: 0 none, 1 skin under a worn garment, 2 garment
uniform sampler2D u_fit;          // per vertex (gl_VertexID): skin: cover by outer, by under; garment: border (m), cover
uniform float u_fitInflate;       // garments: clearance off the skin along the normal (m)

out vec3 v_wpos;
out vec3 v_nrm;
out vec4 v_tan;
out vec2 v_uv0;
out vec2 v_uv1;
out vec3 v_opos;                  // bind-space position / normal (garment patterns, undress wipe)
out vec3 v_onrm;
out vec2 v_fit;

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
    vec3 nrm = normalize(N * a_nrm);
    if (u_skinned == 1 && u_numContacts > 0) {
        float hand = 0.0;
        for (int k = 0; k < 4; ++k) {
            float j = a_joints[k];
            if ((j > u_noDent.x - 0.5 && j < u_noDent.y + 0.5) || (j > u_noDent.z - 0.5 && j < u_noDent.w + 0.5)) hand += a_weights[k];
        }
        float soft = 1.0 - clamp(hand * 1.5, 0.0, 1.0);
        for (int i = 0; i < 8; ++i) {
            if (i >= u_numContacts || soft <= 0.0) break;
            vec3 d = wp.xyz - u_contacts[i].xyz;
            float l = length(d), r = u_contacts[i].w;
            if (l < r && l > 1e-5) {
                float push = (r - l) * soft;
                wp.xyz += d / l * push;
                nrm = normalize(nrm + d / l * (push / r) * 0.6);
            }
        }
    }
    if (u_fitMode == 2) wp.xyz += nrm * u_fitInflate;
    v_wpos = wp.xyz;
    v_nrm = nrm;
    vec3 t = N * a_tan.xyz;
    v_tan = vec4(length(t) > 1e-6 ? normalize(t) : vec3(1.0, 0.0, 0.0), a_tan.w == 0.0 ? 1.0 : a_tan.w);
    v_uv0 = a_uv0;
    v_uv1 = a_uv1;
    v_opos = a_pos;
    v_onrm = a_nrm;
    v_fit = u_fitMode != 0 ? texelFetch(u_fit, ivec2(gl_VertexID & 1023, gl_VertexID >> 10), 0).xy : vec2(0.0, 1.0);
    gl_Position = u_viewProj * wp;
}
