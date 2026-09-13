// Afterglow native — ag_d3_engine.cpp
// Real d3::Engine backend: port of js3d/engine3d.js (renderer/scene setup,
// orbit camera rig, PBR-lite lighting) onto the fixed ag_3d.h surface via
// the d3gl:: ABI (ag_d3_gl.h). Classic-script rules apply: no new headers,
// no edits elsewhere — this is a NEW file only.
//
// Frame contract (until the App3d/Cast/Fx ports land):
//   eng.beginFrame();  // OPAQUE bucket: viewport+clear, depth-tested opaque
//                      //   state, lit program bound, view/proj/lights live.
//   ... opaque draws (future Model/Cast; per-mesh color via uModel/uBaseCol)
//   eng.endFrame();    // FX bucket: additive blending, depth test ON, depth
//                      //   write OFF, fx program bound (uMVP = proj*view).
//   ... fx draws (future Fx: vertex-colored tris/quads, hearts/dust) ...
//   // the next beginFrame() restores opaque state.
//
// JS refs (js3d/engine3d.js): renderer §18-32, lights §288-327 ("buildLights"),
// camera rig §452-463 (CAM3) + §466-638 ("updateCamera3", orbit branch +
// smoothing k), controls §649-728 (CAMKEY3, wheel, drag sens, R reset) +
// §765-788 ("updateCamKeys3"), resize §793-801.
//
// Y-up world, metres-ish units, like three.js. Directional uniform vectors
// point TOWARD the light (three.js shines from .position toward origin).

#include "ag_3d.h"
#include "ag_d3_gl.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace ag::d3 {
namespace {

// ---------------------------------------------------------------- Mat4 ---
// Column-major float[16], OpenGL convention. Model math stays in double
// (JS-number parity); cast to float only at uniform upload.
struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  static Mat4 identity() { return Mat4{}; }

  static Mat4 multiply(const Mat4& a, const Mat4& b) { // out = a*b
    Mat4 o;
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r) {
        float s = 0.0f;
        for (int k = 0; k < 4; ++k) s += a.m[k * 4 + r] * b.m[c * 4 + k];
        o.m[c * 4 + r] = s;
      }
    return o;
  }

  static Mat4 perspective(float fovY, float aspect, float zn, float zf) {
    Mat4 o;
    for (int i = 0; i < 16; ++i) o.m[i] = 0.0f;
    if (aspect <= 0.0f) aspect = 16.0f / 9.0f;
    if (zf <= zn) zf = zn + 1.0f;
    const float t = std::tan(fovY * 0.5f);
    o.m[0] = 1.0f / (t * aspect);
    o.m[5] = 1.0f / t;
    o.m[10] = (zf + zn) / (zn - zf);
    o.m[11] = -1.0f;
    o.m[14] = (2.0f * zf * zn) / (zn - zf);
    return o;
  }

  static Mat4 lookAt(float ex, float ey, float ez, float cx, float cy,
                     float cz, float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (fl > 0.0f) {
      fx /= fl;
      fy /= fl;
      fz /= fl;
    } else {
      fz = -1.0f;
    }
    float sx = fy * uz - fz * uy, sy = fz * ux - fx * uz, sz = fx * uy - fy * ux;
    float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (sl > 0.0f) {
      sx /= sl;
      sy /= sl;
      sz /= sl;
    }
    const float ux2 = sy * fz - sz * fy, uy2 = sz * fx - sx * fz,
                uz2 = sx * fy - sy * fx;
    Mat4 o;
    o.m[0] = sx;
    o.m[1] = sy;
    o.m[2] = sz;
    o.m[3] = 0.0f;
    o.m[4] = ux2;
    o.m[5] = uy2;
    o.m[6] = uz2;
    o.m[7] = 0.0f;
    o.m[8] = -fx;
    o.m[9] = -fy;
    o.m[10] = -fz;
    o.m[11] = 0.0f;
    o.m[12] = -(sx * ex + sy * ey + sz * ez);
    o.m[13] = -(ux2 * ex + uy2 * ey + uz2 * ez);
    o.m[14] = (fx * ex + fy * ey + fz * ez);
    o.m[15] = 1.0f;
    return o;
  }

  static Mat4 rotate(float ang, float x, float y, float z) { // axis-angle
    float l = std::sqrt(x * x + y * y + z * z);
    if (l > 0.0f) {
      x /= l;
      y /= l;
      z /= l;
    }
    const float c = std::cos(ang), s = std::sin(ang), t = 1.0f - c;
    Mat4 o;
    o.m[0] = t * x * x + c;
    o.m[1] = t * x * y + s * z;
    o.m[2] = t * x * z - s * y;
    o.m[3] = 0.0f;
    o.m[4] = t * x * y - s * z;
    o.m[5] = t * y * y + c;
    o.m[6] = t * y * z + s * x;
    o.m[7] = 0.0f;
    o.m[8] = t * x * z + s * y;
    o.m[9] = t * y * z - s * x;
    o.m[10] = t * z * z + c;
    o.m[11] = 0.0f;
    o.m[12] = 0.0f;
    o.m[13] = 0.0f;
    o.m[14] = 0.0f;
    o.m[15] = 1.0f;
    return o;
  }
};

