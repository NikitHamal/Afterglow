// Afterglow native — ag_d3_animfx.cpp
// Port of js3d/anim3d.js (updateAnim3 + updateSolo3, 921 lines) curves and
// js3d/fx3d.js (296 lines) spawn-rate/bloom logic into conservative C++20.
//
// SCOPE: new file only (per task rules). All helpers + smoothed state are
// file-local (anonymous namespace). No header changes, no wall clock
// (<cmath> + g.t only), no RNG (deterministic; audio/RNG hooks are TODOs).
//
// SIM OWNERSHIP: sprite data is sourced from the SAME Game vectors the 2D
// build uses (jets/sweat/hearts/drips/glisten in ag_core.h), so the 2D
// mechanics sim stays shared. This file never spawns into Game vectors
// (Fx::update takes `const Game&`); it only mirrors fx3d.js edge/rate
// constants for flash + bloom smoothing and renders the shared vectors as
// billboarded additive point sprites via d3gl:: (tiny file-local shader).
//
// NUMERIC ORDER: animUpdate preserves the updateAnim3 section order:
//   pose-blend detect -> breathing -> rub ownership -> thrust -> shaft ->
//   chest breath scales -> jiggle springs -> soft tissue -> hair -> rub
//   knead orbits -> oral -> orgasm -> after -> solo sway -> solo moan timer.
// Constants are byte-for-byte the JS values (see inline js: citations).

