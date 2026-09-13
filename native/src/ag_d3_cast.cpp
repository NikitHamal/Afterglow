// Afterglow native — ag_d3_cast.cpp
// Port of the 3D cast + 3D poses: js3d/chars3d.js (cast assembly, BODY canon)
// and js3d/poses3d.js (POSES3[0..6] + ORAL_LICK3/ORAL_BLOW3 + SOLO_POSE3 with
// bio/lerp/select helpers) for d3::Cast::loadAll / ::pose / ::draw (ag_3d.h).
//
// SOURCES (read-only, do not edit):
//   js3d/chars3d.js  — buildHer3/buildHim3, BODY canon, paintFace3 inputs
//   js3d/poses3d.js  — POSES3:81-279, ORAL_LICK3:284-310, ORAL_BLOW3:312-338,
//                      SOLO_POSE3:347-375, currentPose3:382-388,
//                      evaluatePoseBio3:390-417, lerpPose3:419-485,
//                      applyPoseCam3:492-508, anchors:515-546
//   js3d/glbModel.js — GLB_REGISTRY:26-30 (paths/fit/yOff), lazy-load:466-489
//   js3d/anim3d.js   — initChars3:33-44, damp*:311-346, applyRig3:487-544,
//                      updateAnim3 blend:556-579 + thrust:637-641 + shaft:646-658
//                      + breathing:583-586,660-662 + solo:224-232,893-911,
//                      rub keys:597-615, face repaint:869-873
//   js/chars.js      — CHARS:27-76 (GLB flags live on goatchan/kiyoko/anime)
//   js/poses.js      — POSES:5-13 names, posName()
//   js/side.js       — herExpression():7-36 (shared 2D face API)
//
// UNITS: JS pose tables are already radians — copied verbatim, NO deg->rad
// conversion. Camera fov stays in degrees (both sides). Solo him shaft is
// -Math.PI/2 in JS, written here as -(TAU/4).
//
// SHARED-2D-API REUSE:
//   - poses::name(g) (ag_2dmods.h, js/poses.js posName) builds the pose
//     identity key for couple poses (solo/oral short-circuit first, exactly
//     like currentPose3). C++ stub names are shortened ("LEGS-UP" vs 3D
//     "LEGS-UP / DEEP"); mapping table is in poseIdentity() below.
//   - side::herExpression(cv, g) (js/side.js herExpression) is called in
//     Cast::draw on a reused scratch Canvas so the 3D face pass stays linked
//     to the 2D API. The C++ stub returns void today, so the actual face
//     state used here is a file-local mirror (faceFromGame, same formulas).
//
// MISSING Game FIELDS (file-local statics + report in RETURN summary):
//   (a) G.oralMode string — derived blow/lick from oralT==2 (poses3d.js:385
//       accepts either; C++ oral.js/charui.js only ever set oralT 0/1/2).
//   (b) G.char.isGLB / .glbPath — derived from preset (goatchan/kiyoko/anime
//       per js/chars.js:52-76); custom glbPath has no C++ home yet.
//   (c) G.mouths[] {t0,dur,i} — C++ MoanMouth{x,y,t} differs; mouth-term
//       skipped in faceFromGame (TODO).
//   (d) GLB_STORE per-preset models — Cast holds one her/him pair only; the
//       two non-active registry paths are recorded file-locally (TODO).
//   (e) Model joint storage + visibility flags — evaluated joints live in
//       file-local CastState; Model::pose(g,dt) is driven with the same
//       inputs until Model gains joint channels (TODO).
//   (f) Camera orbit-mode gate — C++ Camera has no mode; presets apply
//       unconditionally on pose switch (TODO).
//
// TODOS (also returned in chat summary):
//   T1 ag_3d_todo.cpp still defines Cast::loadAll/pose/draw stubs — DELETE
//      those three stubs (or drop the file) once this TU links, else duplicate
//      symbols. Add this file to native/CMakeLists.txt afterglow_core.
//   T2 Model::load cgltf backend (glbModel.js) — loadAll resolves paths now,
//      loads flip true automatically once Model::load works.
//   T3 Per-preset GLB_STORE + procedural-vs-GLB visibility (anim3d.js:854-857,
//      AGENTS.md 3.3) + 2-bone IK/solveHands3 + updateSolo3 rub trajectories
//      belong to the anim3d/engine3d port; anchors here are data only.
#include "ag_3d.h"
#include "ag_2dmods.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace ag::d3 {
namespace {

// ---- tiny vectors (JS plain arrays) ----
using V3 = std::array<double, 3>;
using V2 = std::array<double, 2>;

// One rig configuration: mirrors a poses3d.js her:/him: block verbatim.
struct Rig {
  V3 pos{0, 0, 0}, up{0, 1, 0}, fwd{0, 0, 1}, spine{0, 0, 0}, chest{0, 0, 0},
      head{0, 0, 0};
  V2 hip{0, 0}, hipRot{0, 0}, knee{0, 0}, ankle{0, 0}, toes{0, 0}, arm{0, 0},
      armZ{0, 0}, elbow{0, 0}, wrist{0, 0};
  double spread = 0, shaft = 0; // shaft: him only
  bool hasShaft = false;
};

enum class HKind : int {
  None = 0,
  HerChest,
  HerHead,
  HerHips,
  HerKneeL,
  HerKneeR,
  HerThighL,
  HerThighR,
  HerShoulderR, // sic: only in ORAL_BLOW3.himR; anim3d handTarget3 has no
                // branch for it, so JS falls through to herChest. Mirrored.
  HimShoulderL,
  HimShoulderR,
  HimThighL,
  HimThighR
};

// Mirrors a {k,x,y,z,floor?} hand entry; present=false is JS null.
struct Hand {
  bool present = false;
  HKind kind = HKind::None;
  double x = 0, y = 0, z = 0;
  double floor = 0.125; // handTarget3 default (anim3d.js:95)
  bool hasFloor = false;
};

struct Cam {
  double yaw = 0, pitch = 0, dist = 4, fov = 42;
  V3 target{0, 0.35, 0};
};

struct PoseEntry {
  const char* key = "";     // stable C++ key (== poses::name-ish)
  const char* label3d = ""; // exact POSES3[].name for parity logs
  Rig her, him;
  Hand himL, himR, herL, herR;
  Cam cam;
};

// ---- compact table builders (positional, one call per rig) ----
inline Rig mkRig(V3 pos, V3 up, V3 fwd, V3 spine, V3 chest, V3 head, V2 hip,
                 V2 hipRot, V2 knee, V2 ankle, V2 toes, double spread, V2 arm,
                 V2 armZ, V2 elbow, V2 wrist) {
  Rig r;
  r.pos = pos;
  r.up = up;
  r.fwd = fwd;
  r.spine = spine;
  r.chest = chest;
  r.head = head;
  r.hip = hip;
  r.hipRot = hipRot;
  r.knee = knee;
  r.ankle = ankle;
  r.toes = toes;
  r.spread = spread;
  r.arm = arm;
  r.armZ = armZ;
  r.elbow = elbow;
  r.wrist = wrist;
  return r;
}
inline void setShaft(Rig& r, double s) {
  r.shaft = s;
  r.hasShaft = true;
}
inline Hand mkHand(HKind k, double x, double y, double z) {
  Hand h;
  h.present = true;
  h.kind = k;
  h.x = x;
  h.y = y;
  h.z = z;
  return h;
}
inline Hand mkHandFloor(HKind k, double x, double y, double z, double fl) {
  Hand h = mkHand(k, x, y, z);
  h.floor = fl;
  h.hasFloor = true;
  return h;
}
inline Cam mkCam(double yaw, double pitch, double dist, double fov, V3 tgt) {
  Cam c;
  c.yaw = yaw;
  c.pitch = pitch;
  c.dist = dist;
  c.fov = fov;
  c.target = tgt;
  return c;
}

// ---- pose table: indices 0..6 couple, 7 lick, 8 blow, 9 solo ----
inline const std::array<PoseEntry, 10>& poseTable() {
  static const std::array<PoseEntry, 10> k = [] {
    std::array<PoseEntry, 10> t{};
    { // 0 — MISSIONARY (poses3d.js:82-108)
      PoseEntry e;
      e.key = "MISSIONARY";
      e.label3d = "MISSIONARY";
      e.her = mkRig({0, 0.31, -0.52}, {0, -0.06, -0.998}, {0, 0.998, -0.06},
                    {-0.02, 0, 0}, {0.04, 0, 0}, {-0.22, 0, 0}, {-1.22, -1.14},
                    {0.38, -0.38}, {1.96, 1.88}, {0.42, 0.38}, {0.55, 0.50},
                    0.58, {0.18, 0.18}, {0.72, -0.72}, {-0.22, -0.22},
                    {0.15, -0.15});
      e.him = mkRig({0, 0.50, -0.21}, {0, -0.10, -0.99}, {0, -0.99, 0.10},
                    {0.12, 0, 0}, {0.08, 0, 0}, {-0.46, 0, 0}, {-1.58, -1.58},
                    {0.22, -0.22}, {1.84, 1.84}, {0.28, 0.28}, {0.10, 0.10},
                    0.44, {-1.42, -1.42}, {0.82, -0.82}, {-0.18, -0.18},
                    {-0.10, 0.10});
      setShaft(e.him, -2.96);
      e.himL = mkHandFloor(HKind::HerChest, 0.26, -0.04, 0.06, 0.12);
      e.himR = mkHandFloor(HKind::HerChest, -0.26, -0.04, 0.06, 0.12);
      e.herL = mkHand(HKind::HimShoulderL, 0.06, -0.02, 0.04);
      e.herR = mkHand(HKind::HimShoulderR, -0.06, -0.02, 0.04);
      e.cam = mkCam(0.45, 0.32, 4.2, 42, {0, 0.36, -0.42});
      t[0] = e;
    }
    { // 1 — LEGS-UP / DEEP (poses3d.js:110-136)
      PoseEntry e;
      e.key = "LEGS-UP";
      e.label3d = "LEGS-UP / DEEP";
      e.her = mkRig({0, 0.31, -0.60}, {0, 0.05, -0.998}, {0, 0.998, 0.05},
                    {-0.08, 0, 0}, {0.06, 0, 0}, {-0.16, 0, 0}, {-2.42, -2.34},
                    {0.24, -0.24}, {0.62, 0.56}, {0.72, 0.68}, {0.85, 0.80},
                    0.36, {0.24, 0.24}, {1.20, -1.20}, {-0.18, -0.18},
                    {0.25, -0.25});
      e.him = mkRig({0, 0.49, -0.28}, {0, -0.14, -0.99}, {0, -0.99, 0.14},
                    {0.18, 0, 0}, {0.12, 0, 0}, {-0.58, 0, 0}, {-1.48, -1.48},
                    {0.20, -0.20}, {1.82, 1.82}, {0.32, 0.32}, {0.15, 0.15},
                    0.38, {-1.46, -1.46}, {0.68, -0.68}, {-0.15, -0.15},
                    {-0.08, 0.08});
      setShaft(e.him, -3.02);
      e.himL = mkHand(HKind::HerKneeL, 0.06, 0.02, 0.04);
      e.himR = mkHand(HKind::HerKneeR, -0.06, 0.02, 0.04);
      e.herL = mkHand(HKind::HerHead, -0.14, 0.05, 0.02);
      e.herR = mkHand(HKind::HerHead, 0.14, 0.05, 0.02);
      e.cam = mkCam(0.32, 0.28, 3.9, 40, {0, 0.38, -0.36});
      t[1] = e;
    }
    { // 2 — DOGGY (ARCHED) (poses3d.js:138-164)
      PoseEntry e;
      e.key = "DOGGY";
      e.label3d = "DOGGY (ARCHED)";
      e.her = mkRig({0, 0.56, -0.28}, {0, 0.15, -0.99}, {0, -0.99, -0.15},
                    {0.28, 0, 0}, {-0.18, 0, 0}, {0.38, 0, 0}, {-1.34, -1.34},
                    {0.18, -0.18}, {1.12, 1.12}, {0.65, 0.65}, {0.45, 0.45},
                    0.32, {-1.82, -1.82}, {0.24, -0.24}, {-0.12, -0.12},
                    {0.08, -0.08});
      e.him = mkRig({0, 0.58, 0.18}, {0, 0.88, -0.47}, {0, -0.47, -0.88},
                    {-0.12, 0, 0}, {0.14, 0, 0}, {-0.14, 0, 0}, {-0.38, -0.38},
                    {0.28, -0.28}, {1.66, 1.66}, {0.48, 0.48}, {0.20, 0.20},
                    0.48, {-0.56, -0.56}, {0.46, -0.46}, {-0.26, -0.26},
                    {-0.15, 0.15});
      setShaft(e.him, -1.78);
      e.himL = mkHand(HKind::HerHips, -0.14, 0.02, -0.06);
      e.himR = mkHand(HKind::HerHips, 0.14, 0.02, -0.06);
      e.herL = mkHandFloor(HKind::HerChest, -0.16, -0.12, 0.03, 0.12);
      e.herR = mkHandFloor(HKind::HerChest, 0.16, -0.12, 0.03, 0.12);
      e.cam = mkCam(-0.62, 0.36, 4.4, 44, {0, 0.52, 0.02});
      t[2] = e;
    }
    { // 3 — PRONE BONE (FLAT), herL/herR null (poses3d.js:166-191)
      PoseEntry e;
      e.key = "PRONE";
      e.label3d = "PRONE BONE (FLAT)";
      e.her = mkRig({0, 0.24, -0.42}, {0, -0.22, -0.97}, {0, -0.97, 0.22},
                    {0.12, 0, 0}, {-0.08, 0, 0}, {0.22, 0.62, -0.10},
                    {0.08, 0.04}, {0.10, -0.10}, {0.22, 0.18}, {0.35, 0.30},
                    {0.60, 0.55}, 0.24, {3.05, 3.05}, {1.25, -1.25},
                    {-0.24, -0.24}, {0.12, -0.12});
      e.him = mkRig({0, 0.46, -0.14}, {0, -0.06, -0.99}, {0, -0.99, 0.06},
                    {0.15, 0, 0}, {0.08, 0, 0}, {-0.34, 0, 0}, {-0.58, -0.58},
                    {0.26, -0.26}, {0.36, 0.36}, {0.22, 0.22}, {0.10, 0.10},
                    0.48, {-1.42, -1.42}, {0.68, -0.68}, {-0.14, -0.14},
                    {-0.08, 0.08});
      setShaft(e.him, -2.38);
      e.himL = mkHandFloor(HKind::HerChest, -0.28, -0.02, 0.06, 0.12);
      e.himR = mkHandFloor(HKind::HerChest, 0.28, -0.02, 0.06, 0.12);
      e.cam = mkCam(0.68, 0.42, 4.3, 42, {0, 0.38, -0.16});
      t[3] = e;
    }
    { // 4 — COWGIRL (RIDING) (poses3d.js:193-223)
      PoseEntry e;
      e.key = "COWGIRL";
      e.label3d = "COWGIRL (RIDING)";
      e.her = mkRig({0, 0.30, 0.02}, {0, 0.98, -0.18}, {0, 0.18, 0.98},
                    {-0.16, 0, 0}, {0.12, 0, 0}, {-0.22, 0, 0}, {-1.12, -1.12},
                    {0.48, -0.48}, {2.54, 2.54}, {0.82, 0.82}, {0.45, 0.45},
                    0.68, {-0.34, -0.34}, {0.46, -0.46}, {-0.30, -0.30},
                    {0.10, -0.10});
      e.him = mkRig({0, 0.17, -0.12}, {0, 0.05, -0.99}, {0, 0.99, 0.05},
                    {0.08, 0, 0}, {0.06, 0, 0}, {0.26, 0, 0}, {-0.26, -0.26},
                    {0.24, -0.24}, {0.38, 0.38}, {0.25, 0.25}, {0.10, 0.10},
                    0.46, {-0.88, -0.88}, {0.92, -0.92}, {-0.65, -0.65},
                    {-0.12, 0.12});
      setShaft(e.him, 0.15);
      e.himL = mkHandFloor(HKind::HimShoulderL, -0.25, -0.05, -0.12, 0.12);
      e.himR = mkHandFloor(HKind::HimShoulderR, 0.25, -0.05, -0.12, 0.12);
      e.herL = mkHand(HKind::HerHead, -0.10, 0.02, 0.06);
      e.herR = mkHand(HKind::HerHead, 0.10, 0.02, 0.06);
      e.cam = mkCam(0.22, 0.24, 3.8, 42, {0, 0.58, -0.02});
      t[4] = e;
    }
    { // 5 — REV COWGIRL (poses3d.js:225-251)
      PoseEntry e;
      e.key = "REV-COWG";
      e.label3d = "REV COWGIRL";
      e.her = mkRig({0, 0.38, 0.02}, {0, 0.96, 0.26}, {0, -0.26, 0.96},
                    {0.22, 0, 0}, {-0.14, 0, 0}, {0.14, 0, 0}, {-1.14, -1.14},
                    {0.46, -0.46}, {2.54, 2.54}, {0.82, 0.82}, {0.45, 0.45},
                    0.64, {-0.34, -0.34}, {0.34, -0.34}, {-0.28, -0.28},
                    {0.10, -0.10});
      e.him = mkRig({0, 0.17, 0.11}, {0, 0.05, -0.99}, {0, 0.99, 0.05},
                    {0.08, 0, 0}, {0.06, 0, 0}, {0.22, 0, 0}, {-0.26, -0.26},
                    {0.24, -0.24}, {0.38, 0.38}, {0.25, 0.25}, {0.10, 0.10},
                    0.46, {-0.74, -0.74}, {1.02, -1.02}, {-0.82, -0.82},
                    {-0.12, 0.12});
      setShaft(e.him, -1.64);
      e.himL = mkHand(HKind::HerHips, -0.14, 0.03, 0.05);
      e.himR = mkHand(HKind::HerHips, 0.14, 0.03, 0.05);
      e.herL = mkHand(HKind::HerHead, -0.10, 0.08, 0.02);
      e.herR = mkHand(HKind::HerHead, 0.10, 0.08, 0.02);
      e.cam = mkCam(-0.35, 0.20, 3.8, 42, {0, 0.60, 0.16});
      t[5] = e;
    }
    { // 6 — SPOONING, herL/herR null (poses3d.js:253-278)
      PoseEntry e;
      e.key = "SPOON";
      e.label3d = "SPOONING";
      e.her = mkRig({0, 0.16, -0.32}, {0, 0.04, -0.99}, {0.98, 0.16, 0.08},
                    {0.06, -0.12, 0.04}, {0.04, -0.08, 0}, {0.12, -0.38, 0.08},
                    {-1.12, -0.68}, {0.22, -0.12}, {1.62, 1.34}, {0.45, 0.40},
                    {0.52, 0.48}, 0.22, {-0.88, -0.88}, {0.58, -0.58},
                    {-0.98, -0.98}, {0.15, -0.15});
      e.him = mkRig({-0.14, 0.18, -0.08}, {0, 0.04, -0.99}, {0.98, 0.12, 0.06},
                    {0.08, -0.10, 0}, {0.06, -0.06, 0}, {0.08, -0.34, 0},
                    {-0.76, -0.54}, {0.18, -0.10}, {1.06, 0.84}, {0.35, 0.30},
                    {0.15, 0.15}, 0.12, {-1.08, -1.08}, {0.52, -0.52},
                    {-1.08, -1.08}, {-0.10, 0.10});
      setShaft(e.him, -2.54);
      e.himL = mkHand(HKind::HerChest, 0.08, -0.06, 0.08);
      e.himR = mkHand(HKind::HerHips, 0.04, 0.02, 0.06);
      e.cam = mkCam(1.20, 0.30, 4.1, 42, {0, 0.38, -0.08});
      t[6] = e;
    }
    { // 7 — ORAL (LICK) (poses3d.js:284-310; ORAL3 alias, poses3d.js:340)
      PoseEntry e;
      e.key = "LICK";
      e.label3d = "ORAL (LICK)";
      e.her = mkRig({0, 0.30, -0.48}, {0, -0.06, -0.998}, {0, 0.998, -0.06},
                    {-0.02, 0, 0}, {0.06, 0, 0}, {-0.34, 0, 0}, {-1.34, -1.22},
                    {0.55, -0.55}, {2.10, 2.00}, {0.55, 0.50}, {0.72, 0.68},
                    0.82, {-0.68, -0.68}, {1.08, -1.08}, {-0.98, -0.98},
                    {0.20, -0.20});
      e.him = mkRig({0, 0.53, 0.24}, {0, -0.18, -0.98}, {0, -0.98, 0.18},
                    {0.10, 0, 0}, {0.16, 0, 0}, {-0.45, 0, 0}, {-0.64, -0.64},
                    {0.32, -0.32}, {1.22, 1.22}, {0.38, 0.38}, {0.15, 0.15},
                    0.56, {-1.92, -1.92}, {0.68, -0.68}, {-0.74, -0.74},
                    {-0.12, 0.12});
      setShaft(e.him, 0.0);
      e.himL = mkHand(HKind::HerThighL, 0.06, 0.04, 0.06);
      e.himR = mkHand(HKind::HerThighR, -0.06, 0.04, 0.06);
      e.herL = mkHand(HKind::HerHead, -0.10, 0.04, 0.02);
      e.herR = mkHand(HKind::HerHead, 0.10, 0.04, 0.02);
      e.cam = mkCam(0.15, 0.36, 3.6, 38, {0, 0.36, -0.08});
      t[7] = e;
    }
    { // 8 — ORAL (BLOW), note him-first order in JS (poses3d.js:312-338)
      PoseEntry e;
      e.key = "BLOW";
      e.label3d = "ORAL (BLOW)";
      e.her = mkRig({0, 0.30, 0.18}, {0, 0.58, -0.81}, {0, -0.81, -0.58},
                    {0.18, 0, 0}, {0.14, 0, 0}, {-0.22, 0, 0}, {-1.88, -1.88},
                    {0.22, -0.22}, {2.35, 2.35}, {0.55, 0.55}, {0.45, 0.45},
                    0.34, {-0.85, -0.85}, {0.38, -0.38}, {-0.55, -0.55},
                    {0.15, -0.15});
      e.him = mkRig({0, 0.44, -0.26}, {0, 0.45, -0.89}, {0, 0.89, 0.45},
                    {0.08, 0, 0}, {0.06, 0, 0}, {-0.30, 0, 0}, {-1.45, -1.45},
                    {0.42, -0.42}, {1.70, 1.70}, {0.32, 0.32}, {0.10, 0.10},
                    0.62, {-0.95, -0.95}, {0.72, -0.72}, {-0.35, -0.35},
                    {0.10, -0.10});
      setShaft(e.him, -1.25);
      e.herL = mkHand(HKind::HimThighL, 0.08, 0.06, 0.06);
      e.herR = mkHand(HKind::HimThighR, -0.08, 0.06, 0.06);
      e.himL = mkHand(HKind::HerHead, 0.12, 0.06, 0.04);
      e.himR = mkHand(HKind::HerShoulderR, 0.04, 0.02, 0.04);
      e.cam = mkCam(0.65, 0.28, 2.3, 38, {0, 0.38, -0.06});
      t[8] = e;
    }
    { // 9 — SOLO (poses3d.js:347-375; shaft -Math.PI/2)
      PoseEntry e;
      e.key = "SOLO";
      e.label3d = "SOLO";
      e.her = mkRig({0, 0.20, -0.22}, {0, -0.04, -0.999}, {0, 0.999, -0.04},
                    {0.04, 0, 0}, {0.02, 0, 0}, {-0.12, 0.04, 0}, {0.06, 0.06},
                    {0.10, -0.10}, {0.20, 0.20}, {0.15, 0.15}, {0.12, 0.12},
                    0.26, {0.14, 0.14}, {0.36, -0.36}, {0.18, 0.18},
                    {0.04, -0.04});
      e.him = mkRig({0, -1.5, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}, {0, 0, 0},
                    {0, 0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0,
                    {0, 0}, {0, 0}, {0, 0}, {0, 0});
      setShaft(e.him, -(TAU / 4.0));
      e.herL = mkHand(HKind::HerChest, -0.08, 0.04, 0.09);
      e.herR = mkHand(HKind::HerHips, 0.04, -0.03, 0.12);
      e.cam = mkCam(0.50, 0.30, 3.4, 40, {0, 0.35, -0.20});
      t[9] = e;
    }
    return t;
  }();
  return k;
}

// ---- GLB registry (js3d/glbModel.js:26-30, js/chars.js:52-76) ----
struct GlbReg {
  const char* preset;
  const char* rel; // under assets/
  double fit, yOff;
};
inline constexpr GlbReg kGlbReg[3] = {
    {"goatchan", "assets/goatchan/goatchan.glb", 1.35, 0.62},
    {"kiyoko", "assets/kiyoko.005.glb", 1.35, 0.62},
    {"anime", "assets/free_download_female_anime.glb", 1.35, 0.62},
};

// BODY canon (chars3d.js:639-661 her, 1191-1194 him) for the anim3d-port IK
// stage — limb lengths the aim solver needs: her upper/fore 0.250/0.232,
// thigh/shin 0.430/0.410, headR 0.115, shoulderX 0.148; him upper/fore
// 0.272/0.252, thigh/shin 0.418/0.402, headR 0.134. Kept as a comment until
// the solver lands (new-file-only rule: no second TU to house it).

// ---- math mirrors ----
inline double angLerp(double a, double b, double t) { // poses3d.js:27-32
  double d = std::fmod(b - a, TAU);
  const double pi = TAU / 2.0;
  if (d < -pi) d += TAU;
  if (d > pi) d -= TAU;
  return a + d * t;
}
inline double hermite(double t) { // lerpPose3 s-curve (poses3d.js:424)
  t = clamp(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}
inline double dampNum(double cur, double tgt, double rate,
                      double dt) { // anim3d.js:311-315
  if (!std::isfinite(cur)) cur = tgt;
  if (!std::isfinite(tgt)) return cur;
  return cur + (tgt - cur) * (1.0 - std::exp(-rate * dt));
}
inline V3 dampV3(V3 sm, V3 tg, double rate, double dt) {
  for (int i = 0; i < 3; i++) sm[i] = dampNum(sm[i], tg[i], rate, dt);
  return sm;
}
inline V2 dampV2(V2 sm, V2 tg, double rate, double dt) {
  for (int i = 0; i < 2; i++) sm[i] = dampNum(sm[i], tg[i], rate, dt);
  return sm;
}
inline void dampRig(Rig& sm, const Rig& tg, double dt) { // dampCfg3:338-346
  sm.pos = dampV3(sm.pos, tg.pos, 10, dt);
  sm.up = dampV3(sm.up, tg.up, 10, dt);
  sm.fwd = dampV3(sm.fwd, tg.fwd, 10, dt);
  sm.spine = dampV3(sm.spine, tg.spine, 14, dt);
  sm.chest = dampV3(sm.chest, tg.chest, 14, dt);
  sm.head = dampV3(sm.head, tg.head, 11, dt);
  sm.hip = dampV2(sm.hip, tg.hip, 14, dt);
  sm.hipRot = dampV2(sm.hipRot, tg.hipRot, 14, dt);
  sm.knee = dampV2(sm.knee, tg.knee, 14, dt);
  sm.ankle = dampV2(sm.ankle, tg.ankle, 14, dt);
  sm.toes = dampV2(sm.toes, tg.toes, 14, dt);
  sm.spread = dampNum(sm.spread, tg.spread, 14, dt);
  sm.arm = dampV2(sm.arm, tg.arm, 14, dt);
  sm.armZ = dampV2(sm.armZ, tg.armZ, 14, dt);
  sm.elbow = dampV2(sm.elbow, tg.elbow, 14, dt);
  sm.wrist = dampV2(sm.wrist, tg.wrist, 14, dt);
  if (tg.hasShaft) {
    sm.shaft = dampNum(sm.hasShaft ? sm.shaft : tg.shaft, tg.shaft, 11, dt);
    sm.hasShaft = true;
  }
}

// ---- biomechanical state (poses3d.js:39-67) ----
struct Bio {
  double breathHeave = 0, recoilY = 0, tremor = 0, toeTension = 0, lordosis = 0;
};
inline Bio bioState(const Game& g) {
  Bio b;
  const double t = g.t;
  const double p = clamp(g.pleasure / 100.0, 0.0, 1.0);
  const double ar = clamp(g.ar / 100.0, 0.0, 1.0);
  const double d = g.depth;
  const bool inOrg = (g.state == "orgasm");
  const double brRate = 0.18 + p * 0.45;
  const double breathCycle = std::sin(t * TAU * brRate);
  b.breathHeave = breathCycle * (0.012 + p * 0.024);
  const double impact = g.impact;
  b.recoilY = -std::sin(d * (TAU / 2.0)) * 0.028 * (1.0 + impact);
  const double pi = TAU / 2.0;
  const double orgF =
      inOrg ? std::sin(pi * clamp(g.orgT / 5.2, 0.0, 1.0)) : 0.0;
  b.tremor = (orgF * 0.045 + std::max(0.0, p - 0.7) * 0.015) *
             (std::sin(t * 42.0) * 0.65 + std::cos(t * 58.0) * 0.35);
  b.toeTension = clamp(p * 0.6 + orgF * 0.75 + ar * 0.2, 0.0, 1.4);
  b.lordosis = std::sin(t * 1.2) * 0.04 + p * 0.18 + (inOrg ? 0.22 : 0.0);
  return b;
}

inline PoseEntry evalBio(PoseEntry o, const Game& g) { // poses3d.js:390-417
  const Bio b = bioState(g);
  const double d = g.depth;
  o.her.pos[1] += b.recoilY + b.breathHeave * 0.4;
  o.her.chest[0] += b.breathHeave * 1.5;
  o.her.spine[0] += b.lordosis * 0.5;
  o.her.pos[2] -= d * 0.025;
  o.him.pos[2] -= d * 0.030;
  o.her.pos[0] += b.tremor * 0.3;
  o.her.knee[0] += b.tremor * 1.2;
  o.her.knee[1] -= b.tremor * 1.2;
  o.her.toes[0] = clamp(o.her.toes[0] + b.toeTension * 0.45, 0.0, 1.8);
  o.her.toes[1] = clamp(o.her.toes[1] + b.toeTension * 0.45, 0.0, 1.8);
  o.him.pos[1] += b.breathHeave * 0.5;
  o.him.chest[0] += b.breathHeave * 1.2;
  return o;
}

// ---- pose blend (poses3d.js:419-485) ----
inline V3 mixV3(V3 a, V3 b, double s) {
  for (int i = 0; i < 3; i++) a[i] += (b[i] - a[i]) * s;
  return a;
}
inline V2 mixAng2(V2 a, V2 b, double s) {
  for (int i = 0; i < 2; i++) a[i] = angLerp(a[i], b[i], s);
  return a;
}
inline V3 mixAng3(V3 a, V3 b, double s) {
  for (int i = 0; i < 3; i++) a[i] = angLerp(a[i], b[i], s);
  return a;
}
inline Rig mixRig(const Rig& a, const Rig& b, double s) {
  Rig o;
  o.pos = mixV3(a.pos, b.pos, s);
  o.up = mixV3(a.up, b.up, s);
  o.fwd = mixV3(a.fwd, b.fwd, s);
  o.spine = mixAng3(a.spine, b.spine, s);
  o.chest = mixAng3(a.chest, b.chest, s);
  o.head = mixAng3(a.head, b.head, s);
  o.hip = mixAng2(a.hip, b.hip, s);
  o.hipRot = mixAng2(a.hipRot, b.hipRot, s);
  o.knee = mixAng2(a.knee, b.knee, s);
  o.ankle = mixAng2(a.ankle, b.ankle, s);
  o.toes = mixAng2(a.toes, b.toes, s);
  o.spread = a.spread + (b.spread - a.spread) * s;
  o.arm = mixAng2(a.arm, b.arm, s);
  o.armZ = mixAng2(a.armZ, b.armZ, s);
  o.elbow = mixAng2(a.elbow, b.elbow, s);
  o.wrist = mixAng2(a.wrist, b.wrist, s);
  if (!a.hasShaft) {
    o.shaft = b.shaft;
    o.hasShaft = b.hasShaft;
  } else if (!b.hasShaft) {
    o.shaft = a.shaft;
    o.hasShaft = true;
  } else {
    o.shaft = angLerp(a.shaft, b.shaft, s);
    o.hasShaft = true;
  }
  return o;
}
inline PoseEntry lerpPose(const PoseEntry& a, const PoseEntry& b, double t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  const double s = hermite(t);
  PoseEntry o;
  o.key = s > 0.5 ? b.key : a.key;
  o.label3d = s > 0.5 ? b.label3d : a.label3d;
  o.her = mixRig(a.her, b.her, s);
  o.him = mixRig(a.him, b.him, s);
  const Hand* src = s > 0.5 ? &b.himL : &a.himL; // hands: data, not motion
  o.himL = *src;
  src = s > 0.5 ? &b.himR : &a.himR;
  o.himR = *src;
  src = s > 0.5 ? &b.herL : &a.herL;
  o.herL = *src;
  src = s > 0.5 ? &b.herR : &a.herR;
  o.herR = *src;
  o.cam.yaw = angLerp(a.cam.yaw, b.cam.yaw, s);
  o.cam.pitch = a.cam.pitch + (b.cam.pitch - a.cam.pitch) * s;
  o.cam.dist = a.cam.dist + (b.cam.dist - a.cam.dist) * s;
  o.cam.fov = a.cam.fov + (b.cam.fov - a.cam.fov) * s;
  o.cam.target = mixV3(a.cam.target, b.cam.target, s);
  return o;
}

// ---- selection (poses3d.js:382-388). oralMode missing in C++: oralT==2 is
// blow (poses3d.js:385 accepts either; C++ writers only set 0/1/2). ----
inline bool isBlow(const Game& g) { return g.oralT == 2.0; }
inline int selectIndex(const Game& g) {
  if (g.solo) return 9;
  if (clamp(g.oral, 0.0, 1.0) > 0.03) return isBlow(g) ? 8 : 7;
  int i = g.pos % 7;
  if (i < 0) i += 7; // JS % keeps sign; Game.pos is never negative in play
  return i;
}
inline bool isGLBPreset(const std::string& preset) {
  return preset == "goatchan" || preset == "kiyoko" || preset == "anime";
}

// Pose identity via the shared 2D API (js/poses.js posName -> poses::name).
// C++ stub names ("LEGS-UP","DOGGY","PRONE","COWGIRL","REV-COWG","SPOON")
// are prefixes of the 3D labels; solo/oral short-circuit like currentPose3.
inline std::string poseIdentity(const Game& g) {
  if (g.solo) return "SOLO";
  if (clamp(g.oral, 0.0, 1.0) > 0.03) return isBlow(g) ? "BLOW" : "LICK";
  return poses::name(g);
}

// ---- face state: file-local mirror of js/side.js herExpression():7-36 ----
// (C++ side::herExpression returns void today, so pose/draw cannot read E
// back from it; this mirrors the formulas exactly and draw() still calls the
// shared API for linkage. Mouths-term skipped: C++ MoanMouth{x,y,t} lacks
// JS {t0,dur,i} — see header note (c).)
struct Face {
  double eye = 0.7, rolled = 0, mouth = 0.1, blush = 0.2, brow = 0.3,
         tilt = 0.2;
};
inline Face faceFromGame(const Game& g) {
  Face f;
  const double p = clamp(g.pleasure / 100.0, 0.0, 1.0);
  const double ar = clamp(g.ar / 100.0, 0.0, 1.0);
  f.eye = lerp(0.72, 0.12, sm(0.08, 0.92, p));
  f.rolled = 0.0;
  f.mouth = p * 0.36;
  f.blush = 0.12 + p * 0.44 + ar * 0.22;
  f.brow = lerp(-0.06, 0.46, sm(0.20, 0.90, p));
  f.tilt = 0.12 + p * 0.0035;
  if (!g.speech.empty()) f.mouth += 0.28;
  if (g.blinkPh > 0) f.eye *= (1.0 - g.blinkPh);
  if (g.state == "orgasm") {
    const double e = std::sin((TAU / 2.0) * clamp(g.orgT / 5.2, 0.0, 1.0));
    f.rolled = 0.70 + e * 0.30;
    f.eye = lerp(f.eye, 0.06, e);
    f.mouth = std::max(f.mouth, 0.88 * e);
    f.tilt = 0.50;
    f.blush = 1.0;
    f.brow = 0.65;
  }
  if (g.after > 0) {
    f.eye = std::min(f.eye, 0.16);
    f.mouth = std::max(f.mouth, 0.18);
    f.blush = std::max(f.blush, 0.45);
  }
  if (g.state == "finish" && g.finishT > 1) {
    f.eye = 0.06;
    f.mouth = 0.22;
  }
  if (g.kiss > 0.6) f.mouth = std::min(f.mouth, 0.45);
  f.eye = clamp(f.eye, 0.0, 1.0);
  f.rolled = clamp(f.rolled, 0.0, 1.0);
  f.mouth = clamp(f.mouth, 0.0, 1.0);
  f.blush = clamp(f.blush, 0.0, 1.0);
  return f;
}

// ---- path join (assetDir + registry rel) ----
inline std::string joinPath(const char* dir, const char* rel) {
  std::string d = dir ? dir : "";
  while (!d.empty() && (d.back() == '/' || d.back() == '\\')) d.pop_back();
  return d + "/" + rel;
}

// ---- file-local cast state (JS module-level _prevPose3/_smPose3/_thrust3…)
// Single-threaded like the JS main loop; replaces missing Game/Model fields.
struct CastState {
  bool init = false;
  int lastPos = -1000000, lastOral = -1;
  bool lastSolo = false;
  int prevIdx = 0, curIdx = 0;
  double blend = 1.0;
  Rig smHer, smHim;
  bool smInit = false;
  PoseEntry cur{};
  bool hasCur = false;
  double thrustY = 0, thrustZ = 0, shaftP = 0, breath = 0, faceAccum = 0;
  Face face{};
  bool glbActive = false;
  std::string identity, appliedIdentity;
  bool camInit = false;
  std::string glbFull[3];
  bool glbOk[3] = {false, false, false};
  bool rubActive = false;
  int rubKeys = 0; // bit0 himL,1 himR,2 herL,3 herR (anim3d.js:600-613)
};
inline CastState& castState() {
  static CastState s;
  return s;
}

} // namespace

bool Cast::loadAll(const char* assetDir) {
  // Same files as js3d (GLB_REGISTRY): goatchan.glb etc. under assets/.
  // him is procedural (buildHim3) — no GLB file, left unloaded by design.
  // Cast holds a single her Model, so the active-girl default (goatchan,
  // the glbModel.js:468 lazy-load default) loads now; kiyoko/anime paths
  // are resolved + recorded for the GLB_STORE stage (header T3).
  CastState& s = castState();
  if (!assetDir || !*assetDir) return false;
  for (int i = 0; i < 3; i++) {
    s.glbFull[i] = joinPath(assetDir, kGlbReg[i].rel);
    s.glbOk[i] = false;
  }
  s.glbOk[0] = her.load(s.glbFull[0].c_str()); // false until cgltf lands (T2)
  s.glbActive = her.loaded;
  return s.glbOk[0];
}

void Cast::pose(const Game& g, double dt) {
  // Mirrors updateAnim3 blend/damp/thrust layers (anim3d.js:556-641) over the
  // poses3d.js tables; forwards the same (Game, dt) into Model::pose, which
  // consumes the file-local evaluated joints once it gains channels (T3).
  CastState& s = castState();
  if (!(dt > 0)) dt = 0.016;
  if (dt > 0.05) dt = 0.05; // js3d/app3d.js:235 frame clamp

  const double oral = clamp(g.oral, 0.0, 1.0);
  const int posIdx = g.pos;
  const int oralNow = oral > 0.03 ? (isBlow(g) ? 2 : 1) : 0;
  const bool soloNow = g.solo;
  const std::string ident = poseIdentity(g); // shared 2D API (poses::name)
  const int tgtIdx = selectIndex(g);
  const auto& tab = poseTable();

  if (!s.init || posIdx != s.lastPos || oralNow != s.lastOral ||
      soloNow != s.lastSolo) {
    if (!s.init) {
      s.prevIdx = tgtIdx; // initChars3: _prevPose3 = currentPose3()
      s.blend = 1.0;
    } else {
      s.blend = 0.0; // keep _prevPose3 as blend start (anim3d.js:561)
    }
    s.lastPos = posIdx;
    s.lastOral = oralNow;
    s.lastSolo = soloNow;
    s.init = true;
  }
  PoseEntry tgt = evalBio(tab[static_cast<size_t>(tgtIdx)], g);
  PoseEntry cur = tgt;
  if (s.blend < 1.0) {
    s.blend = std::min(1.0, s.blend + dt * 2.6); // anim3d.js:573
    PoseEntry prv = evalBio(tab[static_cast<size_t>(s.prevIdx)], g);
    cur = lerpPose(prv, tgt, s.blend);
    if (s.blend >= 1.0) s.prevIdx = tgtIdx;
  } else {
    s.prevIdx = tgtIdx;
  }
  if (g.solo) {
    // Runtime solo hands override the authored table (anim3d.js:224-232,
    // used by solveHands3:241-242). Authored SOLO_POSE3.hands differs
    // slightly (herR x/y 0.04/-0.03 vs 0.05/-0.04; herL z 0.09 vs 0.08) —
    // the override wins in JS, so it wins here.
    cur.herL = mkHand(HKind::HerChest, -0.08, 0.04, 0.08);
    cur.herR = mkHand(HKind::HerHips, 0.05, -0.04, 0.12);
  }
  s.cur = cur;
  s.hasCur = true;
  s.curIdx = tgtIdx;
  s.identity = ident;

  if (!s.smInit) { // dampCfg3 layer needs a seed (anim3d.js:627-628)
    s.smHer = cur.her;
    s.smHim = cur.him;
    s.smInit = true;
  } else {
    dampRig(s.smHer, cur.her, dt);
    dampRig(s.smHim, cur.him, dt);
  }
  // Thrust drive: cowgirl rides vertical, horizontal poses slide Z, oral
  // stays put (anim3d.js:637-641). Added to the damped roots like JS.
  const bool cowgirl = oral <= 0.03 && (posIdx == 4 || posIdx == 5);
  const double depth = clamp(g.depth, 0.0, 1.0);
  s.thrustY = dampNum(s.thrustY, cowgirl ? (1.0 - depth) * 0.11 : 0.0, 8, dt);
  s.thrustZ = dampNum(s.thrustZ,
                      (!cowgirl && oral <= 0.03) ? (1.0 - depth) * 0.11 : 0.0,
                      8, dt);
  s.smHer.pos[1] += s.thrustY;
  s.smHim.pos[2] += s.thrustZ;
  // Shaft smoothing + breathing (anim3d.js:583-586,648-653,660-662).
  s.shaftP = dampNum(s.shaftP, g.shaftPulse, 7, dt);
  const double p = clamp(g.pleasure / 100.0, 0.0, 1.0);
  s.breath =
      std::sin(g.t * TAU * (0.16 + p * 0.0045)) * ((2.4 + p * 1.6) / 260.0);
  // Rub ownership (anim3d.js:597-613): which arms IK owns vs FK. Stored for
  // the solveHands3 stage; solo rub uses spaceHeld like JS (G.spaceHeld).
  const double rub = clamp(g.rub, 0.0, 1.0);
  s.rubActive = (rub > 0.03 || (g.solo && g.spaceHeld != 0)) &&
                g.state != "climax" && g.state != "finish";
  s.rubKeys = 0;
  if (s.rubActive) {
    const int zone = g.rubZone;
    if (g.solo) {
      if (zone == 3) s.rubKeys = (1 << 2) | (1 << 3);
      else if (zone == 1) s.rubKeys = (1 << 2);
      else if (zone == 2) s.rubKeys = (1 << 3);
      else s.rubKeys = (1 << 2) | (1 << 3);
    } else {
      if (zone == 3 || zone == 1) s.rubKeys = (1 << 1);
      else if (zone == 2) s.rubKeys = (1 << 0);
      else s.rubKeys = (1 << 0) | (1 << 1);
    }
  }
  // Face state at anim3d.js:869-873 cadence (1/26 s repaint gate lives in
  // the texture upload; state itself is cheap — updated every step).
  s.faceAccum += dt;
  s.face = faceFromGame(g);
  // GLB-vs-procedural visibility source (anim3d.js:854-857, AGENTS.md 3.3):
  // preset-derived until G.char.isGLB lands (header note b).
  s.glbActive = isGLBPreset(g.ch.preset);

  her.pose(g, dt); // Model::pose inputs driven with the same sim step
  if (!g.solo) him.pose(g, dt); // him hidden in solo (anim3d.js:854)
}

void Cast::draw(Engine& e, const Game& g) {
  CastState& s = castState();
  const auto& tab = poseTable();
  const PoseEntry& src = tab[static_cast<size_t>(s.init ? s.curIdx
                                                       : selectIndex(g))];
  // Camera preset on pose switch (poses3d.js applyPoseCam3:492-508). JS only
  // applies in orbit mode; C++ Camera has no mode yet — applied always (f).
  if (!s.camInit || s.identity != s.appliedIdentity) {
    e.cam.yaw = src.cam.yaw;
    e.cam.pitch = src.cam.pitch;
    e.cam.dist = src.cam.dist;
    e.cam.fov = src.cam.fov;
    e.cam.tx = src.cam.target[0];
    e.cam.ty = src.cam.target[1];
    e.cam.tz = src.cam.target[2];
    s.appliedIdentity = s.identity;
    s.camInit = true;
  }
  // Shared 2D face API linkage (js/side.js herExpression via paintFace3 in
  // anim3d.js:872-873). Stub is a no-op today; static scratch avoids a
  // 1280x720 alloc per frame. Real E comes from s.face (faceFromGame).
  {
    static Canvas scratch;
    Game mut = g; // herExpression takes Game&; revisit to const& (note c)
    side::herExpression(scratch, mut);
  }
  // Mutual exclusivity (AGENTS.md 3.3): solo hides him; her Model IS the GLB
  // surface when active, so it always draws (procedural-dummy split is T3).
  (void)s.glbActive;
  her.draw(e, g);
  if (!g.solo) him.draw(e, g);
}

} // namespace ag::d3