// ------------------------------------------------------------- shaders ---
// GLSL ES 1.00: `#ifdef GL_ES` guard first so the same source compiles on
// desktop GL (precision statements are ES-only) and on real ES drivers.
constexpr const char kPrecision[] =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n";

constexpr const char kOpaqueVS[] = R"GLSL(
attribute vec3 aPos;
attribute vec3 aNormal;
uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;
varying vec3 vN;
varying vec3 vW;
void main() {
  vec4 w = uModel * vec4(aPos, 1.0);
  vW = w.xyz;
  vN = mat3(uModel) * aNormal;
  gl_Position = uProj * uView * w;
}
)GLSL";

// PBR-lite matching three.js r150 params from buildLights(): warm key with
// PCF-ish shadowed key (shadows need a map pass — TODO, key treated as
// unshadowed here), cool moon rim, warm counter-rim, pink fill, lamp point
// (dist 12, decay 2), hemisphere bounce + boudoir ambient, distance fog,
// ACES-filmic approx + sRGB encode (renderer toneMapping/outputEncoding).
constexpr const char kOpaqueFS[] = R"GLSL(
varying vec3 vN;
varying vec3 vW;
uniform vec3 uCamPos;
uniform vec3 uBaseCol;
uniform vec3 uEmissive;
uniform float uSpecAmt;
uniform vec3 uKeyDir;  uniform vec3 uKeyCol;
uniform vec3 uRimDir;  uniform vec3 uRimCol;
uniform vec3 uCtrDir;  uniform vec3 uCtrCol;
uniform vec3 uFillDir; uniform vec3 uFillCol;
uniform vec3 uLampPos; uniform vec3 uLampCol;
uniform vec3 uHemiSky; uniform vec3 uHemiGround;
uniform vec3 uAmbient;
uniform vec3 uFogColor; uniform float uFogNear; uniform float uFogFar;
uniform float uExposure;
void main() {
  vec3 N = normalize(vN);
  vec3 V = normalize(uCamPos - vW);
  vec3 amb = mix(uHemiGround, uHemiSky, N.y * 0.5 + 0.5) + uAmbient;
  vec3 col = uBaseCol * (amb
    + uKeyCol  * max(dot(N, uKeyDir), 0.0)
    + uCtrCol  * max(dot(N, uCtrDir), 0.0)
    + uFillCol * max(dot(N, uFillDir), 0.0));
  float fres = pow(1.0 - abs(dot(N, V)), 3.0);
  float rimFace = clamp(dot(N, uRimDir) * 0.5 + 0.5, 0.0, 1.0);
  col += uRimCol * (fres * rimFace);
  vec3 H = normalize(uKeyDir + V);
  col += uKeyCol * (pow(max(dot(N, H), 0.0), 42.0) * uSpecAmt);
  vec3 Ld = uLampPos - vW;
  float dist = length(Ld);
  vec3 L = Ld / max(dist, 0.0001);
  float att = clamp(1.0 - dist / 12.0, 0.0, 1.0);
  att *= att;
  col += uBaseCol * uLampCol * (max(dot(N, L), 0.0) * att);
  col += uEmissive;
  float fd = length(uCamPos - vW);
  float f = clamp((fd - uFogNear) / max(uFogFar - uFogNear, 0.0001), 0.0, 1.0);
  f = f * f * (3.0 - 2.0 * f);
  col = mix(col, uFogColor, f);
  col *= uExposure;
  col = clamp((col * (col * 2.51 + vec3(0.03)))
    / (col * (col * 2.43 + vec3(0.59)) + vec3(0.14)), 0.0, 1.0);
  col = pow(col, vec3(0.4545));
  gl_FragColor = vec4(col, 1.0);
}
)GLSL";

// Additive fx bucket: vertex-colored unlit (fx3d.js hearts/dust/bloom
// sprites become tris/quads; depth-tested, no depth write — like the sheen
// shells / gloss sprites in engine3d.js).
constexpr const char kFxVS[] = R"GLSL(
attribute vec3 aPos;
attribute vec4 aCol;
uniform mat4 uMVP;
varying vec4 vC;
void main() {
  vC = aCol;
  gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

constexpr const char kFxFS[] = R"GLSL(
varying vec4 vC;
uniform float uGlobalA;
void main() {
  gl_FragColor = vec4(vC.rgb, vC.a * uGlobalA);
}
)GLSL";

