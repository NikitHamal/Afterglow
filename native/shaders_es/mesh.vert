#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
// Shared vertex shader: static or GPU-skinned meshes. Bones live in an RGBA32F texture, 7 texels per joint: the skin
// matrix (4 columns), its rigid part as a dual quaternion (real, dual) and (uniform scale, linear flag). Skinning blends
// the dual quaternions, so twisting and bending joints keep their volume (no candy-wrapper wrists or collapsing elbows,
// shoulders, knees); a vertex falls back to linear blending only when one of its joints scales non-uniformly.
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec2 a_uv1;
layout(location = 4) in vec4 a_tan;
layout(location = 5) in vec4 a_joints;
layout(location = 6) in vec4 a_weights;
layout(location = 7) in vec4 a_color;    // COLOR_0 (hair shell: hairline fringe in A); unbound = (0, 0, 0, 1)

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
out float v_colA;

const int kTexels = 7;
vec4 boneTexel(float j, int k) { return texelFetch(u_bones, ivec2(int(j + 0.5) * kTexels + k, u_skinRow), 0); }
mat4 boneMat(float j) { return mat4(boneTexel(j, 0), boneTexel(j, 1), boneTexel(j, 2), boneTexel(j, 3)); }

mat3 quatMat(vec4 q) {
    float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    float xx = q.x * x2, yy = q.y * y2, zz = q.z * z2, xy = q.x * y2, xz = q.x * z2, yz = q.y * z2;
    float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;
    return mat3(1.0 - (yy + zz), xy + wz, xz - wy,
                xy - wz, 1.0 - (xx + zz), yz + wx,
                xz + wy, yz - wx, 1.0 - (xx + yy));
}

// skinned position + normal matrix (bind space -> model space)
void skin(out vec3 pos, out mat3 N) {
    vec4 w = a_weights;
    float sw = w.x + w.y + w.z + w.w;
    w /= max(sw, 1e-6);
    vec4 e0 = boneTexel(a_joints.x, 6), e1 = boneTexel(a_joints.y, 6), e2 = boneTexel(a_joints.z, 6), e3 = boneTexel(a_joints.w, 6);
    float linear = max(max(w.x > 0.0 ? e0.y : 0.0, w.y > 0.0 ? e1.y : 0.0), max(w.z > 0.0 ? e2.y : 0.0, w.w > 0.0 ? e3.y : 0.0));
    if (linear > 0.5) {
        mat4 S = w.x * boneMat(a_joints.x) + w.y * boneMat(a_joints.y) + w.z * boneMat(a_joints.z) + w.w * boneMat(a_joints.w);
        pos = (S * vec4(a_pos, 1.0)).xyz;
        N = mat3(S);
        return;
    }
    vec4 r0 = boneTexel(a_joints.x, 4), r1 = boneTexel(a_joints.y, 4), r2 = boneTexel(a_joints.z, 4), r3 = boneTexel(a_joints.w, 4);
    vec4 d0 = boneTexel(a_joints.x, 5), d1 = boneTexel(a_joints.y, 5), d2 = boneTexel(a_joints.z, 5), d3 = boneTexel(a_joints.w, 5);
    // antipodal quaternions: every influence joins the hemisphere of the strongest one
    vec4 ref = r0;
    float wm = w.x;
    if (w.y > wm) { ref = r1; wm = w.y; }
    if (w.z > wm) { ref = r2; wm = w.z; }
    if (w.w > wm) { ref = r3; }
    vec4 s = vec4(dot(r0, ref) < 0.0 ? -w.x : w.x, dot(r1, ref) < 0.0 ? -w.y : w.y,
                  dot(r2, ref) < 0.0 ? -w.z : w.z, dot(r3, ref) < 0.0 ? -w.w : w.w);
    vec4 br = s.x * r0 + s.y * r1 + s.z * r2 + s.w * r3;
    vec4 bd = s.x * d0 + s.y * d1 + s.z * d2 + s.w * d3;
    float len = max(length(br), 1e-6);
    br /= len; bd /= len;
    float sc = w.x * e0.x + w.y * e1.x + w.z * e2.x + w.w * e3.x;
    vec3 p = a_pos * sc;
    pos = p + 2.0 * cross(br.xyz, cross(br.xyz, p) + br.w * p) + 2.0 * (br.w * bd.xyz - bd.w * br.xyz + cross(br.xyz, bd.xyz));
    N = quatMat(br) * sc;
}

void main() {
    mat4 M = u_model;
    vec4 wp;
    mat3 N;
    if (u_skinned == 1) {
        vec3 sp;
        mat3 SN;
        skin(sp, SN);
        wp = u_model * vec4(sp, 1.0);
        N = mat3(u_model) * SN;
    } else {
        wp = M * vec4(a_pos, 1.0);
        N = mat3(M);
    }
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
    v_fit = u_fitMode != 0 ? texelFetch(u_fit, ivec2(gl_VertexID & 1023, gl_VertexID >> 10), 0).xy : vec2(0.0, 1.0);
    // a garment stands off the skin (the fabric's thickness), but its hems lie on it: no hollow at a cuff
    if (u_fitMode == 2) wp.xyz += nrm * u_fitInflate * smoothstep(0.004, 0.03, v_fit.x);
    v_wpos = wp.xyz;
    v_nrm = nrm;
    vec3 t = N * a_tan.xyz;
    v_tan = vec4(length(t) > 1e-6 ? normalize(t) : vec3(1.0, 0.0, 0.0), a_tan.w == 0.0 ? 1.0 : a_tan.w);
    v_uv0 = a_uv0;
    v_uv1 = a_uv1;
    v_opos = a_pos;
    v_colA = a_color.a;
    v_onrm = a_nrm;
    gl_Position = u_viewProj * wp;
}