#include "ag_3d.h"
#include "ag_d3_gl.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ag::d3 {
namespace {

// ---- file-local math (1:1 with anim3d.js dampNum3 / springStep) ----
constexpr double PI_D = 3.141592653589793;

inline double dampNum(double cur, double tgt, double rate, double dt) {
  if (!std::isfinite(cur)) cur = tgt;
  if (!std::isfinite(tgt)) return cur;
  if (dt <= 0.0) return cur; // js: 1-exp(0) == 0 -> unchanged
  return cur + (tgt - cur) * (1.0 - std::exp(-rate * dt));
}

struct Spring1 {
  double x = 0.0, v = 0.0;
};
inline double springStep(Spring1& s, double target, double k, double c,
                         double dt) {
  if (dt <= 0.0) return s.x; // js: zero integration step
  s.v += (-k * (s.x - target) - c * s.v) * dt;
  s.x += s.v * dt;
  return s.x;
}

// ---- file-local smoothed animation state ----
// (Rig-blend structs _smPose3 / joint quaternions / world anchors like
// vulvaWorld3 / breastWorld3 / shaftTipWorld3 have no native home yet:
// Model in ag_3d.h exposes only load/pose/draw. These scalars hold the
// already-damped additive layer so Model::pose can consume them once the
// header gains bone fields. See TODO(1).)
struct AnimBones {
  double thrustY = 0.0, thrustZ = 0.0; // js: _thrust3.y/z
  double breath = 0.0, breathRate = 0.16, breathAmt = 2.4;
  double chestHerX = 1.0, chestHerY = 1.0, chestHerZ = 1.0;
  double chestHimX = 1.0, chestHimY = 1.0, chestHimZ = 1.0;
  double shaftPulseSm = 0.0, shaftScale = 1.0, glansScale = 1.0;
  double shaftRotX = -1.4507963267948965; // -PI/2+0.12, ar term added per-frame
  double emisR = 0.0, emisG = 0.0, emisB = 0.0;
  double jigBreast = 0.0, jigButt = 0.0, jigClamped = 0.0;
  double drive = 0.0, squash = 0.0;
  double hairX = 0.0, hairZ = 0.0;
  double knead = 0.0, knead2 = 0.0;
  double clitOx = 0.0, clitOy = 0.0, clitOz = 0.0;
  double breastBobX = 0.0, breastBobY = 0.0;
  double oralBob = 0.0, oralSp = 0.0, gagWob = 0.0;
  double orgasmE = 0.0, tremor = 0.0, afterS = 0.0;
  double soloSway = 0.0, soloSway2 = 0.0;
  double poseEase = 1.0;
  bool isCowgirl = false, isBlow = false, rubActive = false;
};

Spring1 s_breastJig, s_buttJig;
struct HairS {
  double x = 0.0, v = 0.0, z = 0.0, vz = 0.0;
};
HairS s_hair;
double s_thrustY = 0.0, s_thrustZ = 0.0, s_shaftP = 0.0;
double s_poseBlend = 1.0, s_poseEase = 1.0;
int s_lastPos = -1, s_lastOral = 0;
bool s_lastSolo = false;
AnimBones s_bones;

// ---- file-local FX mirror state (fx3d.js:122-124) ----
double s_lastSpurtsFx = 0.0;
double s_flash = 0.0;    // one-shot white flash (flash3(0.42)), linear decay
double s_bloomSm = 0.0;  // smoothed bloom overlay opacity
double s_dripAcc = 0.0, s_sweatAcc = 0.0, s_glisAcc = 0.0, s_heartAcc = 0.0;

// ---- file-local point-sprite GL state ----
struct FxVert {
  float x, y;          // clip-space position (-1..1, y-up)
  float size;          // gl_PointSize in px
  float alpha;         // per-sprite alpha multiplier
  float r, g, b;       // sprite tint
};

constexpr int kFxMax = 768;
std::vector<FxVert> s_batch; // reserved to kFxMax on first draw (no hot alloc)

d3gl::GLuint s_fxProg = 0, s_fxVbo = 0;
d3gl::GLint s_aPos = -1, s_aSize = -1, s_aAlpha = -1, s_aColor = -1;
bool s_fxTried = false, s_fxReady = false;

constexpr const char* kFxVs =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "attribute vec2 aPos;\n"
    "attribute float aSize;\n"
    "attribute float aAlpha;\n"
    "attribute vec3 aColor;\n"
    "varying float vA;\n"
    "varying vec3 vC;\n"
    "void main(){\n"
    "  vA = aAlpha;\n"
    "  vC = aColor;\n"
    "  gl_PointSize = aSize;\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

constexpr const char* kFxFs =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying float vA;\n"
    "varying vec3 vC;\n"
    "void main(){\n"
    "  vec2 d = gl_PointCoord - vec2(0.5);\n"
    "  float r2 = dot(d, d) * 4.0;\n"
    "  float soft = exp(-r2 * 3.0);\n" // js makeSprite3 'wet'/'soft' bead approx
    "  float a = soft * vA;\n"
    "  if (a < 0.01) discard;\n"
    "  gl_FragColor = vec4(vC, a);\n"
    "}\n";

d3gl::GLuint compileOne(d3gl::GLenum type, const char* src) {
  if (!d3gl::CreateShader || !d3gl::ShaderSource || !d3gl::CompileShader ||
      !d3gl::GetShaderiv || !d3gl::DeleteShader)
    return 0;
  d3gl::GLuint s = d3gl::CreateShader(type);
  if (s == 0) return 0;
  d3gl::ShaderSource(s, 1, &src, nullptr);
  d3gl::CompileShader(s);
  d3gl::GLint ok = 0;
  d3gl::GetShaderiv(s, d3gl::COMPILE_STATUS, &ok);
  if (ok == 0) {
    d3gl::DeleteShader(s);
    return 0;
  }
  return s;
}

bool ensureFx() {
  if (s_fxReady) return true;
  if (s_fxTried) return false;
  s_fxTried = true;
  if (!d3gl::CreateProgram || !d3gl::AttachShader || !d3gl::LinkProgram ||
      !d3gl::GetProgramiv || !d3gl::UseProgram || !d3gl::GetAttribLocation ||
      !d3gl::GenBuffers || !d3gl::BindBuffer || !d3gl::BufferData ||
      !d3gl::DeleteShader || !d3gl::DeleteProgram)
    return false;
  d3gl::GLuint vs = compileOne(d3gl::VERTEX_SHADER, kFxVs);
  if (vs == 0) return false;
  d3gl::GLuint fs = compileOne(d3gl::FRAGMENT_SHADER, kFxFs);
  if (fs == 0) {
    d3gl::DeleteShader(vs);
    return false;
  }
  d3gl::GLuint pr = d3gl::CreateProgram();
  if (pr == 0) {
    d3gl::DeleteShader(vs);
    d3gl::DeleteShader(fs);
    return false;
  }
  d3gl::AttachShader(pr, vs);
  d3gl::AttachShader(pr, fs);
  d3gl::LinkProgram(pr);
  d3gl::DeleteShader(vs);
  d3gl::DeleteShader(fs);
  d3gl::GLint ok = 0;
  d3gl::GetProgramiv(pr, d3gl::LINK_STATUS, &ok);
  if (ok == 0) {
    d3gl::DeleteProgram(pr);
    return false;
  }
  d3gl::GLint pPos = d3gl::GetAttribLocation(pr, "aPos");
  d3gl::GLint pSize = d3gl::GetAttribLocation(pr, "aSize");
  d3gl::GLint pAlpha = d3gl::GetAttribLocation(pr, "aAlpha");
  d3gl::GLint pCol = d3gl::GetAttribLocation(pr, "aColor");
  if (pPos < 0 || pSize < 0 || pAlpha < 0 || pCol < 0) {
    d3gl::DeleteProgram(pr);
    return false;
  }
  d3gl::GLuint vb = 0;
  d3gl::GenBuffers(1, &vb);
  if (vb == 0) {
    d3gl::DeleteProgram(pr);
    return false;
  }
  s_fxProg = pr;
  s_fxVbo = vb;
  s_aPos = pPos;
  s_aSize = pSize;
  s_aAlpha = pAlpha;
  s_aColor = pCol;
  s_batch.reserve((std::size_t)kFxMax);
  s_fxReady = true;
  return true;
}

inline float toClipX(double x) {
  return (float)(x / 1280.0 * 2.0 - 1.0); // js virtual W=1280
}
inline float toClipY(double y) {
  return (float)(1.0 - y / 720.0 * 2.0); // js virtual H=720, y-down -> y-up
}
inline void pushSprite(float cx, float cy, float size, float alpha, float r,
                       float g, float b) {
  if ((int)s_batch.size() >= kFxMax) return;
  if (!(alpha > 0.004f)) return;
  if (!(size > 0.5f)) return;
  // Clamp to clip rect with a small margin so offscreen 2D spawns (e.g.
  // ballistic jets arcing out of frame) do not generate huge points.
  if (cx < -1.2f || cx > 1.2f || cy < -1.2f || cy > 1.2f) return;
  if (size > 480.0f) size = 480.0f;
  s_batch.push_back(FxVert{cx, cy, size, alpha, r, g, b});
}

} // namespace
// ---- animation update (ported from updateAnim3 + updateSolo3) ----
void animUpdate(Game& g, double dt) {
  // Declared here (no header change per new-file-only rule).
  // Uses g.t only; never wall clock. dt<=0 integrates nothing (JS parity).
  if (dt < 0.0) dt = 0.0;
  const double t = g.t;
  const double depth = clamp(g.depth, 0.0, 1.0);      // js:549-552
  const double oral = clamp(g.oral, 0.0, 1.0);        //
  const double p = clamp(g.pleasure / 100.0, 0.0, 1.0); //
  const double ar = clamp(g.ar / 100.0, 0.0, 1.0);    // js:647 moisture

  // ---- pose-change detect + blend (js:557-579) ----
  // Missing G.oralMode (string) -> derive blow flag from g.oralT==2, which
  // is the second half of the JS condition (G.oralMode==='blow'||G.oralT===2).
  int posIdx = g.pos;
  if (posIdx < 0) posIdx = 0;
  if (posIdx > 6) posIdx = 6;
  const bool isBlow = (g.oralT == 2.0);
  const int oralNow = (oral > 0.03) ? (isBlow ? 2 : 1) : 0;
  const bool soloNow = g.solo;
  if (posIdx != s_lastPos || oralNow != s_lastOral || soloNow != s_lastSolo) {
    s_lastPos = posIdx;
    s_lastOral = oralNow;
    s_lastSolo = soloNow;
    s_poseBlend = 0.0;
  }
  if (s_poseBlend < 1.0 && dt > 0.0) {
    s_poseBlend = std::min(1.0, s_poseBlend + dt * 2.6); // js:573 rate 2.6
  }
  const double pb = clamp(s_poseBlend, 0.0, 1.0);
  s_poseEase = pb * pb * (3.0 - 2.0 * pb); // js:574 smootherstep
  // js:581 k = 1-pow(0.0009,dt) feeds applyRig FK easing only; no rig here.
  (void)(1.0 - std::pow(0.0009, dt > 0.0 ? dt : 0.0));

  // ---- breathing (js:584-586) ----
  const double brRate = 0.16 + p * 0.0045;
  const double brAmt = 2.4 + p * 1.6;
  const double br = std::sin(t * TAU * brRate) * (brAmt / 260.0);

  // ---- rub ownership (js:597-613): keys only; arm IK lives in Model ----
  const double rub = clamp(g.rub, 0.0, 1.0);
  const int zone = g.rubZone; // js:598 zone selects which arm(s) rub
  const bool spaceHeld = (g.spaceHeld != 0.0);
  const bool rubActive = (rub > 0.03 || (g.solo && spaceHeld)) &&
                         g.state != "climax" && g.state != "finish";
  const double rubAmt =
      (g.solo && spaceHeld) ? std::max(rub, 0.85) : rub; // js:721
  // js:603-612 ownership mirror (no rig here; gates orbit outputs below):
  // solo zone3=both(clit+breast) 1=herL 2=herR 0=both-breast;
  // dual zone3/1=himR 2=himL 0=both. Only the solo-zone-3 clit orbit exists.
  const bool clitOrbit =
      rubActive && g.solo && zone == 3; // js:727-733 solo masturbation
  const bool breastOrbit = rubActive; // knead applies in every branch

  // ---- thrust (js:637-641): damped stroke drive ----
  const bool isCowgirl = (oral <= 0.03) && (posIdx == 4 || posIdx == 5);
  const double tgtThrustY = isCowgirl ? (1.0 - depth) * 0.11 : 0.0;
  const double tgtThrustZ =
      (!isCowgirl && oral <= 0.03) ? (1.0 - depth) * 0.11 : 0.0;
  s_thrustY = dampNum(s_thrustY, tgtThrustY, 8.0, dt); // js:638-639 rate 8
  s_thrustZ = dampNum(s_thrustZ, tgtThrustZ, 8.0, dt);

  // ---- shaft (js:647-658): smoothed pulse + arousal swell ----
  // Missing per-pose shaft angle (smHim.shaft from poses3d) -> file-local
  // fallback -PI/2+0.12-ar*0.10 (the JS default branch, js:651-652).
  s_shaftP = dampNum(s_shaftP, g.shaftPulse, 7.0, dt); // js:648 rate 7
  const double pulse = 1.0 + 0.34 * s_shaftP;          // js:649
  const double shaftRotX = -PI_D / 2.0 + 0.12 - ar * 0.10;
  const double glansScale = pulse * (1.0 + 0.12 * s_shaftP); // js:654

  // ---- breathing chest scales (js:661-662) ----
  const double herCX = 1.0 + br * 1.5, herCY = 1.0 + br * 2.2,
               herCZ = 1.0 + br * 1.8;
  const double himCX = 1.0 + br * 1.0, himCY = 1.0 + br * 1.4,
               himCZ = 1.0 + br * 1.2;

  // ---- breast + buttock jiggle springs (js:665-673) ----
  const double drive = g.impact * 0.9 + std::fabs(g.vel) * 0.06; // js:665
  const double bt = springStep(s_breastJig, drive, 190.0, 15.0, dt); // js:666
  const double bt2 =
      springStep(s_buttJig, drive * 0.85, 165.0, 14.0, dt); // js:667
  const double bs = clamp(bt, -0.4, 0.4);                   // js:668

  // ---- soft-tissue compression (js:677-686) ----
  // Missing live joint angles (no rig yet) -> flex terms stay 0, so joint
  // scales stay 1 and only the torso dent (sq from drive) evaluates.
  const double sq = clamp(drive, 0.0, 0.6); // js:685

  // ---- hair inertia springs (js:690-712, k70/c13) ----
  // Missing head world quaternion -> angular-velocity feedthrough is 0;
  // idle sway + thrust-lag + nod terms preserved exactly.
  const double htx =
      std::sin(t * 2.2) * 0.02 - bs * 0.10; // js:704 (wvx term = 0)
  const double htz =
      std::sin(t * 1.7) * 0.018 + g.nod * 0.06; // js:705 (wvz term = 0)
  if (dt > 0.0) {
    s_hair.v += (-70.0 * (s_hair.x - htx) - 13.0 * s_hair.v) * dt; // js:706
    s_hair.x += s_hair.v * dt;
    s_hair.vz += (-70.0 * (s_hair.z - htz) - 13.0 * s_hair.vz) * dt; // js:708
    s_hair.z += s_hair.vz * dt;
  }

  // ---- rub knead orbits (js:721-723 + 729-739 solo micro-orbits) ----
  const double knead = std::sin(t * 11.0) * 0.024 * rubAmt;  // js:722
  const double knead2 = std::cos(t * 11.0) * 0.014 * rubAmt; // js:723
  const double clitOx = std::sin(t * 12.0) * 0.016 * rubAmt; // js:730
  const double clitOy = std::cos(t * 12.0) * 0.008 * rubAmt; // js:731
  const double clitOz = std::sin(t * 6.0) * 0.010 * rubAmt;  // js:732
  const double breastBobY = std::sin(t * 8.0) * 0.014 * rubAmt; // js:737
  const double breastBobX = std::cos(t * 8.0) * 0.010 * rubAmt; // js:738

  // ---- oral (js:810-832) ----
  double oralBob = 0.0, oralSp = 0.0, gagWob = 0.0;
  if (oral > 0.03) {
    if (isBlow) { // js:812-822 fellatio
      oralSp = 6.2 + std::fabs(g.vel) * 4.5; // js:814
      oralBob = std::sin(t * oralSp) * 0.062 * (0.5 + depth * 0.5); // js:815
    } else {      // js:824-830 cunnilingus
      oralSp = 5.2;
      oralBob =
          std::sin(t * 5.2) * 0.055 * (0.4 + g.oralDepth * 0.6); // js:825
      gagWob = std::sin(t * 22.0) * 0.01 * g.oralGag;            // js:829
    }
  }

  // ---- orgasm arch + tremble (js:835-842) ----
  double orgE = 0.0, tremor = 0.0;
  if (g.state == "orgasm") {
    orgE = std::sin(PI_D * clamp(g.orgT / 5.2, 0.0, 1.0)); // js:836
    tremor = std::sin(t * 44.0) * 0.012 * orgE;             // js:839
  }

  // ---- afterglow slump (js:846-850) ----
  const double afterS = clamp(g.after / 3.0, 0.0, 1.0); // js:847

  // ---- solo sway (js:893-906): hip-first figure-eight, additive ----
  if (dt > 0.0) g.soloPh += dt * 0.6; // js:894 (drives a G field)
  const double sway = std::sin(g.soloPh);              // js:897
  const double sway2 = std::sin(g.soloPh * 0.5 + 0.8); //

  // ---- solo ambient-moan timer (js:908-912) ----
  // Missing audio hook (playMoan) + RNG -> deterministic 4.5s re-arm
  // (midpoint of js 3.5+rand*3.5). See TODO(3).
  if (g.solo && dt > 0.0) {
    g.soloMoanT -= dt;
    if (g.soloMoanT <= 0.0 && g.state == "play") g.soloMoanT = 4.5;
  }

  // ---- publish file-local bone layer for Model::pose ----
  s_bones.thrustY = s_thrustY;
  s_bones.thrustZ = s_thrustZ;
  s_bones.breath = br;
  s_bones.breathRate = brRate;
  s_bones.breathAmt = brAmt;
  s_bones.chestHerX = herCX;
  s_bones.chestHerY = herCY;
  s_bones.chestHerZ = herCZ;
  s_bones.chestHimX = himCX;
  s_bones.chestHimY = himCY;
  s_bones.chestHimZ = himCZ;
  s_bones.shaftPulseSm = s_shaftP;
  s_bones.shaftScale = pulse;
  s_bones.glansScale = glansScale;
  s_bones.shaftRotX = shaftRotX;
  s_bones.emisR = 0.28 * ar; // js:657
  s_bones.emisG = 0.06 * ar;
  s_bones.emisB = 0.09 * ar;
  s_bones.jigBreast = bt;
  s_bones.jigButt = bt2;
  s_bones.jigClamped = bs;
  s_bones.drive = drive;
  s_bones.squash = sq;
  s_bones.hairX = s_hair.x;
  s_bones.hairZ = s_hair.z;
  s_bones.knead = rubActive ? knead : 0.0;
  s_bones.knead2 = rubActive ? knead2 : 0.0;
  s_bones.clitOx = clitOrbit ? clitOx : 0.0;
  s_bones.clitOy = clitOrbit ? clitOy : 0.0;
  s_bones.clitOz = clitOrbit ? clitOz : 0.0;
  s_bones.breastBobX = breastOrbit ? breastBobX : 0.0;
  s_bones.breastBobY = breastOrbit ? breastBobY : 0.0;
  s_bones.oralBob = oralBob;
  s_bones.oralSp = oralSp;
  s_bones.gagWob = gagWob;
  s_bones.orgasmE = orgE;
  s_bones.tremor = tremor;
  s_bones.afterS = afterS;
  s_bones.soloSway = sway;
  s_bones.soloSway2 = sway2;
  s_bones.poseEase = s_poseEase;
  s_bones.isCowgirl = isCowgirl;
  s_bones.isBlow = isBlow;
  s_bones.rubActive = rubActive;
}