// Inverted-hull ink outline (OUTLINE_MAT): extrude along normals, drawn
// with front-face culling by the future Cast pass. Warm dark brown lineart.
constexpr const char kOutlineVS[] = R"GLSL(
attribute vec3 aPos;
attribute vec3 aNormal;
uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;
uniform float uThick;
void main() {
  vec3 p = aPos + normalize(aNormal) * uThick;
  gl_Position = uProj * uView * uModel * vec4(p, 1.0);
}
)GLSL";

constexpr const char kOutlineFS[] = R"GLSL(
uniform vec3 uColor;
void main() {
  gl_FragColor = vec4(uColor, 1.0);
}
)GLSL";

// -------------------------------------------------------- js constants ---
// buildLights() params (hex sRGB + intensity), renderer clear/fog color.
constexpr float kNear = 0.05f, kFar = 120.0f; // PerspectiveCamera(42,16/9,…)
constexpr float kFogNear = 9.0f, kFogFar = 26.0f; // scene.fog(0x0d0709,9,26)
constexpr unsigned kBgHex = 0x0d0709u;
constexpr unsigned kAmbHex = 0x3a2030u;
constexpr float kAmbGain = 0.22f; // AmbientLight
constexpr unsigned kHemiSkyHex = 0xffd9c8u, kHemiGndHex = 0x2a1218u;
constexpr float kHemiGain = 0.35f; // HemisphereLight

struct LightDef { // DirectionalLight(color, intensity) at position
  float px, py, pz;
  unsigned hex;
  float gain;
};
constexpr LightDef kKey{ -5.0f, 3.4f, 0.8f, 0xffb072u, 1.15f };
constexpr LightDef kRim{ 5.5f, 3.2f, -4.5f, 0x8fb6ffu, 1.10f };
constexpr LightDef kCtr{ -5.0f, 2.6f, 3.8f, 0xff9d76u, 0.30f };
constexpr LightDef kFill{ 2.0f, 1.4f, 5.0f, 0xff8fa8u, 0.25f };
constexpr float kLampPx = -2.9f, kLampPy = 1.35f, kLampPz = -1.4f;
constexpr unsigned kLampHex = 0xffa860u;
constexpr float kLampGain = 1.1f; // PointLight(color, 1.1, distance 12, 2)

constexpr unsigned kOutlineHex = 0x3a1e26u; // OUTLINE_MAT warm brown
constexpr float kOutlineThick = 0.014f;

// Camera-rig clamps (updateCamKeys3 / wheel / drag handlers).
constexpr double kPitchMin = -0.25, kPitchMax = 1.28;
constexpr double kDistMin = 0.12, kDistMax = 16.0;
constexpr double kTxMin = -3.2, kTxMax = 3.2, kTzMin = -4.5, kTzMax = 4.5;
// resetCam3(): yaw 0.35, pitch 0.30, dist 5.4, target (0,0.55,0).
constexpr double kResetYaw = 0.35, kResetPitch = 0.30, kResetDist = 5.4;
constexpr double kResetTx = 0.0, kResetTy = 0.55, kResetTz = 0.0;

// ------------------------------------------------------------ locals ---
// State that engine3d.js keeps outside the camera struct (GL programs,
// smoothed _yaw/_pitch/_dist, CAMKEY3 flags, wheel queue, shake energy,
// viewport, cached matrices). One GL context => one shared copy.
struct CamKeys { // mirrors CAMKEY3 numeric flags (WASD + arrows)
  double f = 0, b = 0, l = 0, r = 0; // KeyW/S/A/D
  double yl = 0, yr = 0, pu = 0, pd = 0; // ArrowLeft/Right/Up/Down
};

d3gl::GLuint s_opaque = 0, s_fx = 0, s_outline = 0;

struct OpaqueLoc {
  d3gl::GLint proj = -1, view = -1, model = -1, camPos = -1;
  d3gl::GLint baseCol = -1, emissive = -1, specAmt = -1;
  d3gl::GLint keyDir = -1, keyCol = -1, rimDir = -1, rimCol = -1;
  d3gl::GLint ctrDir = -1, ctrCol = -1, fillDir = -1, fillCol = -1;
  d3gl::GLint lampPos = -1, lampCol = -1;
  d3gl::GLint hemiSky = -1, hemiGnd = -1, ambient = -1;
  d3gl::GLint fogCol = -1, fogNear = -1, fogFar = -1, exposure = -1;
};
struct FxLoc {
  d3gl::GLint mvp = -1, globalA = -1;
};
struct OutlineLoc { // matrices ride per-shell; thick/color are init-once.
  d3gl::GLint thick = -1, color = -1;
};
OpaqueLoc s_oLoc;
FxLoc s_fLoc;
OutlineLoc s_olLoc;

