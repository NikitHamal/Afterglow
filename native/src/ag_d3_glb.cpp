// Afterglow native — ag_d3_glb.cpp
// d3::Model port of js3d/glbModel.js (cgltf load + CPU skinning, GLES2 draw).
//
// INTEGRATION NOTE: ag_3d_todo.cpp still carries stub Model::load/unload/
// pose/draw bodies. Adding this TU to the build requires DELETING those four
// stubs (else duplicate symbols) and listing this file in CMakeLists.txt
// under afterglow_core (cgltf.h include path is already configured there).
// d3gl:: entry points are DEFINED by the platform layer (B5 loader); this
// file only calls through them after null-checking, so headless builds
// (no GL context) run pose()/load() CPU-side safely and draw() no-ops.
//
// COVERED (js3d/glbModel.js -> here):
//   parse .glb/.gltf via cgltf (GLB container + external/embedded .bin and
//     base64 buffers via cgltf_load_buffers) | TRIANGLES + TRIANGLE_STRIP/FAN
//     | indexed + non-indexed (sequential) | interleaved stride + sparse
//     accessors (via cgltf_accessor_read_*) | NORMAL gen when missing
//     (area-weighted) | planar UV fallback (glbEnsureUV) | COLOR_0 NaN/Inf
//     drop (glbDropBadColor) + valid COLOR_0 mean folded into factor for
//     untextured mats | auto-fit scale (fit 1.35/maxDim, yOff 0.62,
//     glbPlaceEntry) | pelvis anchor (MMD/hips/pelvis/ASCII fallback, else
//     52% box rule) | per-part drive gains (GLB_PARTS) + damped copy
//     (GLB_DRIVE_RATE 18) | side-swap arm/leg map (GLB_RIG_MAP, all 22 rows:
//     spine/head/arms/hands/legs incl. ALT+twist aux bones/feet/toes/breast)
//     | thigh ABSOLUTE + mirrored spread, knee hinge, ankle affine
//     (ankle0 -1.6, ankleK 0.85, dangleK 0.7), toe *0.5, wrist whisper 0.12,
//     breast locked | finger curl/fan chains with decay (GLB_HAND3 synth)
//     | thrust jiggle spring (k 130, damp 10, force -depthDelta*1.8,
//     updateGLBModel) + calm/activity gate + frozen intro | breathing scale,
//     tail sway, ear flicks (glbSecondary3) | oral wobble (glbFollowHer3) |
//     bed-lift clamp (glbCollideBed, sunk-only) | base-color texture object
//     (TexImage2D RGBA8, LINEAR, CLAMP, UNPACK_ALIGNMENT 1) else flat factor
// SKIPPED (needs scene-graph/decoder/platform pieces not yet ported):
//   PNG/JPEG texture pixels (no stb_image in vendor yet; decoder hook +
//     MIME detection wired, BMP32/TGA-raw decode; falls back to factor) |
//   KHR_draco_mesh_compression (no decoder) | morph targets, skinning with
//   >4 joints/vtx (first 4 renormalized), POINTS/LINES | glTF animations
//   (JS drives procedurally, not from clips — same here) | texture sanitize/
//   canvas-rebake, vertexColors toggle, roughness/metalness clamp (PBR-lite
//   lambert stands in for three r150) | couple-fit him offset, char lights,
//   shadows, per-part visibility/tint UI, dev URL flags | Kiyoko/anime stay
//   root-follow automatically (no skeleton -> null rig, same as JS).
// GAME->BONE BRIDGE: native has no her3 rig yet (chars3d/anim3d pending), so
// the her3-side source eulers of glbDriveRig are synthesized from Game state
// (pose index tables + depth/pleasure/rub/oral/climax rhythms, same special
// cases). When the procedural rig lands, replace synthSrc() only — the drive,
// skinning and draw paths are already rig-agnostic.
#include "ag_3d.h"
#include "ag_d3_gl.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace ag::d3 {
namespace {

// ---- tiny math (float; column-major Mat4 for GL) ----
inline float f(double v) { return (float)v; }
struct V3 {
  float x = 0, y = 0, z = 0;
};
struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
};
struct M4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
inline M4 mulM(const M4& a, const M4& b) {
  M4 o{};
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++)
      o.m[c * 4 + r] =
          a.m[r] * b.m[c * 4] + a.m[4 + r] * b.m[c * 4 + 1] +
          a.m[8 + r] * b.m[c * 4 + 2] + a.m[12 + r] * b.m[c * 4 + 3];
  return o;
}
// three.js 'XYZ' intrinsic euler -> quat (matches dst.rotation order).
inline Quat eulerQ(float x, float y, float z) {
  float cx = cosf(x * 0.5f), sx = sinf(x * 0.5f), cy = cosf(y * 0.5f),
        sy = sinf(y * 0.5f), cz = cosf(z * 0.5f), sz = sinf(z * 0.5f);
  Quat q;
  q.w = cx * cy * cz - sx * sy * sz;
  q.x = sx * cy * cz + cx * sy * sz;
  q.y = cx * sy * cz - sx * cy * sz;
  q.z = cx * cy * sz + sx * sy * cz;
  return q;
}
inline Quat mulQ(const Quat& a, const Quat& b) {
  Quat o;
  o.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
  o.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
  o.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
  o.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
  return o;
}
// bind quat -> XYZ euler (for additive drive targets).
inline void quatE(const Quat& q, float& x, float& y, float& z) {
  float sp = 2 * (q.w * q.y - q.z * q.x);
  if (sp > 1)
    sp = 1;
  if (sp < -1)
    sp = -1;
  x = atan2f(2 * (q.w * q.x + q.y * q.z), 1 - 2 * (q.x * q.x + q.y * q.y));
  y = asinf(sp);
  z = atan2f(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z));
}
inline M4 composeM(const float t[3], const Quat& q, const float s[3]) {
  float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y,
        xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y,
        wz = q.w * q.z;
  M4 o{};
  o.m[0] = (1 - 2 * (yy + zz)) * s[0];
  o.m[1] = (2 * (xy + wz)) * s[0];
  o.m[2] = (2 * (xz - wy)) * s[0];
  o.m[4] = (2 * (xy - wz)) * s[1];
  o.m[5] = (1 - 2 * (xx + zz)) * s[1];
  o.m[6] = (2 * (yz + wx)) * s[1];
  o.m[8] = (2 * (xz + wy)) * s[2];
  o.m[9] = (2 * (yz - wx)) * s[2];
  o.m[10] = (1 - 2 * (xx + yy)) * s[2];
  o.m[12] = t[0];
  o.m[13] = t[1];
  o.m[14] = t[2];
  o.m[15] = 1;
  return o;
}
inline V3 xformP(const M4& m, float x, float y, float z) {
  V3 o;
  o.x = m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12];
  o.y = m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13];
  o.z = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14];
  return o;
}
inline V3 xformV(const M4& m, float x, float y, float z) {
  V3 o;
  o.x = m.m[0] * x + m.m[4] * y + m.m[8] * z;
  o.y = m.m[1] * x + m.m[5] * y + m.m[9] * z;
  o.z = m.m[2] * x + m.m[6] * y + m.m[10] * z;
  return o;
}
inline M4 perspM(float fovY, float aspect, float zn, float zf) {
  float t = tanf(fovY * 0.5f);
  M4 o{};
  o.m[0] = 1 / (aspect * t);
  o.m[5] = 1 / t;
  o.m[10] = -(zf + zn) / (zf - zn);
  o.m[11] = -1;
  o.m[14] = -2 * zf * zn / (zf - zn);
  return o;
}
inline M4 lookM(const V3& eye, const V3& at) {
  V3 z{eye.x - at.x, eye.y - at.y, eye.z - at.z};
  float l = sqrtf(z.x * z.x + z.y * z.y + z.z * z.z);
  if (l < 1e-6f)
    l = 1;
  z.x /= l;
  z.y /= l;
  z.z /= l;
  V3 x{z.z, 0, -z.x};
  float xl = sqrtf(x.x * x.x + x.z * x.z);
  if (xl < 1e-6f) {
    x = V3{1, 0, 0};
    xl = 1;
  }
  x.x /= xl;
  x.z /= xl;
  V3 y{z.y * x.z - z.z * 0, z.z * x.x - z.x * x.z, z.x * 0 - z.y * x.x};
  M4 o{};
  o.m[0] = x.x;
  o.m[1] = y.x;
  o.m[2] = z.x;
  o.m[4] = 0;
  o.m[5] = y.y;
  o.m[6] = z.y;
  o.m[8] = x.z;
  o.m[9] = y.z;
  o.m[10] = z.z;
  o.m[12] = -(x.x * eye.x + x.z * eye.z);
  o.m[13] = -(y.x * eye.x + y.y * eye.y + y.z * eye.z);
  o.m[14] = -(z.x * eye.x + z.y * eye.y + z.z * eye.z);
  o.m[15] = 1;
  return o;
}

// ---- d3gl readiness (headless-safe: any missing pointer -> GL no-op) ----
bool glReady() {
  using namespace d3gl;
  return Viewport && Clear && Enable && DepthFunc && CullFace &&
         CreateShader && ShaderSource && CompileShader && GetShaderiv &&
         CreateProgram && AttachShader && LinkProgram && GetProgramiv &&
         UseProgram && GetAttribLocation && GetUniformLocation &&
         EnableVertexAttribArray && VertexAttribPointer &&
         DisableVertexAttribArray && UniformMatrix4fv && Uniform4fv &&
         Uniform1i && GenBuffers && BindBuffer && BufferData &&
         BufferSubData && DeleteBuffers && GenTextures && BindTexture &&
         TexImage2D && TexParameteri && DeleteTextures && ActiveTexture &&
         DrawElements && PixelStorei;
}