// ---- FX update: spurt-edge flash + bloom smoothing (fx3d.js:181-273) ----
void Fx::update(const Game& g, double dt) {
  if (dt < 0.0) dt = 0.0;
  if (dt > 0.25) dt = 0.25; // tab-switch guard (render-only state)
  // js:188-202 climax burst edge: fire flash when the spurt counter ticks.
  // World anchors (shaftTipWorld3/him3) do not exist natively, so the 26-jet
  // ballistic spawn stays owned by the shared 2D sim (mechanics spawnJets);
  // here we mirror only the screen response: flash3(0.42).
  const double sp = g.spurts;
  if (sp != s_lastSpurtsFx) {
    s_lastSpurtsFx = sp;
    if (g.state == "climax" || g.state == "finish") s_flash = 0.42; // js:196
  }
  if ((g.state != "climax" && g.state != "finish") && sp == 0.0)
    s_lastSpurtsFx = 0.0; // js:200-202 re-arm
  // flash3 fade is a .5s ease-out (js:281-286); linear 0.84/s matches the
  // 0.42 impulse decaying over ~0.5s.
  if (s_flash > 0.0) {
    s_flash -= dt * 0.84;
    if (s_flash < 0.0) s_flash = 0.0;
  }
  // js:265-269 bloom overlay target = bloom*0.45 clamped 0..0.8.
  const double target = clamp(g.bloom * 0.45, 0.0, 0.8);
  s_bloomSm = dampNum(s_bloomSm, target, 8.0, dt);
  // Rate accumulators below document fx3d.js throttle constants; spawning
  // stays in the shared 2D sim, so these only advance for parity/tuning:
  // js:208 drip 0.09s, js:220 sweat 0.55-p*0.42 gated p>0.28,
  // js:234 glisten 0.14s gated ar>0.24, js:247 hearts 0.22s in orgasm.
  s_dripAcc += dt;
  s_sweatAcc += dt;
  s_glisAcc += dt;
  s_heartAcc += dt;
}