int s_vw = 1280, s_vh = 720;
int s_mode = 0; // 0 orbit | 1 side | 2 top (CAM3.mode minus fpv — see TODOs)
CamKeys s_keys;
double s_wheel = 0.0; // queued wheel notches (+ = zoom out, like deltaY>0)
double s_shake = 0.0; // CAM3.shake energy (G.shake/impact feed it later)
bool s_resetReq = false; // KeyR requested resetCam3()
double s_syaw = 0.0, s_spitch = 0.30, s_sdist = 5.4; // smoothed _yaw/_…/_dist
bool s_haveSmooth = false;
Mat4 s_view = Mat4::identity(), s_proj = Mat4::identity();
float s_eye[3] = {0.0f, 2.1f, 5.2f};
std::chrono::steady_clock::time_point s_last;
bool s_haveLast = false;
std::uint32_t s_rnd = 0x1337u; // deterministic shake jitter (parity-stable)

void hexToF(unsigned hex, float gain, float out[3]) {
  out[0] = (float)((hex >> 16) & 255u) * (1.0f / 255.0f) * gain;
  out[1] = (float)((hex >> 8) & 255u) * (1.0f / 255.0f) * gain;
  out[2] = (float)(hex & 255u) * (1.0f / 255.0f) * gain;
}

void lightDirCol(const LightDef& d, float dir[3], float col[3]) {
  const float l =
      std::sqrt(d.px * d.px + d.py * d.py + d.pz * d.pz); // from origin,
  if (l > 0.0f) { // three.js DirectionalLight default target
    dir[0] = d.px / l;
    dir[1] = d.py / l;
    dir[2] = d.pz / l;
  } else {
    dir[0] = 0.0f;
    dir[1] = 1.0f;
    dir[2] = 0.0f;
  }
  hexToF(d.hex, d.gain, col);
}

float shakeJitter() { // [0,1) LCG — JS uses Math.random; fixed seed keeps
  s_rnd = s_rnd * 1664525u + 1013904223u; // headless parity frames stable.
  return (float)(s_rnd >> 8) * (1.0f / 16777216.0f);
}

bool glReady() { // every entry point this TU touches (d3gl::load is
  return d3gl::CreateShader && d3gl::ShaderSource && // all-or-nothing, but
         d3gl::CompileShader && d3gl::GetShaderiv && // fail closed anyway.
         d3gl::GetShaderInfoLog && d3gl::DeleteShader &&
         d3gl::CreateProgram && d3gl::AttachShader && d3gl::LinkProgram &&
         d3gl::GetProgramiv && d3gl::GetProgramInfoLog &&
         d3gl::UseProgram && d3gl::DeleteProgram &&
         d3gl::GetUniformLocation && d3gl::UniformMatrix4fv &&
         d3gl::Uniform3fv && d3gl::Uniform1f && d3gl::Viewport &&
         d3gl::Clear && d3gl::ClearColor && d3gl::ClearDepthf &&
         d3gl::Enable && d3gl::Disable && d3gl::BlendFunc &&
         d3gl::DepthFunc && d3gl::DepthMask && d3gl::CullFace;
}

bool compileShader(d3gl::GLenum type, const char* name, const char* body,
                   d3gl::GLuint& out) {
  out = 0;
  const d3gl::GLuint s = d3gl::CreateShader(type);
  if (s == 0) {
    std::fprintf(stderr, "[d3] CreateShader failed (%s)\n", name);
    return false;
  }
  const d3gl::GLchar* parts[2] = {kPrecision, body};
  d3gl::ShaderSource(s, 2, parts, nullptr);
  d3gl::CompileShader(s);
  d3gl::GLint ok = 0;
  d3gl::GetShaderiv(s, d3gl::COMPILE_STATUS, &ok);
  if (ok == 0) {
    char log[2048];
    d3gl::GLsizei n = 0;
    d3gl::GetShaderInfoLog(s, (d3gl::GLsizei)(sizeof(log) - 1), &n, log);
    log[sizeof(log) - 1] = '\0';
    if (n < 0) n = 0;
    std::fprintf(stderr, "[d3] shader compile failed (%s):\n%.*s\n", name,
                 (int)n, log);
    d3gl::DeleteShader(s);
    return false;
  }
  out = s;
  return true;
}

bool linkProgram(const char* name, d3gl::GLuint vs, d3gl::GLuint fs,
                 d3gl::GLuint& out) {
  out = 0;
  const d3gl::GLuint p = d3gl::CreateProgram();
  if (p == 0) {
    std::fprintf(stderr, "[d3] CreateProgram failed (%s)\n", name);
    return false;
  }
  d3gl::AttachShader(p, vs);
  d3gl::AttachShader(p, fs);
  d3gl::LinkProgram(p);
  d3gl::GLint ok = 0;
  d3gl::GetProgramiv(p, d3gl::LINK_STATUS, &ok);
  if (ok == 0) {
    char log[2048];
    d3gl::GLsizei n = 0;
    d3gl::GetProgramInfoLog(p, (d3gl::GLsizei)(sizeof(log) - 1), &n, log);
    log[sizeof(log) - 1] = '\0';
    if (n < 0) n = 0;
    std::fprintf(stderr, "[d3] program link failed (%s):\n%.*s\n", name,
                 (int)n, log);
    d3gl::DeleteProgram(p);
    return false;
  }
  out = p;
  return true;
}