// ---- file-local GL program (PBR-lite lambert; engine owns frame/clear) ----
struct Prog {
  d3gl::GLuint id = 0;
  d3gl::GLint aPos = -1, aNor = -1, aUV = -1;
  d3gl::GLint uMVP = -1, uModel = -1, uColor = -1, uHasTex = -1, uTex = -1;
  bool ok = false, tried = false;
};
Prog& prog() {
  static Prog p;
  return p;
}
d3gl::GLuint compileOne(d3gl::GLenum type, const char* src) {
  using namespace d3gl;
  d3gl::GLuint s = CreateShader(type);
  if (!s)
    return 0;
  ShaderSource(s, 1, &src, nullptr);
  CompileShader(s);
  d3gl::GLint st = 0;
  GetShaderiv(s, COMPILE_STATUS, &st);
  if (!st) {
    DeleteShader(s);
    return 0;
  }
  return s;
}
void ensureProg() {
  using namespace d3gl;
  Prog& p = prog();
  if (p.tried || !glReady())
    return;
  p.tried = true;
  static const char* kVS =
      "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
      "attribute vec3 aPos; attribute vec3 aNor; attribute vec2 aUV;\n"
      "uniform mat4 uMVP; uniform mat4 uModel;\n"
      "varying vec3 vN; varying vec2 vUV;\n"
      "void main(){ vN = normalize((uModel * vec4(aNor, 0.0)).xyz);\n"
      " vUV = aUV; gl_Position = uMVP * vec4(aPos, 1.0); }\n";
  static const char* kFS =
      "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
      "uniform vec4 uColor; uniform int uHasTex; uniform sampler2D uTex;\n"
      "varying vec3 vN; varying vec2 vUV;\n"
      "void main(){ vec3 base = uColor.rgb;\n"
      " if (uHasTex == 1) { base *= texture2D(uTex, vUV).rgb; }\n"
      " float d = max(dot(normalize(vN), normalize(vec3(0.5, 0.8, 0.6))), "
      "0.0);\n"
      " gl_FragColor = vec4(base * (0.45 + 0.55 * d), uColor.a); }\n";
  d3gl::GLuint vs = compileOne(VERTEX_SHADER, kVS);
  d3gl::GLuint fs = compileOne(FRAGMENT_SHADER, kFS);
  if (!vs || !fs)
    return;
  p.id = CreateProgram();
  if (!p.id)
    return;
  AttachShader(p.id, vs);
  AttachShader(p.id, fs);
  LinkProgram(p.id);
  DeleteShader(vs);
  DeleteShader(fs);
  d3gl::GLint st = 0;
  GetProgramiv(p.id, LINK_STATUS, &st);
  if (!st) {
    DeleteProgram(p.id);
    p.id = 0;
    return;
  }
  p.aPos = GetAttribLocation(p.id, "aPos");
  p.aNor = GetAttribLocation(p.id, "aNor");
  p.aUV = GetAttribLocation(p.id, "aUV");
  p.uMVP = GetUniformLocation(p.id, "uMVP");
  p.uModel = GetUniformLocation(p.id, "uModel");
  p.uColor = GetUniformLocation(p.id, "uColor");
  p.uHasTex = GetUniformLocation(p.id, "uHasTex");
  p.uTex = GetUniformLocation(p.id, "uTex");
  p.ok = p.aPos >= 0 && p.uMVP >= 0;
  if (p.ok && p.uTex >= 0) {
    UseProgram(p.id);
    Uniform1i(p.uTex, 0);
  }
}

// ---- names: strip '.'/'_' (UTF-8 safe: both are single-byte ASCII) ----
std::string cleanKey(const char* s) {
  std::string o;
  if (!s)
    return o;
  for (const char* p = s; *p; p++)
    if (*p != '.' && *p != '_')
      o += *p;
  return o;
}
std::string lowerOf(const std::string& s) {
  std::string o = s;
  for (char& c : o)
    if (c >= 'A' && c <= 'Z')
      c = (char)(c - 'A' + 'a');
  return o;
}

// ---- rig map (GLB_RIG_MAP, 22 rows; MMD names as UTF-8 escapes) ----
enum Part : int { P_SPINE, P_HEAD, P_ARMS, P_HANDS, P_LEGS, P_FEET, P_BREAST };
enum Mode : int { M_EULER, M_THIGH, M_ANKLE, M_TOE, M_WRIST };
enum Src : int {
  S_TORSO,
  S_CHEST,
  S_NECK,
  S_SH,
  S_ELB,
  S_HAND,
  S_HIP,
  S_KNEE,
  S_FOOT,
  S_TOE,
  S_BREAST
};
struct RigRow {
  Part part;
  Src src;
  int side; // -1 L, +1 R, 0 mid
  Mode mode;
  const char* to; // MMD/glTF node name (cleanKey-compared)
};
// clang-format off
const RigRow kRig[] = {
  {P_SPINE, S_TORSO, 0, M_EULER, "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab"},       // 上半身
  {P_SPINE, S_CHEST, 0, M_EULER, "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab" "2"},     // 上半身2 (\x needs split before hex digit)
  {P_HEAD,  S_NECK,  0, M_EULER, "\xe9\xa6\x96"},                              // 首
  {P_ARMS,  S_SH,  +1, M_EULER, "\xe8\x85\x95.L"},                             // 腕L <- armR (swap)
  {P_ARMS,  S_SH,  -1, M_EULER, "\xe8\x85\x95.R"},                             // 腕R <- armL
  {P_ARMS,  S_ELB, +1, M_EULER, "\xe3\x81\xb2.L"},                             // ひじL
  {P_ARMS,  S_ELB, -1, M_EULER, "\xe3\x81\xb2.R"},                             // ひじR
  {P_HANDS, S_HAND, +1, M_WRIST,"\xe6\x89\x8b\xe9\xa6\x96.L"},                 // 手首L (anatomical)
  {P_HANDS, S_HAND, -1, M_WRIST,"\xe6\x89\x8b\xe9\xa6\x96.R"},                 // 手首R
  {P_LEGS,  S_HIP, +1, M_THIGH, "\xe8\xb6\xb3.L"},                             // 足L <- legR
  {P_LEGS,  S_HIP, -1, M_THIGH, "\xe8\xb6\xb3.R"},                             // 足R <- legL
  {P_LEGS,  S_KNEE,+1, M_EULER, "\xe3\x81\xb2\xe3\x81\x96.L"},                  // ひざL
  {P_LEGS,  S_KNEE,-1, M_EULER, "\xe3\x81\xb2\xe3\x81\x96.R"},                  // ひざR
  {P_LEGS,  S_KNEE,+1, M_EULER, "\xe3\x81\xb2" "ALT.L"},                       // ひざALTL (aux, off thigh)
  {P_LEGS,  S_KNEE,-1, M_EULER, "\xe3\x81\xb2" "ALT.R"},                       // ひざALTR
  {P_LEGS,  S_HIP, +1, M_THIGH, "\xe8\xb6\xb3\xe6\x8d\xa9.L"},                  // 足捩L (aux, off pelvis)
  {P_LEGS,  S_HIP, -1, M_THIGH, "\xe8\xb6\xb3\xe6\x8d\xa9.R"},                  // 足捩R
  {P_FEET,  S_FOOT,+1, M_ANKLE, "\xe8\xb6\xb3\xe9\xa6\x96.L"},                  // 足首L
  {P_FEET,  S_FOOT,-1, M_ANKLE, "\xe8\xb6\xb3\xe9\xa6\x96.R"},                  // 足首R
  {P_FEET,  S_TOE, +1, M_TOE,   "\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88.L"},      // つま先L
  {P_FEET,  S_TOE, -1, M_TOE,   "\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88.R"},      // つま先R
  {P_BREAST,S_BREAST,+1,M_EULER,"\xe4\xb9\xb3\xe8\xa6\xaa.L"},                  // 乳親L
  {P_BREAST,S_BREAST,-1,M_EULER,"\xe4\xb9\xb3\xe8\xa6\xaa.R"},                  // 乳親R
};
// clang-format on
inline float partGain(Part p) {
  switch (p) {
  case P_ARMS:
  case P_HANDS:
  case P_FEET:
    return 0.85f;
  case P_BREAST:
    return 0.0f; // sculpt preserved, like JS
  default:
    return 1.0f;
  }
}

// ---- geometry store ----
constexpr int kMaxChunkVerts = 65520;
struct Chunk {
  std::vector<float> bind; // xyz|nxnynz per vert (source for skinning)
  std::vector<float> uv;   // u,v per vert
  std::vector<float> live; // skinned xyz|nxnynz (uploaded to GL)
  std::vector<unsigned short> idx;
  std::vector<unsigned short> j4; // joint ids (skinned only)
  std::vector<float> w4;          // joint weights (skinned only)
  std::vector<float> ibm;         // joints*16 inverse bind (skinned only)
  std::vector<int> jnode;         // joint node indices (skinned only)
  int node = -1;
  bool skinned = false;
  float color[4] = {1, 1, 1, 1};
  std::vector<unsigned char> texPx; // decoded RGBA, pending upload
  int texW = 0, texH = 0;
  bool hasTex = false;
  d3gl::GLuint vboP = 0, vboN = 0, vboU = 0, ibo = 0, tex = 0;
  bool gpu = false;
  float lastMat[16] = {0};
  bool rigidDirty = true;
  bool liveDirty = false; // CPU live newer than GPU copy (rigid chunks)
};
struct GNode {
  std::string name, ck, low;
  int parent = -1;
  float t[3] = {0, 0, 0}, s[3] = {1, 1, 1};
  Quat qb;
  float be[3] = {0, 0, 0}; // bind euler XYZ
  float add[3] = {0, 0, 0};
  bool abs = false;
  float absE[3] = {0, 0, 0};
  bool isJoint = false;
};
struct Bone {
  int node = -1;
  Part part = P_SPINE;
  Src src = S_TORSO;
  int side = 0;
  Mode mode = M_EULER;
  float cur[3] = {0, 0, 0};
  bool init = false;
};
struct Finger {
  int n[4] = {-1, -1, -1, -1};
  float bx[4] = {0, 0, 0, 0}, bz[4] = {0, 0, 0, 0};
  int side = 0; // +1=L, -1=R anatomical (GLB_HAND3 keys)
  int order = 0; // index 0..pinky 3, thumb -1
  bool thumb = false;
};
struct Entry {
  std::vector<GNode> nodes;
  std::vector<M4> glob;
  std::vector<char> globOk;
  std::vector<Chunk> chunks;
  std::vector<Bone> bones;
  std::vector<Finger> fingers;
  std::vector<int> tail, ears;
  float baseScale = 1, pelvis[3] = {0, 0.7f, 0}, minY = 0;
  float rootP[3] = {0, 0.62f, 0}, rootE[3] = {0, 0, 0};
  // dynamics (updateGLBModel)
  float prevDepth = 0, jiggleY = 0, jiggleV = 0, calm = 0;
  float earT = 3, earFlick = 0;
  unsigned rng = 1234567;
  int skippedPrim = 0, skippedTex = 0;
};
std::unordered_map<std::string, Entry>& store() {
  static std::unordered_map<std::string, Entry> s;
  return s;
}