// ---- FX draw: shared Game vectors as additive billboards ----
void Fx::draw(Engine& e, const Game& g) {
  (void)e; // fixed camera rig (engine3d.js) not yet ported; overlay is
           // screen-space so it needs no camera.
  if (!d3gl::UseProgram || !d3gl::BindBuffer || !d3gl::BufferData ||
      !d3gl::DrawArrays || !d3gl::Enable || !d3gl::Disable ||
      !d3gl::BlendFunc || !d3gl::DepthMask ||
      !d3gl::EnableVertexAttribArray || !d3gl::DisableVertexAttribArray ||
      !d3gl::VertexAttribPointer)
    return; // GL not loaded (headless parity): draw nothing, touch nothing.
  if (!ensureFx()) return;
  s_batch.clear();
  const double t = g.t;

  // Bloom + flash first (behind particles; additive so order is cosmetic).
  // js:266 target = bloom*0.45; flash adds the one-shot 0.42 kick.
  const double bloomA = clamp(s_bloomSm + s_flash, 0.0, 0.8);
  if (bloomA > 0.004) {
    pushSprite(0.0f, 0.0f, 440.0f, (float)bloomA, 1.0f, 0.949f, 0.925f);
    pushSprite(0.0f, 0.15f, 260.0f, (float)(bloomA * 0.7), 1.0f, 0.93f,
               0.90f);
  }

  // Climax jets: js gfx.js:415-439 alpha = clamp(life*2.4,0,1)*0.88,
  // side view only (j.view!=='side' skip). Size ~ droplet head 4+spd*0.009.
  // Native Jet lacks pixel speed after sim steps, so use a fixed 9px bead;
  // the tail gradient is approximated by the soft shader falloff.
  for (const Jet& j : g.jets) {
    if (j.view != 's') continue;
    const double a = clamp(j.life * 2.4, 0.0, 1.0) * 0.88;
    pushSprite(toClipX(j.x), toClipY(j.y), 9.0f, (float)a, 1.0f, 0.98f,
               0.965f);
  }

  // Drips: js gfx.js:379-412. Native Drip has {x,ox,p,j} (no oy/sp: the
  // 2D sim integrates p via sp). Rebuild the track position exactly:
  // pts ox/oy, oy+22, oy+46, oy+68 with oy fallback 505 (js:380).
  for (const Drip& d : g.drips) {
    const double pp = clamp(d.p, 0.0, 1.0) * 3.0;
    int i = (int)pp;
    if (i > 2) i = 2;
    if (i < 0) i = 0;
    const double f = pp - (double)i;
    static const double kOY[4] = {0.0, 22.0, 46.0, 68.0};
    static const double kOX[4] = {0.0, -7.0, -12.0, -16.0};
    const double y = 505.0 + lerp(kOY[i], kOY[i + 1], f);
    const double x =
        lerp(kOX[i], kOX[i + 1], f) + d.x + std::sin(d.p * 9.5 + d.j) * 1.8;
    pushSprite(toClipX(x), toClipY(y), 6.0f, 0.78f, 1.0f, 0.972f, 0.957f);
  }

  // Glisten specks: js gfx.js:442-448 alpha = a*0.65*pulse,
  // pulse = 0.5+0.5*sin(t*8+x). Additive in BOTH builds (fx3d.js:134).
  for (const Speck& s : g.glisten) {
    const double pulse = 0.5 + 0.5 * std::sin(t * 8.0 + s.x);
    const double a = std::min(s.a, 0.55) * 0.65 * pulse; // js:663 clamp
    pushSprite(toClipX(s.x), toClipY(s.y), 4.0f, (float)a, 1.0f, 0.988f,
               0.972f);
  }

  // Sweat beads: js gfx.js:451-461 specular bead alpha 0.85*life.
  // Native Sweat lacks vy/ph (2D sim integrates); draw needs x/y/life only.
  for (const Sweat& s : g.sweat) {
    const double a = 0.85 * clamp(s.life, 0.0, 1.0);
    pushSprite(toClipX(s.x), toClipY(s.y), 3.5f, (float)a, 0.90f, 0.95f,
               1.0f);
  }

  // Hearts: js gfx.js:464-473 alpha 0.62*fade, wobble x+sin(ph)*7,
  // size 7.5*s radius -> ~15*s px diameter, pink #ff6487.
  for (const Heart& h : g.hearts) {
    const double fade = std::min(1.0, h.life);
    const double hx = h.x + std::sin(h.ph) * 7.0; // js:470
    pushSprite(toClipX(hx), toClipY(h.y), (float)(15.0 * h.s), (float)(0.62 * fade),
               1.0f, 0.392f, 0.529f);
  }

  if (s_batch.empty()) return;
  const d3gl::GLsizei n = (d3gl::GLsizei)s_batch.size();

  d3gl::UseProgram(s_fxProg);
  d3gl::BindBuffer(d3gl::ARRAY_BUFFER, s_fxVbo);
  d3gl::BufferData(d3gl::ARRAY_BUFFER,
                   (d3gl::GLsizeiptr)((std::size_t)n * sizeof(FxVert)),
                   s_batch.data(), d3gl::DYNAMIC_DRAW);

  const d3gl::GLsizei stride = (d3gl::GLsizei)sizeof(FxVert);
  d3gl::EnableVertexAttribArray((d3gl::GLuint)s_aPos);
  d3gl::VertexAttribPointer((d3gl::GLuint)s_aPos, 2, d3gl::FLOAT_,
                            d3gl::FALSE_, stride,
                            reinterpret_cast<const void*>(offsetof(FxVert, x)));
  d3gl::EnableVertexAttribArray((d3gl::GLuint)s_aSize);
  d3gl::VertexAttribPointer((d3gl::GLuint)s_aSize, 1, d3gl::FLOAT_,
                            d3gl::FALSE_, stride,
                            reinterpret_cast<const void*>(offsetof(FxVert, size)));
  d3gl::EnableVertexAttribArray((d3gl::GLuint)s_aAlpha);
  d3gl::VertexAttribPointer((d3gl::GLuint)s_aAlpha, 1, d3gl::FLOAT_,
                            d3gl::FALSE_, stride,
                            reinterpret_cast<const void*>(offsetof(FxVert, alpha)));
  d3gl::EnableVertexAttribArray((d3gl::GLuint)s_aColor);
  d3gl::VertexAttribPointer((d3gl::GLuint)s_aColor, 3, d3gl::FLOAT_,
                            d3gl::FALSE_, stride,
                            reinterpret_cast<const void*>(offsetof(FxVert, r)));

  // Task mandates additive billboards (fx3d.js used Normal except glisten;
  // see summary). Depth write off so overlay never occludes the scene.
  d3gl::Enable(d3gl::BLEND);
  d3gl::BlendFunc(d3gl::SRC_ALPHA, d3gl::ONE_);
  d3gl::DepthMask(d3gl::FALSE_);
  d3gl::DrawArrays(d3gl::POINTS, 0, n);
  d3gl::DepthMask(d3gl::TRUE_);
  d3gl::BlendFunc(d3gl::SRC_ALPHA, d3gl::ONE_MINUS_SRC_ALPHA);

  d3gl::DisableVertexAttribArray((d3gl::GLuint)s_aPos);
  d3gl::DisableVertexAttribArray((d3gl::GLuint)s_aSize);
  d3gl::DisableVertexAttribArray((d3gl::GLuint)s_aAlpha);
  d3gl::DisableVertexAttribArray((d3gl::GLuint)s_aColor);
  d3gl::BindBuffer(d3gl::ARRAY_BUFFER, 0);
  d3gl::UseProgram(0);
}

} // namespace ag::d3