void queryOpaqueLocs() {
  const d3gl::GLuint p = s_opaque;
  s_oLoc.proj = d3gl::GetUniformLocation(p, "uProj");
  s_oLoc.view = d3gl::GetUniformLocation(p, "uView");
  s_oLoc.model = d3gl::GetUniformLocation(p, "uModel");
  s_oLoc.camPos = d3gl::GetUniformLocation(p, "uCamPos");
  s_oLoc.baseCol = d3gl::GetUniformLocation(p, "uBaseCol");
  s_oLoc.emissive = d3gl::GetUniformLocation(p, "uEmissive");
  s_oLoc.specAmt = d3gl::GetUniformLocation(p, "uSpecAmt");
  s_oLoc.keyDir = d3gl::GetUniformLocation(p, "uKeyDir");
  s_oLoc.keyCol = d3gl::GetUniformLocation(p, "uKeyCol");
  s_oLoc.rimDir = d3gl::GetUniformLocation(p, "uRimDir");
  s_oLoc.rimCol = d3gl::GetUniformLocation(p, "uRimCol");
  s_oLoc.ctrDir = d3gl::GetUniformLocation(p, "uCtrDir");
  s_oLoc.ctrCol = d3gl::GetUniformLocation(p, "uCtrCol");
  s_oLoc.fillDir = d3gl::GetUniformLocation(p, "uFillDir");
  s_oLoc.fillCol = d3gl::GetUniformLocation(p, "uFillCol");
  s_oLoc.lampPos = d3gl::GetUniformLocation(p, "uLampPos");
  s_oLoc.lampCol = d3gl::GetUniformLocation(p, "uLampCol");
  s_oLoc.hemiSky = d3gl::GetUniformLocation(p, "uHemiSky");
  s_oLoc.hemiGnd = d3gl::GetUniformLocation(p, "uHemiGround");
  s_oLoc.ambient = d3gl::GetUniformLocation(p, "uAmbient");
  s_oLoc.fogCol = d3gl::GetUniformLocation(p, "uFogColor");
  s_oLoc.fogNear = d3gl::GetUniformLocation(p, "uFogNear");
  s_oLoc.fogFar = d3gl::GetUniformLocation(p, "uFogFar");
  s_oLoc.exposure = d3gl::GetUniformLocation(p, "uExposure");
}

void queryFxLocs() {
  s_fLoc.mvp = d3gl::GetUniformLocation(s_fx, "uMVP");
  s_fLoc.globalA = d3gl::GetUniformLocation(s_fx, "uGlobalA");
}

void uploadMat(d3gl::GLint loc, const Mat4& m) {
  if (loc >= 0)
    d3gl::UniformMatrix4fv(loc, 1, static_cast<d3gl::GLboolean>(0), m.m);
}
void upload3(d3gl::GLint loc, const float v[3]) {
  if (loc >= 0) d3gl::Uniform3fv(loc, 1, v);
}
void upload1(d3gl::GLint loc, float v) {
  if (loc >= 0) d3gl::Uniform1f(loc, v);
}

void zoomStep(Camera& cam, double dir) { // wheel handler: adaptive step so
  const double st = cam.dist < 0.8   ? 0.035 // macro stays precise while
                    : cam.dist < 2.2 ? 0.10 // wide shots still move fast.
                    : cam.dist < 4.5 ? 0.25
                                     : 0.45;
  cam.dist = ag::clamp(cam.dist + dir * st, kDistMin, kDistMax);
}