// ---- accessor reads ----
const cgltf_accessor* findAcc(const cgltf_primitive& pr, const char* name) {
  for (cgltf_size i = 0; i < pr.attributes_count; i++)
    if (pr.attributes[i].name && !std::strcmp(pr.attributes[i].name, name))
      return pr.attributes[i].data;
  return nullptr;
}
bool readV3(const cgltf_accessor* a, size_t i, float o[3]) {
  if (!a)
    return false;
  cgltf_float t[4] = {0, 0, 0, 0};
  size_t n = (size_t)cgltf_num_components(a->type);
  if (n > 4)
    n = 4;
  if (!cgltf_accessor_read_float(a, (cgltf_size)i, t, (cgltf_size)n))
    return false;
  o[0] = t[0];
  o[1] = n > 1 ? t[1] : 0;
  o[2] = n > 2 ? t[2] : 0;
  return true;
}

// ---- minimal image decode: BMP32/TGA raw; PNG/JPEG detected->skip ----
bool isPng(const unsigned char* d, size_t n) {
  static const unsigned char k[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  return n >= 8 && !std::memcmp(d, k, 8);
}
bool isJpg(const unsigned char* d, size_t n) {
  return n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF;
}
// Returns RGBA (bottom-up rows, GL-ready). BMP/TGA raw only; PNG/JPEG need
// stb_image (TODO vendor): detected and reported, never misdecoded.
bool decodeImage(const unsigned char* d, size_t n, std::vector<unsigned char>& px,
                 int& w, int& h, bool& compressed) {
  px.clear();
  w = h = 0;
  compressed = false;
  if (!d || n < 22)
    return false;
  if (isPng(d, n) || isJpg(d, n)) {
    compressed = true;
    return false;
  }
  // BMP: 'BM', 32-bit BI_RGB, bottom-up.
  if (d[0] == 'B' && d[1] == 'M' && n > 54) {
    int off = d[10] | (d[11] << 8) | (d[12] << 16) | (d[13] << 24);
    int ww = d[18] | (d[19] << 8) | (d[20] << 16) | (d[21] << 24);
    int hh = d[22] | (d[23] << 8) | (d[24] << 16) | (d[25] << 24);
    int bpp = d[28] | (d[29] << 8), comp = d[30];
    if (ww > 0 && hh > 0 && ww < 8192 && hh < 8192 && bpp == 32 && comp == 0 &&
        (size_t)off + (size_t)ww * (size_t)hh * 4 <= n) {
      w = ww;
      h = hh;
      px.assign(d + off, d + off + (size_t)ww * (size_t)hh * 4);
      // BMP is BGRA -> RGBA.
      for (size_t i = 0; i < px.size(); i += 4) {
        unsigned char t = px[i];
        px[i] = px[i + 2];
        px[i + 2] = t;
      }
      return true;
    }
    return false;
  }
  // TGA: uncompressed truecolor (type 2), 24/32-bit, bottom-left origin.
  {
    int idl = d[0], cmap = d[1], type = d[2];
    int ww = d[12] | (d[13] << 8), hh = d[14] | (d[15] << 8), bpp = d[16];
    size_t off = 18 + (size_t)(unsigned)idl;
    if (cmap == 0 && type == 2 && ww > 0 && hh > 0 && ww < 8192 && hh < 8192 &&
        (bpp == 32 || bpp == 24)) {
      size_t stride = (size_t)ww * (size_t)(bpp / 8);
      if (off + stride * (size_t)hh <= n) {
        w = ww;
        h = hh;
        px.resize((size_t)ww * (size_t)hh * 4);
        for (int y = 0; y < hh; y++)
          for (int x = 0; x < ww; x++) {
            size_t s = off + (size_t)y * stride + (size_t)x * (bpp / 8);
            size_t q = ((size_t)y * (size_t)ww + (size_t)x) * 4;
            px[q] = d[s + 2];
            px[q + 1] = d[s + 1];
            px[q + 2] = d[s];
            px[q + 3] = bpp == 32 ? d[s + 3] : 255;
          }
        return true;
      }
    }
  }
  return false;
}
std::vector<unsigned char> readFile(const std::string& p) {
  std::vector<unsigned char> o;
  FILE* fp = nullptr;
#if defined(_MSC_VER)
  fopen_s(&fp, p.c_str(), "rb");
#else
  fp = std::fopen(p.c_str(), "rb");
#endif
  if (!fp)
    return o;
  std::fseek(fp, 0, SEEK_END);
  long n = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (n > 0 && n < 200 * 1024 * 1024) {
    o.resize((size_t)n);
    if (std::fread(o.data(), 1, (size_t)n, fp) != (size_t)n)
      o.clear();
  }
  std::fclose(fp);
  return o;
}

// ---- node global matrices (DFS with memo; root premultiplied) ----
M4 nodeLocal(const GNode& n) {
  Quat q = n.abs ? eulerQ(n.absE[0], n.absE[1], n.absE[2])
                 : mulQ(n.qb, eulerQ(n.add[0], n.add[1], n.add[2]));
  return composeM(n.t, q, n.s);
}
M4 nodeWorld(Entry& e, const M4& root, int i, int depth = 0) {
  size_t u = (size_t)(i < 0 ? 0 : i);
  if (e.globOk[u])
    return e.glob[u];
  if (depth > 256)
    return root; // corrupt cycle guard
  M4 par = root;
  if (e.nodes[u].parent >= 0)
    par = nodeWorld(e, root, e.nodes[u].parent, depth + 1);
  e.glob[u] = mulM(par, nodeLocal(e.nodes[u]));
  e.globOk[u] = 1;
  return e.glob[u];
}

// ---- Game -> her3-source euler synth (stand-in until anim3d port) ----
// Per-pose tables (pos 0..6): hipFlex, spread, knee, armFlex, armSpread,
// elbow, ankle, toe, depthK. Neutral lying-cycle approximations.
void srcEuler(const Game& g, Src s, int side, float t, float o[3]) {
  static const float kHip[7] = {0.5f, 1.1f, 1.3f, 0.2f, 0.9f, 0.7f, 0.6f};
  static const float kSpr[7] = {0.5f, 0.7f, 0.5f, 0.35f, 0.75f, 0.55f, 0.5f};
  static const float kKnee[7] = {1.0f, 0.5f, 0.7f, 0.3f, 1.2f, 0.9f, 0.8f};
  static const float kArm[7] = {0.4f, 0.5f, 0.9f, 0.4f, 0.3f, 0.5f, 0.6f};
  static const float kArmS[7] = {0.5f, 0.5f, 0.3f, 0.4f, 0.6f, 0.5f, 0.45f};
  static const float kElb[7] = {0.6f, 0.7f, 0.4f, 0.6f, 0.5f, 0.6f, 0.5f};
  static const float kAnk[7] = {-1.2f, -1.0f, -1.4f, -1.3f, -0.9f, -1.1f,
                                -1.2f};
  static const float kToe[7] = {0.2f, 0.3f, 0.4f, 0.2f, 0.2f, 0.3f, 0.25f};
  static const float kDK[7] = {0.4f, 0.2f, 0.5f, 0.3f, 0.6f, 0.4f, 0.4f};
  int p = g.pos < 0 ? 0 : (g.pos > 6 ? 6 : g.pos);
  float depth = f(g.depth), ple = f(g.pleasure) / 100.0f;
  float rub = f(ag::clamp(g.rub, 0.0, 1.0));
  float oral = f(ag::clamp(g.oral, 0.0, 1.0));
  float gag = f(ag::clamp(g.oralGag, 0.0, 1.0));
  float breath = sinf(t * 2.5f);
  float act = side > 0 ? 1.0f : 0.0f; // solo rub hand: +side
  if (g.rubZone == 1)
    act = side < 0 ? 1.0f : 0.0f;
  else if (g.rubZone == 2)
    act = side > 0 ? 1.0f : 0.0f;
  else if (!g.solo)
    act = 1.0f;
  o[0] = o[1] = o[2] = 0;
  switch (s) {
  case S_TORSO:
    o[0] = 0.1f + depth * 0.25f + breath * 0.008f;
    o[2] = 0.05f * sinf(t * 0.7f);
    break;
  case S_CHEST:
    o[0] = 0.06f + depth * 0.15f + breath * 0.006f;
    break;
  case S_NECK:
    o[0] = -0.1f + f(g.nod) * 0.2f + breath * 0.004f;
    if (oral > 0.03f)
      o[0] += sinf(t * 5.2f) * 0.06f * (0.4f + oral + gag);
    o[0] += ple * 0.12f; // head tips back with arousal
    break;
  case S_SH:
    o[0] = kArm[p] + sinf(t * 9.0f) * 0.35f * rub * act;
    o[2] = (float)side * (kArmS[p] + 0.1f * sinf(t * 9.0f + 1.2f) * rub * act);
    if (g.solo && rub > 0 && side > 0)
      o[0] += 0.5f; // reach to pelvis
    break;
  case S_ELB:
    o[0] = kElb[p] + 0.18f * sinf(t * 9.0f + 0.7f) * rub * act;
    if (g.solo && rub > 0 && side > 0)
      o[0] += 0.6f;
    break;
  case S_HAND:
    o[0] = kElb[p] * 0.12f;
    break;
  case S_HIP:
    o[0] = kHip[p] + depth * kDK[p];
    o[2] = (float)side * kSpr[p] * (1.0f + 0.1f * sinf(t * 1.1f));
    break;
  case S_KNEE:
    o[0] = kKnee[p];
    break;
  case S_FOOT:
    o[0] = kAnk[p] + 0.15f * breath;
    break;
  case S_TOE:
    o[0] = kToe[p] + 0.05f * breath;
    break;
  default:
    break;
  }
}

// ---- geometry build ----
bool buildEntry(const char* glbPath, Entry& e) {
  cgltf_options opt{};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&opt, glbPath, &data) != cgltf_result_success ||
      !data) {
    if (data)
      cgltf_free(data);
    return false;
  }
  bool ok = false;
  if (cgltf_load_buffers(&opt, data, glbPath) != cgltf_result_success) {
    cgltf_free(data);
    return false;
  }
  // Nodes.
  size_t nn = data->nodes_count;
  if (nn == 0 || nn > 4096) {
    cgltf_free(data);
    return false;
  }
  e.nodes.resize(nn);
  for (size_t i = 0; i < nn; i++) {
    GNode& n = e.nodes[i];
    n.name = data->nodes[i].name ? data->nodes[i].name : "";
    n.ck = cleanKey(n.name.c_str());
    n.low = lowerOf(n.name);
    n.parent = data->nodes[i].parent
                   ? (int)(data->nodes[i].parent - data->nodes)
                   : -1;
    if (n.parent < 0 || (size_t)n.parent >= nn)
      n.parent = -1;
    if (data->nodes[i].has_translation) {
      n.t[0] = data->nodes[i].translation[0];
      n.t[1] = data->nodes[i].translation[1];
      n.t[2] = data->nodes[i].translation[2];
    }
    if (data->nodes[i].has_rotation) {
      n.qb.x = data->nodes[i].rotation[0];
      n.qb.y = data->nodes[i].rotation[1];
      n.qb.z = data->nodes[i].rotation[2];
      n.qb.w = data->nodes[i].rotation[3];
    }
    if (data->nodes[i].has_scale) {
      n.s[0] = data->nodes[i].scale[0];
      n.s[1] = data->nodes[i].scale[1];
      n.s[2] = data->nodes[i].scale[2];
    }
    quatE(n.qb, n.be[0], n.be[1], n.be[2]);
    if (n.low.find("tail") != std::string::npos)
      e.tail.push_back((int)i);
    else if (n.low.find("ear") != std::string::npos)
      e.ears.push_back((int)i);
  }
  auto nodeIdx = [&](const cgltf_node* p) -> int {
    if (!p)
      return -1;
    ptrdiff_t d = p - data->nodes;
    return (d >= 0 && (size_t)d < nn) ? (int)d : -1;
  };
  // Node lookup: exact name, cleanKey, then ASCII-substring fallback.
  auto findNode = [&](const char* want) -> int {
    std::string ck = cleanKey(want);
    for (size_t i = 0; i < nn; i++)
      if (e.nodes[i].name == want)
        return (int)i;
    for (size_t i = 0; i < nn; i++)
      if (!e.nodes[i].ck.empty() && e.nodes[i].ck == ck)
        return (int)i;
    return -1;
  };
  auto findSub = [&](const char* sub) -> int {
    for (size_t i = 0; i < nn; i++)
      if (e.nodes[i].low.find(sub) != std::string::npos)
        return (int)i;
    return -1;
  };
  // Rig rows: exact/cleanKey name match, then ASCII-substring fallback
  // (Mixamo/humanoid rigs + any MMD name missed above).
  for (size_t r = 0; r < sizeof(kRig) / sizeof(kRig[0]); r++) {
    const RigRow& row = kRig[r];
    int ni = findNode(row.to);
    if (ni < 0) {
      // ASCII fallback (Mixamo/humanoid rigs + knee rows).
      const char* fb = nullptr;
      switch (row.src) {
      case S_TORSO:
        fb = "spine";
        break;
      case S_CHEST:
        fb = "chest";
        break;
      case S_NECK:
        fb = "neck";
        break;
      case S_SH:
        fb = "shoulder";
        break;
      case S_ELB:
        fb = "elbow";
        break;
      case S_HAND:
        fb = "wrist";
        break;
      case S_HIP:
        fb = "hip";
        break;
      case S_KNEE:
        fb = "knee";
        break;
      case S_FOOT:
        fb = row.mode == M_TOE ? "toe" : "ankle";
        break;
      case S_TOE:
        fb = "toe";
        break;
      default:
        break;
      }
      if (fb)
        ni = findSub(fb);
      if (ni >= 0 && (row.side != 0)) {
        // Prefer side-suffixed node (L/R or _l/_r or .l/.r).
        bool wantL = row.side < 0;
        for (size_t i = 0; i < nn; i++) {
          const std::string& nm = e.nodes[i].low;
          if (nm.find(fb) == std::string::npos)
            continue;
          bool isL = nm.find("_l") != std::string::npos ||
                     nm.find(".l") != std::string::npos ||
                     nm.find("left") != std::string::npos ||
                     (!nm.empty() && nm.back() == 'l');
          bool isR = nm.find("_r") != std::string::npos ||
                     nm.find(".r") != std::string::npos ||
                     nm.find("right") != std::string::npos ||
                     (!nm.empty() && nm.back() == 'r');
          if ((wantL && isL) || (!wantL && isR)) {
            ni = (int)i;
            break;
          }
        }
      }
    }
    if (ni < 0)
      continue;
    Bone b;
    b.node = ni;
    b.part = row.part;
    b.src = row.src;
    b.side = row.side;
    b.mode = row.mode;
    b.cur[0] = e.nodes[(size_t)ni].be[0];
    b.cur[1] = e.nodes[(size_t)ni].be[1];
    b.cur[2] = e.nodes[(size_t)ni].be[2];
    b.init = true;
    e.nodes[(size_t)ni].isJoint = true;
    e.bones.push_back(b);
  }
  // Fingers: stem + U+FF10..FF13 + L/R (GLB_HAND3 chains).
  {
    static const char* kStems[5] = {
        "\xe8\xa6\xaa\xe6\x8c\x87", // 親指 thumb
        "\xe4\xba\xba\xe6\x8c\x87", // 人指 index
        "\xe4\xb8\xad\xe6\x8c\x87", // 中指 middle
        "\xe8\x96\xac\xe6\x8c\x87", // 薬指 ring
        "\xe5\xb0\x8f\xe6\x8c\x87", // 小指 pinky
    };
    static const char* kDig[4] = {"\xef\xbc\x90", "\xef\xbc\x91",
                                  "\xef\xbc\x92", "\xef\xbc\x93"};
    for (int st = 0; st < 5; st++)
      for (int sd = 0; sd < 2; sd++) {
        Finger fg{};
        fg.side = sd == 0 ? 1 : -1; // 0->L(+1), 1->R(-1)
        fg.thumb = (st == 0);
        fg.order = st - 1;
        char side = sd == 0 ? 'L' : 'R';
        bool any = false;
        for (int d = 0; d < 4; d++) {
          std::string nm = std::string(kStems[st]) + kDig[d] + side;
          int ni = findNode(nm.c_str());
          fg.n[d] = ni;
          if (ni >= 0) {
            any = true;
            float ex, ey, ez;
            quatE(e.nodes[(size_t)ni].qb, ex, ey, ez);
            fg.bx[d] = ex;
            fg.bz[d] = ez;
            e.nodes[(size_t)ni].isJoint = true;
          }
        }
        if (any)
          e.fingers.push_back(fg);
      }
  }
  // Primitives -> chunks.
  std::string dir;
  {
    std::string p = glbPath ? glbPath : "";
    size_t s = p.find_last_of("/\\");
    dir = (s == std::string::npos) ? "" : p.substr(0, s + 1);
  }
  size_t totalV = 0;
  for (size_t ni = 0; ni < nn; ni++) {
    cgltf_mesh* mesh = data->nodes[ni].mesh;
    if (!mesh)
      continue;
    cgltf_skin* skin = data->nodes[ni].skin;
    for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
      cgltf_primitive& pr = mesh->primitives[pi];
      if (pr.has_draco_mesh_compression) {
        e.skippedPrim++;
        continue; // no decoder (see header)
      }
      if (pr.type != cgltf_primitive_type_triangles &&
          pr.type != cgltf_primitive_type_triangle_strip &&
          pr.type != cgltf_primitive_type_triangle_fan) {
        e.skippedPrim++; // POINTS/LINES: 2D engine has no use for them
        continue;
      }
      const cgltf_accessor* aPos = findAcc(pr, "POSITION");
      if (!aPos || aPos->count == 0)
        continue;
      size_t nv = (size_t)aPos->count;
      if (totalV + nv > 4 * 1024 * 1024) {
        e.skippedPrim++;
        continue; // corrupt-count guard
      }
      const cgltf_accessor* aNor = findAcc(pr, "NORMAL");
      const cgltf_accessor* aUV = findAcc(pr, "TEXCOORD_0");
      const cgltf_accessor* aJ = findAcc(pr, "JOINTS_0");
      const cgltf_accessor* aW = findAcc(pr, "WEIGHTS_0");
      const cgltf_accessor* aC = findAcc(pr, "COLOR_0");
      std::vector<float> P(nv * 3), N(nv * 3, 0), UV(nv * 2, 0);
      std::vector<unsigned> J(nv * 4, 0);
      std::vector<float> W(nv * 4, 0);
      std::vector<float> C(nv * 3, 1);
      float v[4];
      for (size_t i = 0; i < nv; i++) {
        if (readV3(aPos, i, v)) {
          P[i * 3] = v[0];
          P[i * 3 + 1] = v[1];
          P[i * 3 + 2] = v[2];
        }
        if (aNor && readV3(aNor, i, v)) {
          N[i * 3] = v[0];
          N[i * 3 + 1] = v[1];
          N[i * 3 + 2] = v[2];
        }
      }
      bool hasUV = false;
      if (aUV) {
        cgltf_float t[2];
        hasUV = true;
        for (size_t i = 0; i < nv; i++) {
          if (cgltf_accessor_read_float(aUV, (cgltf_size)i, t, 2)) {
            UV[i * 2] = t[0];
            UV[i * 2 + 1] = t[1];
          }
        }
      }
      if (!hasUV) { // glbEnsureUV: planar map from local bounds
        float mn[3] = {P[0], P[1], P[2]}, mx[3] = {P[0], P[1], P[2]};
        for (size_t i = 1; i < nv; i++)
          for (int k = 0; k < 3; k++) {
            if (P[i * 3 + (size_t)k] < mn[k])
              mn[k] = P[i * 3 + (size_t)k];
            if (P[i * 3 + (size_t)k] > mx[k])
              mx[k] = P[i * 3 + (size_t)k];
          }
        float sx = mx[0] - mn[0], sy = mx[1] - mn[1];
        if (sx <= 0)
          sx = 1;
        if (sy <= 0)
          sy = 1;
        for (size_t i = 0; i < nv; i++) {
          UV[i * 2] = (P[i * 3] - mn[0]) / sx;
          UV[i * 2 + 1] = (P[i * 3 + 1] - mn[1]) / sy;
        }
      }
      bool skinned = (aJ && aW && skin && skin->joints_count > 0);
      if (skinned) {
        cgltf_uint ji[4];
        cgltf_float wf[4];
        for (size_t i = 0; i < nv; i++) {
          bool okJ = cgltf_accessor_read_uint(aJ, (cgltf_size)i, ji, 4) != 0;
          bool okW =
              cgltf_accessor_read_float(aW, (cgltf_size)i, wf, 4) != 0;
          float sum = 0.0f;
          for (int k = 0; k < 4; k++) {
            J[i * 4 + (size_t)k] = okJ ? ji[k] : 0;
            float wgt = okW ? wf[k] : (k == 0 ? 1.0f : 0.0f);
            W[i * 4 + (size_t)k] = wgt;
            sum += wgt;
          }
          if (sum > 1e-6f) // renormalize first-4 (JS-side same clamp)
            for (int k = 0; k < 4; k++)
              W[i * 4 + (size_t)k] /= sum;
          else
            W[i * 4] = 1.0f;
        }
      }
      bool colorOk = false;
      double cMean[3] = {0, 0, 0};
      if (aC) {
        colorOk = true;
        for (size_t i = 0; i < nv && colorOk; i++) {
          if (!readV3(aC, i, v)) {
            colorOk = false;
            break;
          }
          for (int k = 0; k < 3; k++) {
            if (!(v[k] == v[k]) || v[k] > 1e6f || v[k] < -1e6f) {
              colorOk = false; // NaN/Inf run -> drop (glbDropBadColor)
              break;
            }
            C[i * 3 + (size_t)k] = v[k];
            cMean[k] += v[k];
          }
        }
        if (colorOk)
          for (int k = 0; k < 3; k++)
            cMean[k] /= (double)nv;
      }
      // Indices (+ strip/fan expansion).
      std::vector<unsigned> idx;
      if (pr.indices) {
        size_t nic = (size_t)pr.indices->count;
        idx.reserve(nic);
        cgltf_uint t[1];
        for (size_t i = 0; i < nic; i++)
          if (cgltf_accessor_read_uint(pr.indices, (cgltf_size)i, t, 1))
            idx.push_back(t[0] < nv ? t[0] : 0u);
      } else {
        idx.resize(nv);
        for (size_t i = 0; i < nv; i++)
          idx[i] = (unsigned)i;
      }
      std::vector<unsigned> tris;
      if (pr.type == cgltf_primitive_type_triangles) {
        tris = idx;
        tris.resize((tris.size() / 3) * 3);
      } else if (pr.type == cgltf_primitive_type_triangle_fan) {
        for (size_t i = 1; i + 1 < idx.size(); i++) {
          tris.push_back(idx[0]);
          tris.push_back(idx[i]);
          tris.push_back(idx[i + 1]);
        }
      } else {
        for (size_t i = 0; i + 2 < idx.size(); i++) {
          tris.push_back(idx[i + (i % 2 ? 1 : 0)]);
          tris.push_back(idx[i + (i % 2 ? 0 : 1)]);
          tris.push_back(idx[i + 2]);
        }
      }
      if (tris.empty())
        continue;
      // Missing normals -> area-weighted smooth (bind pose).
      bool needN = (aNor == nullptr);
      if (needN) {
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
          unsigned a = tris[i], b = tris[i + 1], c = tris[i + 2];
          float ax = P[(size_t)a * 3], ay = P[(size_t)a * 3 + 1],
                az = P[(size_t)a * 3 + 2];
          float ex1 = P[(size_t)b * 3] - ax, ey1 = P[(size_t)b * 3 + 1] - ay,
                ez1 = P[(size_t)b * 3 + 2] - az;
          float ex2 = P[(size_t)c * 3] - ax, ey2 = P[(size_t)c * 3 + 1] - ay,
                ez2 = P[(size_t)c * 3 + 2] - az;
          float nx = ey1 * ez2 - ez1 * ey2, ny = ez1 * ex2 - ex1 * ez2,
                nz = ex1 * ey2 - ey1 * ex2;
          N[(size_t)a * 3] += nx;
          N[(size_t)a * 3 + 1] += ny;
          N[(size_t)a * 3 + 2] += nz;
          N[(size_t)b * 3] += nx;
          N[(size_t)b * 3 + 1] += ny;
          N[(size_t)b * 3 + 2] += nz;
          N[(size_t)c * 3] += nx;
          N[(size_t)c * 3 + 1] += ny;
          N[(size_t)c * 3 + 2] += nz;
        }
        for (size_t i = 0; i < nv; i++) {
          float l = sqrtf(N[i * 3] * N[i * 3] + N[i * 3 + 1] * N[i * 3 + 1] +
                          N[i * 3 + 2] * N[i * 3 + 2]);
          if (l > 1e-9f) {
            N[i * 3] /= l;
            N[i * 3 + 1] /= l;
            N[i * 3 + 2] /= l;
          } else {
            N[i * 3 + 1] = 1.0f;
          }
        }
      }
      // Material: base factor + base-color image bytes.
      float factor[4] = {1, 1, 1, 1};
      const unsigned char* imgBytes = nullptr;
      size_t imgLen = 0;
      std::vector<unsigned char> imgFile; // keeps URI bytes alive
      if (pr.material && pr.material->has_pbr_metallic_roughness) {
        for (int k = 0; k < 4; k++)
          factor[k] =
              pr.material->pbr_metallic_roughness.base_color_factor[k];
        cgltf_texture_view& tv =
            pr.material->pbr_metallic_roughness.base_color_texture;
        if (tv.texture && tv.texture->image) {
          cgltf_image* im = tv.texture->image;
          if (im->buffer_view) {
            cgltf_buffer_view* bv = im->buffer_view;
            unsigned char* base = nullptr;
            if (bv->data)
              base = (unsigned char*)bv->data;
            else if (bv->buffer && bv->buffer->data)
              base = (unsigned char*)bv->buffer->data + bv->offset;
            if (base && bv->size > 0) {
              imgBytes = base;
              imgLen = bv->size;
            }
          } else if (im->uri) {
            imgFile = readFile(dir + im->uri);
            if (!imgFile.empty()) {
              imgBytes = imgFile.data();
              imgLen = imgFile.size();
            }
          }
        }
      }
      bool useMeanColor = (colorOk && imgBytes == nullptr);
      // Split into <=64k-vert chunks with local index remap.
      std::vector<int> remap(nv, -1);
      std::vector<unsigned> used;
      std::vector<unsigned> cur;
      cur.reserve(4096);
      auto emitChunk = [&](const std::vector<unsigned>& ct) {
        if (ct.empty())
          return;
        for (unsigned vtx : ct)
          if (remap[vtx] < 0) {
            remap[vtx] = (int)used.size();
            used.push_back(vtx);
          }
        Chunk ch;
        ch.node = (int)ni;
        ch.skinned = skinned;
        ch.bind.resize(used.size() * 6);
        ch.uv.resize(used.size() * 2);
        for (size_t i = 0; i < used.size(); i++) {
          unsigned vtx = used[i];
          ch.bind[i * 6] = P[(size_t)vtx * 3];
          ch.bind[i * 6 + 1] = P[(size_t)vtx * 3 + 1];
          ch.bind[i * 6 + 2] = P[(size_t)vtx * 3 + 2];
          ch.bind[i * 6 + 3] = N[(size_t)vtx * 3];
          ch.bind[i * 6 + 4] = N[(size_t)vtx * 3 + 1];
          ch.bind[i * 6 + 5] = N[(size_t)vtx * 3 + 2];
          ch.uv[i * 2] = UV[(size_t)vtx * 2];
          ch.uv[i * 2 + 1] = UV[(size_t)vtx * 2 + 1];
        }
        ch.live = ch.bind;
        ch.idx.resize(ct.size());
        for (size_t i = 0; i < ct.size(); i++)
          ch.idx[i] = (unsigned short)remap[ct[i]];
        if (skinned) {
          ch.j4.resize(used.size() * 4);
          ch.w4.resize(used.size() * 4);
          for (size_t i = 0; i < used.size(); i++) {
            unsigned vtx = used[i];
            for (int k = 0; k < 4; k++) {
              unsigned j = J[(size_t)vtx * 4 + (size_t)k];
              if (j >= (unsigned)skin->joints_count)
                j = 0;
              ch.j4[i * 4 + (size_t)k] = (unsigned short)j;
              ch.w4[i * 4 + (size_t)k] = W[(size_t)vtx * 4 + (size_t)k];
            }
          }
          ch.jnode.resize((size_t)skin->joints_count);
          for (cgltf_size j = 0; j < skin->joints_count; j++)
            ch.jnode[j] = nodeIdx(skin->joints[j]);
          ch.ibm.resize((size_t)skin->joints_count * 16, 0);
          if (skin->inverse_bind_matrices) {
            cgltf_float m[16];
            for (cgltf_size j = 0; j < skin->joints_count; j++) {
              if (cgltf_accessor_read_float(skin->inverse_bind_matrices, j,
                                           m, 16))
                for (int k = 0; k < 16; k++)
                  ch.ibm[(size_t)j * 16 + (size_t)k] = m[k];
              else
                ch.ibm[(size_t)j * 16] = ch.ibm[(size_t)j * 16 + 5] =
                    ch.ibm[(size_t)j * 16 + 10] =
                        ch.ibm[(size_t)j * 16 + 15] = 1.0f;
            }
          } else {
            for (size_t j = 0; j < ch.jnode.size(); j++)
              ch.ibm[j * 16] = ch.ibm[j * 16 + 5] = ch.ibm[j * 16 + 10] =
                  ch.ibm[j * 16 + 15] = 1.0f;
          }
        }
        for (int k = 0; k < 4; k++)
          ch.color[k] = factor[k];
        if (useMeanColor) // valid COLOR_0, untextured (Kiyoko body paint)
          for (int k = 0; k < 3; k++)
            ch.color[k] *= f(cMean[k]);
        if (imgBytes && imgLen > 0) {
          bool comp = false;
          std::vector<unsigned char> px;
          int tw = 0, th = 0;
          if (decodeImage(imgBytes, imgLen, px, tw, th, comp) && !px.empty()) {
            ch.texPx = std::move(px);
            ch.texW = tw;
            ch.texH = th;
            ch.hasTex = true;
          } else if (comp) {
            e.skippedTex++; // PNG/JPEG: needs stb_image (header TODO)
          }
        }
        e.chunks.push_back(std::move(ch));
        totalV += used.size();
        for (unsigned vtx : used)
          remap[vtx] = -1;
        used.clear();
      };
      for (size_t i = 0; i < tris.size(); i += 3) {
        // Greedy: start new chunk if tri would overflow the vert budget.
        // remap[v]>=0 iff v is already in `used`, so no linear search.
        int fresh = 0;
        for (int k = 0; k < 3; k++)
          if (remap[tris[i + (size_t)k]] < 0)
            fresh++;
        if (used.size() + (size_t)fresh > (size_t)kMaxChunkVerts &&
            !cur.empty()) {
          emitChunk(cur);
          cur.clear();
        }
        // Tentatively register.
        for (int k = 0; k < 3; k++) {
          unsigned vtx = tris[i + (size_t)k];
          if (remap[vtx] < 0) {
            remap[vtx] = (int)used.size();
            used.push_back(vtx);
          }
          cur.push_back(vtx);
        }
      }
      if (!cur.empty())
        emitChunk(cur);
    }
  }
  if (e.chunks.empty()) {
    cgltf_free(data);
    return false;
  }
  // Fit (glbPlaceEntry): scale = 1.35/maxDim over bind positions.
  {
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (const Chunk& ch : e.chunks)
      for (size_t i = 0; i < ch.bind.size() / 6; i++)
        for (int k = 0; k < 3; k++) {
          float vtx = ch.bind[i * 6 + (size_t)k];
          if (vtx < mn[k])
            mn[k] = vtx;
          if (vtx > mx[k])
            mx[k] = vtx;
        }
    float dim =
        std::max({mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], 1e-6f});
    e.baseScale = 1.35f / dim;
    float cx = (mn[0] + mx[0]) * 0.5f, cy = (mn[1] + mx[1]) * 0.5f,
          cz = (mn[2] + mx[2]) * 0.5f;
    e.rootP[0] = -cx * e.baseScale;
    e.rootP[1] = -cy * e.baseScale + 0.62f;
    e.rootP[2] = -cz * e.baseScale;
    e.minY = mn[1];
    // Pelvis anchor: bone first (下半身 > hips/pelvis > 腰), else 52% rule.
    int pel = findNode("\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab"); // 下半身
    if (pel < 0)
      pel = findSub("hips");
    if (pel < 0)
      pel = findSub("pelvis");
    if (pel < 0)
      pel = findNode("\xe8\x85\xb0"); // 腰 fallback
    if (pel >= 0) {
      cgltf_float m[16];
      cgltf_node_transform_world(&data->nodes[(size_t)pel], m);
      e.pelvis[0] = m[12] / e.baseScale;
      e.pelvis[1] = m[13] / e.baseScale;
      e.pelvis[2] = m[14] / e.baseScale;
    } else {
      e.pelvis[0] = cx;
      e.pelvis[1] = mn[1] + (mx[1] - mn[1]) * 0.52f;
      e.pelvis[2] = cz;
    }
  }
  e.glob.assign(nn, M4{});
  e.globOk.assign(nn, 0);
  ok = true;
  cgltf_free(data);
  return ok;
}

// ---- skin one entry into live buffers (root premultiplied) ----
void skinEntry(Entry& e, const M4& root) {
  std::fill(e.globOk.begin(), e.globOk.end(), 0);
  for (size_t i = 0; i < e.nodes.size(); i++)
    nodeWorld(e, root, (int)i);
  for (Chunk& ch : e.chunks) {
    if (ch.skinned) {
      size_t nv = ch.bind.size() / 6, nj = ch.jnode.size();
      // Joint matrices on stack-friendly scratch (heap for big rigs).
      for (size_t i = 0; i < nv; i++) {
        float px = ch.bind[i * 6], py = ch.bind[i * 6 + 1],
              pz = ch.bind[i * 6 + 2];
        float nx = ch.bind[i * 6 + 3], ny = ch.bind[i * 6 + 4],
              nz = ch.bind[i * 6 + 5];
        float ox = 0.0f, oy = 0.0f, oz = 0.0f, onx = 0.0f, ony = 0.0f,
              onz = 0.0f;
        for (int k = 0; k < 4; k++) {
          float w = ch.w4[i * 4 + (size_t)k];
          if (w <= 0)
            continue;
          unsigned j = ch.j4[i * 4 + (size_t)k];
          M4 jm{};
          if ((size_t)j < nj && ch.jnode[(size_t)j] >= 0) {
            const M4& gj = e.glob[(size_t)ch.jnode[(size_t)j]];
            const float* ib = &ch.ibm[(size_t)j * 16];
            M4 b;
            std::memcpy(b.m, ib, sizeof(b.m));
            jm = mulM(gj, b);
          } else {
            jm = e.glob[(size_t)(ch.node >= 0 ? ch.node : 0)];
          }
          V3 p = xformP(jm, px, py, pz);
          V3 v = xformV(jm, nx, ny, nz);
          ox += w * p.x;
          oy += w * p.y;
          oz += w * p.z;
          onx += w * v.x;
          ony += w * v.y;
          onz += w * v.z;
        }
        float l = sqrtf(onx * onx + ony * ony + onz * onz);
        if (l > 1e-9f) {
          onx /= l;
          ony /= l;
          onz /= l;
        }
        ch.live[i * 6] = ox;
        ch.live[i * 6 + 1] = oy;
        ch.live[i * 6 + 2] = oz;
        ch.live[i * 6 + 3] = onx;
        ch.live[i * 6 + 4] = ony;
        ch.live[i * 6 + 5] = onz;
      }
    } else {
      const M4& m = e.glob[(size_t)(ch.node >= 0 ? ch.node : 0)];
      bool same = true;
      for (int k = 0; k < 16; k++)
        if (m.m[k] != ch.lastMat[k]) {
          same = false;
          break;
        }
      if (same && !ch.rigidDirty)
        continue;
      std::memcpy(ch.lastMat, m.m, sizeof(ch.lastMat));
      ch.rigidDirty = false;
      ch.liveDirty = true;
      size_t nv = ch.bind.size() / 6;
      for (size_t i = 0; i < nv; i++) {
        V3 p = xformP(m, ch.bind[i * 6], ch.bind[i * 6 + 1],
                      ch.bind[i * 6 + 2]);
        V3 v = xformV(m, ch.bind[i * 6 + 3], ch.bind[i * 6 + 4],
                      ch.bind[i * 6 + 5]);
        float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (l > 1e-9f) {
          v.x /= l;
          v.y /= l;
          v.z /= l;
        }
        ch.live[i * 6] = p.x;
        ch.live[i * 6 + 1] = p.y;
        ch.live[i * 6 + 2] = p.z;
        ch.live[i * 6 + 3] = v.x;
        ch.live[i * 6 + 4] = v.y;
        ch.live[i * 6 + 5] = v.z;
      }
    }
  }
}