// Per-frame camera integration: updateCamKeys3 + updateCamera3 (orbit
// branch) + resetCam3. dt comes from a local steady clock because
// begin/endFrame take no dt (App3d::frame owns the real dt — see TODOs).
void stepCamera(Camera& cam, double dt) {
  if (s_resetReq) {
    s_resetReq = false;
    cam.yaw = kResetYaw;
    cam.pitch = kResetPitch;
    cam.dist = kResetDist;
    cam.tx = kResetTx;
    cam.ty = kResetTy;
    cam.tz = kResetTz;
    s_shake = 0.0;
  }
  if ((s_keys.f != 0) || (s_keys.b != 0) || (s_keys.l != 0) ||
      (s_keys.r != 0) || (s_keys.yl != 0) || (s_keys.yr != 0) ||
      (s_keys.pu != 0) || (s_keys.pd != 0)) {
    cam.yaw -= (s_keys.yl - s_keys.yr) * 1.5 * dt;
    cam.pitch =
        ag::clamp(cam.pitch + (s_keys.pu - s_keys.pd) * 1.1 * dt, kPitchMin,
                  kPitchMax);
    const double sp = 2.2 * dt; // WASD slides the focus point.
    const double sy = std::sin(cam.yaw), cy = std::cos(cam.yaw);
    const double mv = s_keys.f - s_keys.b, st2 = s_keys.r - s_keys.l;
    cam.tx += ((-sy) * mv + cy * st2) * sp; // forward=(-sy,-cy) right=(cy,-sy)
    cam.tz += ((-cy) * mv + (-sy) * st2) * sp;
    cam.tx = ag::clamp(cam.tx, kTxMin, kTxMax);
    cam.tz = ag::clamp(cam.tz, kTzMin, kTzMax);
  }
  while (s_wheel >= 1.0) {
    zoomStep(cam, 1.0);
    s_wheel -= 1.0;
  }
  while (s_wheel <= -1.0) {
    zoomStep(cam, -1.0);
    s_wheel += 1.0;
  }

  double wantYaw = cam.yaw, wantPitch = cam.pitch, wantDist = cam.dist;
  if (s_mode == 1) { // side: fixed profile rig
    wantYaw = ag::TAU * 0.25;
    wantPitch = 0.10;
    wantDist = 5.6;
  } else if (s_mode == 2) { // top: near-overhead rig
    wantYaw = 0.18;
    wantPitch = 0.95;
    wantDist = 4.6;
  }
  if (!s_haveSmooth) { // snap on first frame — mode switches glide after.
    s_syaw = wantYaw;
    s_spitch = wantPitch;
    s_sdist = wantDist;
    s_haveSmooth = true;
  }
  const double k = 1.0 - std::pow(0.0016, dt); // frame-rate independent lerp
  s_syaw = ag::lerp(s_syaw, wantYaw, k);
  s_spitch = ag::lerp(s_spitch, wantPitch, k);
  s_sdist = ag::lerp(s_sdist, wantDist, k);

  const double cp = std::cos(s_spitch);
  double x = cam.tx + std::sin(s_syaw) * cp * s_sdist;
  double y = cam.ty + std::sin(s_spitch) * s_sdist;
  double z = cam.tz + std::cos(s_syaw) * cp * s_sdist;
  if (s_shake > 0.0005) { // impact / climax shake, then exponential decay.
    const double s = s_shake;
    x += (shakeJitter() - 0.5) * s;
    y += (shakeJitter() - 0.5) * s;
    z += (shakeJitter() - 0.5) * s;
    s_shake *= std::pow(0.0001, dt);
    if (s_shake < 0.0005) s_shake = 0.0;
  }
  s_eye[0] = (float)x;
  s_eye[1] = (float)y;
  s_eye[2] = (float)z;
  s_view = Mat4::lookAt(s_eye[0], s_eye[1], s_eye[2], (float)cam.tx,
                        (float)cam.ty, (float)cam.tz, 0.0f, 1.0f, 0.0f);
  const float aspect =
      (s_vw > 0 && s_vh > 0) ? (float)s_vw / (float)s_vh : 16.0f / 9.0f;
  const float fovY =
      (float)(cam.fov * (ag::TAU / 360.0)); // degrees -> radians
  s_proj = Mat4::perspective(fovY, aspect, kNear, kFar);
}

void applyOpaquePass(const Camera& cam) {
  float bg[3];
  hexToF(kBgHex, 1.0f, bg);
  d3gl::Viewport(0, 0, s_vw, s_vh);
  d3gl::ClearColor(bg[0], bg[1], bg[2], 1.0f);
  d3gl::ClearDepthf(1.0f);
  d3gl::Clear(d3gl::COLOR_BUFFER_BIT | d3gl::DEPTH_BUFFER_BIT);
  // Depth-tested opaque bucket: write on, blend off, backface-culled
  // (three.js FrontSide default). Outline shells flip to FRONT later.
  d3gl::Enable(d3gl::DEPTH_TEST);
  d3gl::DepthFunc(d3gl::LEQUAL);
  d3gl::DepthMask(1);
  d3gl::Disable(d3gl::BLEND);
  d3gl::Enable(d3gl::CULL_FACE);
  d3gl::CullFace(d3gl::BACK);
  d3gl::UseProgram(s_opaque);

  uploadMat(s_oLoc.proj, s_proj);
  uploadMat(s_oLoc.view, s_view);
  uploadMat(s_oLoc.model, Mat4::identity());
  upload3(s_oLoc.camPos, s_eye);
  const float white[3] = {1.0f, 1.0f, 1.0f}; // Model port sets per-mesh.
  const float black[3] = {0.0f, 0.0f, 0.0f};
  upload3(s_oLoc.baseCol, white);
  upload3(s_oLoc.emissive, black);
  upload1(s_oLoc.specAmt, 0.35f); // skin clearcoat-ish default (toonMat)
  float dir[3], col[3];
  lightDirCol(kKey, dir, col);
  upload3(s_oLoc.keyDir, dir);
  upload3(s_oLoc.keyCol, col);
  lightDirCol(kRim, dir, col);
  upload3(s_oLoc.rimDir, dir);
  upload3(s_oLoc.rimCol, col);
  lightDirCol(kCtr, dir, col);
  upload3(s_oLoc.ctrDir, dir);
  upload3(s_oLoc.ctrCol, col);
  lightDirCol(kFill, dir, col);
  upload3(s_oLoc.fillDir, dir);
  upload3(s_oLoc.fillCol, col);
  const float lampP[3] = {kLampPx, kLampPy, kLampPz};
  upload3(s_oLoc.lampPos, lampP);
  hexToF(kLampHex, kLampGain, col);
  upload3(s_oLoc.lampCol, col);
  hexToF(kHemiSkyHex, kHemiGain, col);
  upload3(s_oLoc.hemiSky, col);
  hexToF(kHemiGndHex, kHemiGain, col);
  upload3(s_oLoc.hemiGnd, col);
  hexToF(kAmbHex, kAmbGain, col);
  upload3(s_oLoc.ambient, col);
  float fog[3];
  hexToF(kBgHex, 1.0f, fog);
  upload3(s_oLoc.fogCol, fog);
  upload1(s_oLoc.fogNear, kFogNear);
  upload1(s_oLoc.fogFar, kFogFar);
  upload1(s_oLoc.exposure, 1.0f); // toneMappingExposure
  (void)cam;
}

void applyFxPass() {
  // Additive fx bucket (sheen/gloss/fx3d sprites): depth-tested so quads
  // hide behind bodies, but never write depth; SRC_ALPHA,ONE additive.
  d3gl::Enable(d3gl::DEPTH_TEST);
  d3gl::DepthFunc(d3gl::LEQUAL);
  d3gl::DepthMask(0);
  d3gl::Enable(d3gl::BLEND);
  d3gl::BlendFunc(d3gl::SRC_ALPHA, d3gl::ONE_);
  d3gl::UseProgram(s_fx);
  uploadMat(s_fLoc.mvp, Mat4::multiply(s_proj, s_view)); // model = identity
  upload1(s_fLoc.globalA, 1.0f);
}

void destroyPrograms() {
  if (d3gl::UseProgram) d3gl::UseProgram(0);
  if (d3gl::DeleteProgram) {
    if (s_opaque != 0) d3gl::DeleteProgram(s_opaque);
    if (s_fx != 0) d3gl::DeleteProgram(s_fx);
    if (s_outline != 0) d3gl::DeleteProgram(s_outline);
  }
  s_opaque = s_fx = s_outline = 0;
  s_oLoc = OpaqueLoc{};
  s_fLoc = FxLoc{};
  s_olLoc = OutlineLoc{};
}

} // namespace

// TODO(eng-input): platform input has no hook yet. These extern-linkage
// helpers are the future wiring points (declare them in ag_3d.h once
// headers unfreeze); until then the rig simply idles at its defaults:
//   d3SetCamMode(0 orbit | 1 side | 2 top)  <- btnCamOrbit/Side/Top
//   d3PushKey(code, down) code: 0 W,1 S,2 A,3 D,4 Left,5 Right,6 Up,7 Down
//     (down: KeyR -> request R reset; KeyL look-toggle needs input.js)
//   d3PushWheel(notches)  <- CV3 wheel (deltaY>0 = zoom out)
//   d3AddShake(e)         <- G.shake / impact / climax kick
// Drag-orbit (0.006 rad/px, fpv 0.0032) and FPV look offsets ride on the
// platform pointer events; same path as d3PushKey once SDL input lands.
void d3SetCamMode(int mode) {
  s_mode = (mode == 1) ? 1 : ((mode == 2) ? 2 : 0);
}
void d3PushKey(int code, bool down) {
  const double v = down ? 1.0 : 0.0;
  switch (code) {
  case 0:
    s_keys.f = v;
    break;
  case 1:
    s_keys.b = v;
    break;
  case 2:
    s_keys.l = v;
    break;
  case 3:
    s_keys.r = v;
    break;
  case 4:
    s_keys.yl = v;
    break;
  case 5:
    s_keys.yr = v;
    break;
  case 6:
    s_keys.pu = v;
    break;
  case 7:
    s_keys.pd = v;
    break;
  case 8: // KeyR
    if (down) s_resetReq = true;
    break;
  default:
    break;
  }
}
void d3PushWheel(double notches) { s_wheel += notches; }
void d3AddShake(double e) {
  s_shake += e;
  if (s_shake > 1.5) s_shake = 1.5;
}
// Mid-frame switch for the future Fx::draw (opaque draws done, additive
// sprites next). Leaves depth test ON, depth write OFF, fx program bound.
void d3FxPass() {
  if (s_fx == 0) return;
  applyFxPass();
}