// ---- GPU upload (lazy; safe to call without context: guards glReady) ----
void ensureChunkGL(Chunk& ch, bool skinned) {
  using namespace d3gl;
  if (ch.gpu || !glReady())
    return;
  GenBuffers(1, &ch.vboP);
  GenBuffers(1, &ch.vboN);
  GenBuffers(1, &ch.vboU);
  GenBuffers(1, &ch.ibo);
  if (!ch.vboP || !ch.vboN || !ch.vboU || !ch.ibo)
    return;
  size_t nv = ch.live.size() / 6;
  std::vector<float> P(nv * 3), Nr(nv * 3);
  for (size_t i = 0; i < nv; i++) {
    P[i * 3] = ch.live[i * 6];
    P[i * 3 + 1] = ch.live[i * 6 + 1];
    P[i * 3 + 2] = ch.live[i * 6 + 2];
    Nr[i * 3] = ch.live[i * 6 + 3];
    Nr[i * 3 + 1] = ch.live[i * 6 + 4];
    Nr[i * 3 + 2] = ch.live[i * 6 + 5];
  }
  d3gl::GLenum usage = skinned ? DYNAMIC_DRAW : STATIC_DRAW;
  BindBuffer(ARRAY_BUFFER, ch.vboP);
  BufferData(ARRAY_BUFFER, (d3gl::GLsizeiptr)(P.size() * sizeof(float)),
             P.data(), usage);
  BindBuffer(ARRAY_BUFFER, ch.vboN);
  BufferData(ARRAY_BUFFER, (d3gl::GLsizeiptr)(Nr.size() * sizeof(float)),
             Nr.data(), usage);
  BindBuffer(ARRAY_BUFFER, ch.vboU);
  BufferData(ARRAY_BUFFER, (d3gl::GLsizeiptr)(ch.uv.size() * sizeof(float)),
             ch.uv.data(), STATIC_DRAW);
  BindBuffer(ELEMENT_ARRAY_BUFFER, ch.ibo);
  BufferData(ELEMENT_ARRAY_BUFFER,
             (d3gl::GLsizeiptr)(ch.idx.size() * sizeof(unsigned short)),
             ch.idx.data(), STATIC_DRAW);
  BindBuffer(ARRAY_BUFFER, 0);
  BindBuffer(ELEMENT_ARRAY_BUFFER, 0);
  if (ch.hasTex && !ch.texPx.empty() && ch.texW > 0 && ch.texH > 0) {
    GenTextures(1, &ch.tex);
    if (ch.tex) {
      PixelStorei(UNPACK_ALIGNMENT, 1);
      BindTexture(TEXTURE_2D, ch.tex);
      TexImage2D(TEXTURE_2D, 0, (d3gl::GLint)RGBA, ch.texW, ch.texH, 0,
                 RGBA, UNSIGNED_BYTE, ch.texPx.data());
      TexParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, (d3gl::GLint)LINEAR);
      TexParameteri(TEXTURE_2D, TEXTURE_MAG_FILTER, (d3gl::GLint)LINEAR);
      TexParameteri(TEXTURE_2D, TEXTURE_WRAP_S, (d3gl::GLint)CLAMP_TO_EDGE);
      TexParameteri(TEXTURE_2D, TEXTURE_WRAP_T, (d3gl::GLint)CLAMP_TO_EDGE);
      BindTexture(TEXTURE_2D, 0);
      // Pixels now live on the GPU; release the staging copy.
      ch.texPx.clear();
      ch.texPx.shrink_to_fit();
    }
  }
  ch.gpu = true;
  ch.liveDirty = false;
}
void pushChunkGL(Chunk& ch) {
  using namespace d3gl;
  if (!ch.gpu || !glReady())
    return;
  size_t nv = ch.live.size() / 6;
  std::vector<float> P(nv * 3), Nr(nv * 3);
  for (size_t i = 0; i < nv; i++) {
    P[i * 3] = ch.live[i * 6];
    P[i * 3 + 1] = ch.live[i * 6 + 1];
    P[i * 3 + 2] = ch.live[i * 6 + 2];
    Nr[i * 3] = ch.live[i * 6 + 3];
    Nr[i * 3 + 1] = ch.live[i * 6 + 4];
    Nr[i * 3 + 2] = ch.live[i * 6 + 5];
  }
  BindBuffer(ARRAY_BUFFER, ch.vboP);
  BufferSubData(ARRAY_BUFFER, 0, (d3gl::GLsizeiptr)(P.size() * sizeof(float)),
                P.data());
  BindBuffer(ARRAY_BUFFER, ch.vboN);
  BufferSubData(ARRAY_BUFFER, 0, (d3gl::GLsizeiptr)(Nr.size() * sizeof(float)),
                Nr.data());
  BindBuffer(ARRAY_BUFFER, 0);
}
void freeChunkGL(Chunk& ch) {
  using namespace d3gl;
  if (!glReady()) {
    ch.gpu = false;
    ch.vboP = ch.vboN = ch.vboU = ch.ibo = ch.tex = 0;
    return;
  }
  if (ch.vboP || ch.vboN || ch.vboU || ch.ibo) {
    d3gl::GLuint b[4] = {ch.vboP, ch.vboN, ch.vboU, ch.ibo};
    // DeleteBuffers takes a contiguous array; delete non-zero only.
    for (int i = 0; i < 4; i++)
      if (b[i])
        DeleteBuffers(1, &b[i]);
  }
  if (ch.tex)
    DeleteTextures(1, &ch.tex);
  ch.gpu = false;
  ch.vboP = ch.vboN = ch.vboU = ch.ibo = ch.tex = 0;
}

} // namespace