bool Engine::init(int w, int h) {
  if (ready) {
    resize(w, h);
    return true;
  }
  if (!glReady()) {
    std::fprintf(stderr, "[d3] init: GL entry points not loaded\n");
    return false;
  }
  d3gl::GLuint vs = 0, fs = 0;
  // Opaque PBR-lite program.
  bool ok = compileShader(d3gl::VERTEX_SHADER, "opaque-vs", kOpaqueVS, vs);
  if (ok) ok = compileShader(d3gl::FRAGMENT_SHADER, "opaque-fs", kOpaqueFS, fs);
  if (ok) ok = linkProgram("opaque", vs, fs, s_opaque);
  if (vs != 0) d3gl::DeleteShader(vs);
  if (fs != 0) d3gl::DeleteShader(fs);
  if (!ok) {
    destroyPrograms();
    return false;
  }
  // Additive fx program.
  vs = fs = 0;
  ok = compileShader(d3gl::VERTEX_SHADER, "fx-vs", kFxVS, vs);
  if (ok) ok = compileShader(d3gl::FRAGMENT_SHADER, "fx-fs", kFxFS, fs);
  if (ok) ok = linkProgram("fx", vs, fs, s_fx);
  if (vs != 0) d3gl::DeleteShader(vs);
  if (fs != 0) d3gl::DeleteShader(fs);
  if (!ok) {
    destroyPrograms();
    return false;
  }
  // Ink-outline program.
  vs = fs = 0;
  ok = compileShader(d3gl::VERTEX_SHADER, "outline-vs", kOutlineVS, vs);
  if (ok)
    ok = compileShader(d3gl::FRAGMENT_SHADER, "outline-fs", kOutlineFS, fs);
  if (ok) ok = linkProgram("outline", vs, fs, s_outline);
  if (vs != 0) d3gl::DeleteShader(vs);
  if (fs != 0) d3gl::DeleteShader(fs);
  if (!ok) {
    destroyPrograms();
    return false;
  }
  queryOpaqueLocs();
  queryFxLocs();
  // Outline thick/color are rig-wide constants (updateInk3 rescales per
  // shell later); matrices stay per-draw for the future Cast pass, which
  // binds this program with CullFace(FRONT) for the inverted hull.
  s_olLoc.thick = d3gl::GetUniformLocation(s_outline, "uThick");
  s_olLoc.color = d3gl::GetUniformLocation(s_outline, "uColor");
  d3gl::UseProgram(s_outline);
  upload1(s_olLoc.thick, kOutlineThick);
  float ink[3];
  hexToF(kOutlineHex, 1.0f, ink);
  upload3(s_olLoc.color, ink);
  d3gl::UseProgram(0);
  if (w > 0) s_vw = w;
  if (h > 0) s_vh = h;
  s_syaw = cam.yaw;
  s_spitch = cam.pitch;
  s_sdist = cam.dist;
  s_haveSmooth = true;
  s_haveLast = false;
  ready = true;
  return true;
}

void Engine::resize(int w, int h) {
  if (w > 0) s_vw = w;
  if (h > 0) s_vh = h;
  // Viewport + aspect are consumed per-frame in beginFrame() (resize3()
  // likewise just stores size; camera3d.aspect updates on next render).
}

void Engine::shutdown() {
  destroyPrograms();
  s_keys = CamKeys{};
  s_haveSmooth = false;
  s_haveLast = false;
  s_shake = 0.0;
  s_wheel = 0.0;
  s_resetReq = false;
  ready = false;
}

void Engine::beginFrame() {
  if (!ready || s_opaque == 0) return;
  const auto now = std::chrono::steady_clock::now();
  double dt = 1.0 / 60.0;
  if (s_haveLast)
    dt = std::chrono::duration<double>(now - s_last).count();
  s_last = now;
  s_haveLast = true;
  if (dt < 0.0001) dt = 0.0001;
  if (dt > 0.1) dt = 0.1;
  stepCamera(cam, dt);
  applyOpaquePass(cam);
}

void Engine::endFrame() {
  if (!ready || s_fx == 0) return;
  // Open the fx bucket (additive, depth-tested, no depth write). The next
  // beginFrame() restores opaque state; the platform presents on its own
  // (d3gl has no swap entry point — context is platform-owned).
  applyFxPass();
}

} // namespace ag::d3