// ---- d3::Model ----
bool Model::load(const char* glbPath) {
  path = glbPath ? glbPath : "";
  loaded = false;
  if (path.empty())
    return false;
  Entry e;
  if (!buildEntry(path.c_str(), e)) {
    store().erase(path); // drop stale entry: loaded=false, draw() no-ops
    return false;
  }
  // Initial bind-pose skin so first draw is valid even before pose().
  {
    float one[3] = {e.baseScale, e.baseScale, e.baseScale};
    M4 r = composeM(e.rootP, eulerQ(0, 0, 0), one);
    skinEntry(e, r);
  }
  size_t nv = 0;
  for (const Chunk& ch : e.chunks)
    nv += ch.bind.size() / 6;
  std::fprintf(stderr, "[ag_d3_glb] %s: %u chunks, %u verts, %u bones, "
                       "skipped prim %d tex %d\n",
               path.c_str(), (unsigned)e.chunks.size(), (unsigned)nv,
               (unsigned)e.bones.size(), e.skippedPrim, e.skippedTex);
  store()[path] = std::move(e);
  loaded = true;
  return true;
}

void Model::unload() {
  auto it = store().find(path);
  if (it != store().end()) {
    for (Chunk& ch : it->second.chunks)
      freeChunkGL(ch);
    store().erase(it);
  }
  loaded = false;
}

void Model::pose(const Game& g, double dt) {
  auto it = store().find(path);
  if (it == store().end())
    return;
  Entry& e = it->second;
  float dtf = dt <= 0 ? 0.0f : (dt > 0.1 ? 0.1f : f(dt));
  bool frozen = (g.state == "intro");
  float t = f(g.t);
  float depth = f(g.depth);
  float depthDelta = depth - e.prevDepth;
  e.prevDepth = depth;
  // updateGLBModel: jiggle spring + calm gate.
  if (frozen) {
    e.jiggleV = 0;
    e.jiggleY = 0;
  } else if (dtf > 0) {
    float force = -depthDelta * 1.8f;
    e.jiggleV += (force - 130.0f * e.jiggleY - 10.0f * e.jiggleV) * dtf;
    e.jiggleY += e.jiggleV * dtf;
  }
  float calm = 0.12f;
  if (!frozen) {
    float depthVel = dtf > 0 ? fabsf(depthDelta) / dtf : 0.0f;
    float touching = (g.spaceHeld != 0 || g.dragOn || ((int)g.rubT) != 0 ||
                      ((int)g.kissT) != 0 || ((int)g.oralT) != 0)
                         ? 0.7f
                         : 0.0f;
    float hot = (ag::clamp(g.pleasure, 0.0, 100.0) > 40.0) ? 0.3f : 0.0f;
    float tgt = f(ag::clamp((double)(depthVel * 3.0f + touching + hot), 0.0,
                            1.0));
    e.calm += (tgt - e.calm) * (dtf > 0 ? (1.0f - expf(-3.0f * dtf)) : 1.0f);
    calm = 0.12f + 0.88f * e.calm;
  }
  float ple = f(ag::clamp(g.pleasure, 0.0, 100.0) / 100.0);
  // Reset per-frame offsets.
  for (GNode& n : e.nodes) {
    n.add[0] = n.add[1] = n.add[2] = 0;
    n.abs = false;
  }
  // glbDriveRig: damped local copy with special cases.
  float rate = dtf > 0 ? (1.0f - expf(-18.0f * dtf)) : 1.0f;
  float se[3];
  for (Bone& b : e.bones) {
    float gain = partGain(b.part);
    if (gain == 0)
      continue; // breast locked (sculpt preserved)
    GNode& n = e.nodes[(size_t)b.node];
    srcEuler(g, b.src, b.side, t, se);
    float tx, ty, tz;
    bool absolute = false;
    if (b.mode == M_ANKLE) {
      // Affine rest map (GLB_ANKLE0/ANKLE_K) + knee-bend dangle.
      float ke[3] = {0, 0, 0};
      srcEuler(g, S_KNEE, b.side, t, ke);
      float smA = se[0];
      float dg = f(ag::clamp((double)ke[0] - 0.5, 0.0, 1.4)) * 0.7f;
      tx = -1.6f + smA * 0.85f + dg;
      ty = tz = 0;
      absolute = true;
    } else if (b.mode == M_TOE) {
      tx = se[0] * 0.5f; // toeK; toeAbs=false (additive, like JS default)
      ty = tz = 0;
    } else if (b.mode == M_WRIST) {
      float ex = 0.0f;
      srcEuler(g, S_ELB, b.side, t, se);
      ex = se[0];
      tx = ex * 0.12f;
      ty = tz = 0;
    } else if (b.mode == M_THIGH) {
      tx = se[0];
      ty = 0;
      tz = -se[2]; // mirrored spread (opposite axis convention)
      absolute = true;
    } else if (b.src == S_KNEE) {
      tx = se[0] + e.jiggleY * 1.5f;
      ty = tz = 0;
    } else if (b.src == S_ELB) {
      tx = se[0];
      ty = tz = 0; // elbow hinge flexion
    } else {
      tx = se[0];
      ty = se[1];
      tz = se[2];
    }
    if (b.src == S_BREAST) {
      tx = ty = tz = 0;
    }
    float gx = (absolute ? 0 : n.be[0]) + tx * gain;
    float gy = (absolute ? 0 : n.be[1]) + ty * gain;
    float gz = (absolute ? 0 : n.be[2]) + tz * gain;
    if (!b.init) {
      b.cur[0] = gx;
      b.cur[1] = gy;
      b.cur[2] = gz;
      b.init = true;
    } else if (rate >= 1) {
      b.cur[0] = gx;
      b.cur[1] = gy;
      b.cur[2] = gz;
    } else {
      b.cur[0] += (gx - b.cur[0]) * rate;
      b.cur[1] += (gy - b.cur[1]) * rate;
      b.cur[2] += (gz - b.cur[2]) * rate;
    }
    if (absolute) {
      n.abs = true;
      n.absE[0] = b.cur[0];
      n.absE[1] = b.cur[1];
      n.absE[2] = b.cur[2];
    } else {
      n.add[0] = b.cur[0] - n.be[0];
      n.add[1] = b.cur[1] - n.be[1];
      n.add[2] = b.cur[2] - n.be[2];
    }
  }
  // Fingers: task curl/fan (GLB_HAND3 synth; rubbing hand ripples).
  {
    float rub = f(ag::clamp(g.rub, 0.0, 1.0));
    for (Finger& fg : e.fingers) {
      bool active =
          (rub > 0.05f) && (g.solo ? fg.side < 0 : true) &&
          (g.solo || g.rubZone == 0 || (g.rubZone == 1 && fg.side > 0) ||
           (g.rubZone == 2 && fg.side < 0) || g.rubZone == 3);
      float curl = 0.50f, spread = 0.12f;
      if (active) {
        curl = 0.75f + sinf(t * 12.0f) * 0.08f;
        spread = 0.2f;
      }
      curl = f(ag::clamp((double)curl, 0.0, 1.0)) * 0.85f; // hands gain
      spread = f(ag::clamp((double)spread, 0.0, 1.0)) * 0.85f;
      float curlK = fg.thumb ? 0.55f : 1.0f;
      float fan = fg.order < 0 ? 0 : ((float)fg.order - 1.5f) * spread * 0.14f;
      static const float kDecay[4] = {0.95f, 0.75f, 0.55f, 0.38f};
      for (int s = 0; s < 4; s++) {
        if (fg.n[s] < 0)
          continue;
        GNode& n = e.nodes[(size_t)fg.n[s]];
        float k = kDecay[s] * curlK;
        n.add[0] += curl * 0.85f * k;
        n.add[2] += (s == 0 ? fan : fan * 0.4f);
      }
    }
  }
  // glbSecondary3: tail sway + ear flicks (additive, calm-scaled).
  if (!frozen) {
    float sway = 0.15f + 0.85f * calm;
    for (size_t i = 0; i < e.tail.size(); i++) {
      GNode& n = e.nodes[(size_t)e.tail[i]];
      n.add[2] += sinf(t * 2.1f + (float)i * 1.7f) * 0.10f * sway +
                  e.jiggleY * 1.6f;
      n.add[0] += sinf(t * 1.6f + (float)i * 0.9f) * 0.05f * sway;
    }
    e.rng = e.rng * 1664525u + 1013904223u;
    e.earT -= dtf;
    if (e.earT <= 0) {
      e.earT = 2.5f + (float)(e.rng % 1000) / 1000.0f * 4.0f;
      e.earFlick = 0.32f;
    }
    float flick = 0.0f;
    if (e.earFlick > 0) {
      e.earFlick = e.earFlick > dtf ? e.earFlick - dtf : 0;
      float fp = 1.0f - e.earFlick / 0.32f;
      flick = sinf(fp * 3.14159265f * 5.0f) * 0.16f * (1.0f - fp);
    }
    for (int ei : e.ears)
      e.nodes[(size_t)ei].add[0] += flick;
  }
  // glbFollowHer3 (synth her frame until chars3d rig lands): model origin =
  // synth-her-pelvis minus rotated pelvis offset (+ thrust/breath/jiggle),
  // orientation = her yaw + oral wobble. Per-pose tables are stand-ins.
  static const float kYaw[7] = {0, 0.3f, 3.14f, 0.1f, 0, 2.8f, 0.5f};
  static const float kHerY[7] = {0.45f, 0.5f, 0.55f, 0.4f, 0.5f, 0.48f, 0.46f};
  static const float kHerZ[7] = {1.6f, 1.6f, 1.7f, 1.5f, 1.6f, 1.6f, 1.6f};
  int p = g.pos < 0 ? 0 : (g.pos > 6 ? 6 : g.pos);
  float breath = sinf(t * 2.5f) * 0.008f * calm;
  float oral = f(ag::clamp(g.oral, 0.0, 1.0));
  float wob = 0.0f;
  if (oral > 0.03f) {
    float gag = f(ag::clamp(g.oralGag, 0.0, 1.0));
    wob = sinf(t * 5.2f) * 0.022f * (0.4f + oral + gag);
  }
  e.rootE[0] = wob;
  e.rootE[1] = kYaw[p];
  e.rootE[2] = 0;
  float br = sinf(t * 6.2831853f * (0.16f + ple * 0.0045f)) *
             ((2.4f + ple * 1.6f) / 260.0f) * calm;
  float rs = e.baseScale * (1.0f + br * 0.6f);
  if (rs < 0.5f * e.baseScale)
    rs = 0.5f * e.baseScale;
  Quat rq = eulerQ(e.rootE[0], e.rootE[1], e.rootE[2]);
  float her[3] = {0, kHerY[p], kHerZ[p] + depth * 0.1f};
  float unit[3] = {1, 1, 1};
  float zero[3] = {0, 0, 0};
  V3 off = xformV(composeM(zero, rq, unit), e.pelvis[0] * rs,
                  e.pelvis[1] * rs, e.pelvis[2] * rs);
  float rp[3] = {her[0] - off.x, her[1] - off.y + e.jiggleY * 0.05f +
                                    (frozen ? 0.0f : breath),
                 her[2] - off.z};
  float one[3] = {rs, rs, rs};
  M4 root = composeM(rp, rq, one);
  // Bed-solid lift (sunk-only, like glbCollideBed; rigged-model rule).
  {
    float worldMin = rp[1] + e.minY * rs;
    float floor = (rp[0] > -2.3f && rp[0] < 2.3f && rp[2] > 0.85f &&
                   rp[2] < 3.95f)
                      ? 0.24f
                      : 0.13f;
    if (worldMin < floor) {
      rp[1] += floor - worldMin;
      root = composeM(rp, rq, one);
    }
  }
  e.rootP[0] = rp[0];
  e.rootP[1] = rp[1];
  e.rootP[2] = rp[2];
  skinEntry(e, root);
  if (glReady())
    for (Chunk& ch : e.chunks)
      if (ch.gpu && (ch.skinned || ch.liveDirty)) {
        pushChunkGL(ch);
        ch.liveDirty = false;
      }
}

void Model::draw(Engine& e, const Game& g) {
  (void)g;
  if (!loaded)
    return; // missing file: graceful no-op
  auto it = store().find(path);
  if (it == store().end())
    return;
  Entry& en = it->second;
  if (!glReady())
    return; // headless parity: no context, no draw
  ensureProg();
  Prog& pr = prog();
  if (!pr.ok)
    return;
  using namespace d3gl;
  for (Chunk& ch : en.chunks)
    ensureChunkGL(ch, ch.skinned);
  // Camera rig (engine3d orbit defaults; aspect TODO: real framebuffer).
  const Camera& c = e.cam;
  float yaw = f(c.yaw), pitch = f(c.pitch), dist = f(c.dist);
  V3 at{f(c.tx), f(c.ty), f(c.tz)};
  V3 eye{at.x + dist * cosf(pitch) * sinf(yaw), at.y + dist * sinf(pitch),
         at.z + dist * cosf(pitch) * cosf(yaw)};
  M4 view = lookM(eye, at);
  M4 proj = perspM(f(c.fov) * 3.14159265f / 180.0f, 16.0f / 9.0f, 0.05f, 100);
  M4 vp = mulM(proj, view);
  float one[3] = {1, 1, 1};
  float zp[3] = {0, 0, 0};
  M4 ident = composeM(zp, Quat{}, one);
  M4 mvp = mulM(vp, ident);
  Enable(DEPTH_TEST);
  DepthFunc(LEQUAL);
  Enable(CULL_FACE);
  CullFace(BACK);
  UseProgram(pr.id);
  if (pr.uMVP >= 0)
    UniformMatrix4fv(pr.uMVP, 1, FALSE_, mvp.m);
  if (pr.uModel >= 0)
    UniformMatrix4fv(pr.uModel, 1, FALSE_, ident.m);
  ActiveTexture(TEXTURE0);
  for (Chunk& ch : en.chunks) {
    if (!ch.gpu || ch.idx.empty())
      continue;
    bool textured = ch.hasTex && ch.tex != 0;
    if (pr.uColor >= 0) {
      float col[4] = {ch.color[0], ch.color[1], ch.color[2], ch.color[3]};
      if (textured) {
        col[0] = col[1] = col[2] = 1.0f; // map carries albedo
      }
      Uniform4fv(pr.uColor, 1, col);
    }
    if (pr.uHasTex >= 0)
      Uniform1i(pr.uHasTex, textured ? 1 : 0);
    if (textured)
      BindTexture(TEXTURE_2D, ch.tex);
    else
      BindTexture(TEXTURE_2D, 0);
    if (pr.aPos >= 0) {
      BindBuffer(ARRAY_BUFFER, ch.vboP);
      VertexAttribPointer((d3gl::GLuint)pr.aPos, 3, FLOAT_, FALSE_, 0,
                          nullptr);
      EnableVertexAttribArray((d3gl::GLuint)pr.aPos);
    }
    if (pr.aNor >= 0) {
      BindBuffer(ARRAY_BUFFER, ch.vboN);
      VertexAttribPointer((d3gl::GLuint)pr.aNor, 3, FLOAT_, FALSE_, 0,
                          nullptr);
      EnableVertexAttribArray((d3gl::GLuint)pr.aNor);
    }
    if (pr.aUV >= 0) {
      BindBuffer(ARRAY_BUFFER, ch.vboU);
      VertexAttribPointer((d3gl::GLuint)pr.aUV, 2, FLOAT_, FALSE_, 0,
                          nullptr);
      EnableVertexAttribArray((d3gl::GLuint)pr.aUV);
    }
    BindBuffer(ELEMENT_ARRAY_BUFFER, ch.ibo);
    DrawElements(TRIANGLES, (d3gl::GLsizei)ch.idx.size(), UNSIGNED_SHORT,
                 nullptr);
    if (pr.aPos >= 0)
      DisableVertexAttribArray((d3gl::GLuint)pr.aPos);
    if (pr.aNor >= 0)
      DisableVertexAttribArray((d3gl::GLuint)pr.aNor);
    if (pr.aUV >= 0)
      DisableVertexAttribArray((d3gl::GLuint)pr.aUV);
  }
  BindBuffer(ARRAY_BUFFER, 0);
  BindBuffer(ELEMENT_ARRAY_BUFFER, 0);
  BindTexture(TEXTURE_2D, 0);
}

} // namespace ag::d3
