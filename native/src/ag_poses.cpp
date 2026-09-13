// Afterglow native — ag_poses.cpp
// 1:1 port of js/poses.js (522 lines): 7-pose table, Tab cycling, and the
// full pose-driven her/him rendering (side + fpv bodies).
// Numeric order of operations is preserved so frames match the web build.
//
// Routing note (js/app.js:9-21): the web app only calls drawPoseSide/drawPoseFPV
// when (G.pos|0)!==0; missionary (pos 0) is drawn by side.js/fpv.js instead.
// drawBodies() replicates the poses.js dispatch exactly (else-branch falls
// through to spoon), so a standalone pos-0 call renders spoon — callers that
// own the missionary pass (side::/fpv::draw) should keep the app.js guard.
//
// Room/bed/light/fluids are caller-owned (app::draw); this file draws bodies
// only. drawPoseFPV's drawFPVRoom()/drawFPVFluids() wrappers are intentionally
// skipped here. poseJetOrigins/spawnJets (fluid origins for mechanics) are not
// part of the ag_2dmods.h poses API and stay with the mechanics port.
#include "ag_2dmods.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace ag::poses {

// ---- pose table (js/poses.js:5-13) ----
static const char* const POS_NAMES[7] = {
    "MISSIONARY",      "LEGS-UP / DEEP", "DOGGY (ARCHED)", "PRONE BONE (FLAT)",
    "COWGIRL (RIDING)", "REV COWGIRL",    "SPOONING"};
// Lines feed say(line,1.8) once the speech module lands (TODO below).
static const char* const POS_LINES[7] = {
    "like this\xE2\x80\xA6 look into my eyes\xE2\x80\xA6 \xE2\x99\xA5",
    "wrap my legs high\xE2\x80\xA6 put it all in\xE2\x80\xA6 \xE2\x99\xA5",
    "take me from behind\xE2\x80\xA6 arch my back\xE2\x80\xA6 \xE2\x99\xA5",
    "push me flat into the sheets\xE2\x80\xA6 so deep\xE2\x80\xA6 \xE2\x99\xA5",
    "my turn on top\xE2\x80\xA6 I\xE2\x80\x99m going to ride you\xE2\x80\xA6 \xE2\x99\xA5",
    "watch me bounce for you\xE2\x80\xA6 don\xE2\x80\x99t stop\xE2\x80\xA6 \xE2\x99\xA5",
    "hold me tight from behind\xE2\x80\xA6 slow and warm\xE2\x80\xA6 \xE2\x99\xA5"};

// Visual-only randomness (js Math.random in spawnJets, js/poses.js:37-43).
// drawBodies takes no rng param, so this stays file-local; same call sequence
// => same frame. Currently reserved for future jet-origin parity.
static Rng pos_rng(0x9E3779B97F4A7C15ull);
[[maybe_unused]] static F64 pos_rr(F64 a, F64 b) { return pos_rng.range(a, b); }

// TODO(audio/speech): cyclePose also runs say(line,1.8) (js/audio.js:474) and
// playMoan(.38,{dur:.42,pmul:1.16,vol:.85}) (js/audio.js:173). No-ops until the
// audio/charui ports land; call order in cycle() is preserved.
static void pos_say(Game& g, std::string_view txt, F64 dur) {
  (void)g;
  (void)txt;
  (void)dur;
}
static void pos_playMoan(F64 i, F64 dur, F64 pmul, F64 vol) {
  (void)i;
  (void)dur;
  (void)pmul;
  (void)vol;
}

// ---- expression (js/side.js:7-36, file-local: ag side::herExpression is a
// void stub in ag_port_todo.cpp, so poses keeps its own copy) ----
struct PosExpr {
  F64 eye = 0, rolled = 0, mouth = 0, blush = 0, brow = 0, tilt = 0;
};
static PosExpr pos_herExpression(const Game& g) {
  F64 p = g.pleasure / 100.0;
  F64 ar = g.ar / 100.0;
  F64 t = g.t;
  (void)t;
  F64 eye = lerp(0.72, 0.12, sm(0.08, 0.92, p));
  F64 rolled = 0;
  F64 mouth = p * 0.36;
  F64 blush = 0.12 + p * 0.44 + ar * 0.22;
  F64 brow = lerp(-0.06, 0.46, sm(0.20, 0.90, p));
  F64 tilt = 0.12 + p * 0.0035;
  // TODO(mech/audio): JS adds mouth-pop transients from G.mouths entries
  // {t0,dur,i} (js/audio.js:182, js/side.js:17-22). Native MoanMouth is
  // {x,y,t}, so that contribution is ignored until the transient unifies.
  if (!g.speech.empty()) mouth += 0.28;
  if (g.blinkPh > 0) eye *= (1 - g.blinkPh);
  if (g.state == "orgasm") {
    F64 e = std::sin((TAU / 2) * clamp(g.orgT / 5.2, 0.0, 1.0));
    rolled = 0.70 + e * 0.30;
    eye = lerp(eye, 0.06, e);
    mouth = std::max(mouth, 0.88 * e);
    tilt = 0.50;
    blush = 1.0;
    brow = 0.65;
  }
  if (g.after > 0) {
    eye = std::min(eye, 0.16);
    mouth = std::max(mouth, 0.18);
    blush = std::max(blush, 0.45);
  }
  if (g.state == "finish" && g.finishT > 1) {
    eye = 0.06;
    mouth = 0.22;
  }
  if (g.kiss > 0.6) mouth = std::min(mouth, 0.45);
  return {clamp(eye, 0.0, 1.0), clamp(rolled, 0.0, 1.0),
          clamp(mouth, 0.0, 1.0), clamp(blush, 0.0, 1.0), brow, tilt};
}

// ---- tone helpers (file-local: skDark/skLight/herFarT/himFarT live in
// js/skin.js:9-11 + js/side.js:41-42, not in ag_skin.h) ----
static std::string pos_skDark(std::string_view h, F64 t) {
  return lerpHex(h, "#3a1410", clamp(t, 0.0, 1.0));
}
static std::string pos_skLight(std::string_view h, F64 t) {
  return lerpHex(h, "#fff4ea", clamp(t, 0.0, 1.0));
}
static Tone pos_herT(const Game& g) { return herT(g, chars::getSkin(g)); }
static Tone pos_himT(const Game& g) { return himT(g, chars::getSkin(g)); }
static Tone pos_herFarT(const Game& g) {
  return skTone(pos_skDark(chars::getSkin(g).her, 0.30));
}
static Tone pos_himFarT(const Game& g) {
  return skTone(pos_skDark(chars::getSkin(g).him, 0.30));
}

// Dynamic rgba builder: integer-range rgb + %.17g alpha (matches ag_skin.cpp).
static std::string pos_rgba3(const std::array<float, 3>& rgb, F64 a) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", (double)rgb[0],
                (double)rgb[1], (double)rgb[2], a);
  return std::string(buf);
}

// ---- hair (file-local: js/skin.js:308-340, not in ag_skin.h) ----
static void pos_hairMassS(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 rot,
                           std::string_view col) {
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, rot, 0, TAU);
  std::string lc = pos_skLight(col, 0.22), dk = pos_skDark(col, 0.40);
  cv.setFillGrad(gRadial(cv, cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy,
                         rx * 1.2,
                         {{0.0, lc}, {0.55, std::string(col)}, {1.0, dk}}));
  cv.fill();
}
static void pos_tressS(Canvas& cv, F64 x0, F64 y0, F64 c1x, F64 c1y, F64 c2x,
                       F64 c2y, F64 x1, F64 y1, F64 w0, F64 w1,
                       std::string_view col, std::string_view sheen = {}) {
  auto off = [&](F64 t, F64 w, F64& ox, F64& oy) {
    F64 mt = 1 - t;
    F64 bx = mt * mt * mt * x0 + 3 * mt * mt * t * c1x + 3 * mt * t * t * c2x +
             t * t * t * x1;
    F64 by = mt * mt * mt * y0 + 3 * mt * mt * t * c1y + 3 * mt * t * t * c2y +
             t * t * t * y1;
    F64 dx = 3 * mt * mt * (c1x - x0) + 6 * mt * t * (c2x - c1x) +
             3 * t * t * (x1 - c2x);
    F64 dy = 3 * mt * mt * (c1y - y0) + 6 * mt * t * (c2y - c1y) +
             3 * t * t * (y1 - c2y);
    F64 L = std::hypot(dx, dy);
    if (!(L > 0)) L = 1;
    ox = bx - dy / L * w;
    oy = by + dx / L * w;
  };
  cv.beginPath();
  F64 ox = 0, oy = 0;
  off(0, lerp(w0, w1, 0.0), ox, oy);
  cv.moveTo(ox, oy);
  for (F64 t = 0.2; t <= 1.001; t += 0.2) {
    off(t, lerp(w0, w1, t), ox, oy);
    cv.lineTo(ox, oy);
  }
  for (F64 t = 1.0; t >= -0.001; t -= 0.2) {
    off(t, -lerp(w0, w1, t), ox, oy);
    cv.lineTo(ox, oy);
  }
  cv.closePath();
  cv.setFillColorStr(col);
  cv.fill();
  if (!sheen.empty()) {
    cv.save();
    cv.setCompStr("soft-light");
    cv.setStrokeColorStr(sheen);
    cv.setLineWidth(std::max<F64>(1, w0 * 0.3));
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(x0, y0);
    cv.bezierCurveTo(c1x, c1y, c2x, c2y, x1, y1);
    cv.stroke();
    cv.restore();
  }
}

// ---- scene-local capsule (js/poses.js:47-51): every limb via skin limbS ----
static void pos_capsule(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2,
                        const Tone& T) {
  LimbOpt opt;
  opt.belly = 1.06;
  limbS(cv, a, b, r1, r2, T, opt);
}

// ---- her head (js/poses.js:53-81) ----
static void pos_poseHead(Canvas& cv, const Game& g, F64 x, F64 y, F64 dir,
                         const PosExpr& E, F64 s = 1) {
  std::string hc = g.ch.hairColor.empty() ? "#231318" : g.ch.hairColor;
  pos_hairMassS(cv, x - dir * 12 * s, y - 4 * s, 24 * s, 26 * s, dir * 0.22,
                hc);
  Tone T = pos_herT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, 16 * s, 20 * s, 0, 0, TAU); },
            T, x - dir * 5 * s, y - 7 * s, 2 * s, 26 * s);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, 16 * s, 20 * s, 0, 0, TAU); },
           [&]() {
             fAO(cv, x - dir * 10 * s, y + 2 * s, 7 * s, 14 * s, 0.24);
             fHi(cv, x + dir * 4 * s, y - 9 * s, 7 * s, 9 * s,
                 "rgba(255,240,225,0.26)");
           });
  cv.setStrokeColorStr("rgba(65,35,42,.95)");
  cv.setLineWidth(2.2 * s);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(x + dir * 2 * s, y - 4 * s);
  cv.quadraticCurveTo(x + dir * 7 * s, y - 2 * s, x + dir * 12 * s, y - 4 * s);
  cv.stroke();
  {
    std::string bc = g.ch.blushColor.empty() ? "#e86070" : g.ch.blushColor;
    std::string col = pos_rgba3(hexToRgb(bc), 0.10 + E.blush * 0.26);
    shade(cv, x + dir * 5 * s, y + 4 * s, 9 * s, 6 * s, col, 0);
  }
  F64 mo = E.mouth;
  cv.setFillColorStr(g.ch.lipColor.empty() ? "#b3555f" : g.ch.lipColor);
  cv.beginPath();
  cv.ellipse(x + dir * 11 * s, y + 8 * s, 3 + mo * 3.2 * s, 2 + mo * 5 * s, 0,
             0, TAU);
  cv.fill();
  if (mo > 0.2) {
    cv.setFillColorStr("#5a1622");
    cv.beginPath();
    cv.ellipse(x + dir * 11 * s, y + 8 * s, 1.8 + mo * 1.8 * s,
               1.2 + mo * 3.2 * s, 0, 0, TAU);
    cv.fill();
  }
  cv.setFillColorStr("rgba(255,225,225,.34)");
  cv.beginPath();
  cv.ellipse(x + dir * 10 * s, y + 6 * s, 1.5 * s, 1 * s, 0, 0, TAU);
  cv.fill();
  pos_tressS(cv, x - dir * 8 * s, y - 18 * s, x + dir * 8 * s, y + 2 * s,
             x + dir * 2 * s, y + 18 * s, x - dir * 2 * s, y + 32 * s, 4.2 * s,
             1.2 * s, hc, "rgba(255,200,210,0.14)");
}

// ---- his head (js/poses.js:83-95) ----
static void pos_himHead(Canvas& cv, const Game& g, F64 x, F64 y, F64 dir) {
  Tone T = pos_himT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, 18, 20, 0, 0, TAU); }, T,
            x - dir * 6, y - 7, 2, 26);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, 18, 20, 0, 0, TAU); }, [&]() {
    fAO(cv, x - dir * 12, y + 4, 8, 15, 0.22);
    fHi(cv, x + dir * 4, y - 10, 7, 8, "rgba(255,238,220,0.20)");
  });
  cv.setFillColorStr(pos_skDark(T.base, 0.42));
  cv.beginPath();
  cv.moveTo(x - dir * 18, y - 12);
  cv.quadraticCurveTo(x, y - 32, x + dir * 18, y - 12);
  cv.quadraticCurveTo(x + dir * 10, y - 16, x, y - 14);
  cv.quadraticCurveTo(x - dir * 10, y - 12, x - dir * 18, y - 12);
  cv.fill();
  cv.setStrokeColorStr("rgba(74,44,47,.9)");
  cv.setLineWidth(2);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(x + dir * 3, y - 1);
  cv.lineTo(x + dir * 11, y - 1);
  cv.stroke();
}

// ---- breast mound (js/poses.js:97-120; E kept for signature parity) ----
static void pos_poseBreast(Canvas& cv, const Game& g, F64 x, F64 y, F64 r,
                           const PosExpr& E) {
  (void)E;
  if (!std::isfinite(r) || r <= 0) r = 18;
  F64 er = clamp(g.ar / 100.0, 0.0, 1.0);
  Tone T = pos_herT(g);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, x + r * 0.12, y + r * 1.06, r * 0.88, r * 0.34,
        "rgba(118,58,48,.30)", 0);
  cv.restore();
  cv.beginPath();
  cv.ellipse(x, y, r * 0.95, r * 1.15, 0, 0, TAU);
  cv.setFillGrad(gRadial(cv, x - r * 0.28, y - r * 0.44, r * 0.12, x, y,
                         r * 1.3,
                         {{0.0, T.hi}, {0.5, T.b}, {0.85, T.s}, {1.0, T.s}}));
  cv.fill();
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, r * 0.95, r * 1.15, 0, 0, TAU); },
           [&]() {
             fAO(cv, x, y + r * 0.92, r * 0.9, r * 0.4, 0.30);
             fSh(cv, x - r * 0.62, y + r * 0.1, r * 0.42, r * 0.86,
                 "rgba(175,100,80,0.24)", 0);
             fHi(cv, x - r * 0.24, y - r * 0.44, r * 0.42, r * 0.46,
                 "rgba(255,238,222,0.30)");
             fSSS(cv, x + r * 0.3, y + r * 0.3, r * 0.5, r * 0.6, 0);
           });
  F64 aer = r * 0.26 + er * r * 0.07, npr = r * 0.14 + er * r * 0.09;
  cv.setFillGrad(gRadial(cv, x - aer * 0.2, y + r * 0.52 - aer * 0.2, 0, x,
                         y + r * 0.52, aer * 1.15,
                         {{0.0, "rgba(226,150,128,0.95)"},
                          {1.0, "rgba(196,112,96,0.72)"}}));
  cv.beginPath();
  cv.arc(x, y + r * 0.52, aer, 0, TAU);
  cv.fill();
  cv.setFillColorStr(g.ch.nippleColor.empty() ? "#c25f63" : g.ch.nippleColor);
  cv.beginPath();
  cv.arc(x, y + r * 0.52 + npr * 0.4, npr, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,240,235,.42)");
  cv.beginPath();
  cv.ellipse(x - npr * 0.35, y + r * 0.52 - npr * 0.1, npr * 0.45, npr * 0.32,
             0, 0, TAU);
  cv.fill();
}

// ---- glute / mons mound (js/poses.js:122-132) ----
static void pos_poseGlute(Canvas& cv, const Game& g, F64 x, F64 y, F64 rx,
                          F64 ry, F64 dir = 0, F64 rot = 0) {
  Tone T = pos_herT(g);
  F64 rMax = std::max(rx, ry);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, rx, ry, rot, 0, TAU); }, T,
            x - rx * 0.3, y - ry * 0.42, rx * 0.1, rMax * 1.35);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, rx, ry, rot, 0, TAU); }, [&]() {
    if (dir) fAO(cv, x - dir * rx * 0.6, y + ry * 0.08, rx * 0.3, ry * 0.82,
                 0.28);
    fAO(cv, x, y + ry * 0.86, rx * 0.78, ry * 0.24, 0.22);
    fHi(cv, x - rx * 0.3, y - ry * 0.4, rx * 0.44, ry * 0.4,
        "rgba(255,240,225,0.26)");
    fSSS(cv, x + rx * 0.42, y + ry * 0.28, rx * 0.4, ry * 0.5, 0);
  });
}

// ---- shaded glans cap (js/poses.js:134-141) ----
static void pos_poseGlans(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry,
                          F64 pu = 1) {
  F64 rMax = std::max(rx, ry);
  cv.setFillGrad(gRadial(cv, x - rx * 0.3, y - ry * 0.4, 1, x, y, rMax * 1.15,
                         {{0.0, "#e9aa8f"},
                          {0.6, "#c7846f"},
                          {1.0, "#ab6a58"}}));
  cv.beginPath();
  cv.ellipse(x, y, rx * pu, ry * pu, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,240,232,.3)");
  cv.beginPath();
  cv.ellipse(x - rx * 0.28, y - ry * 0.38, rx * 0.3, ry * 0.2, -0.4, 0, TAU);
  cv.fill();
}

// ---- vulva placement (js/poses.js:143-148; body in skin vulvaS stub) ----
static void pos_poseVulva(Canvas& cv, const Game& g, F64 x, F64 y, F64 open,
                          F64 angle = 0, std::string_view view = "side",
                          F64 sc = 0.78) {
  F64 eng = clamp(g.ar / 100.0, 0.0, 1.0);
  cv.save();
  cv.translate(x, y);
  cv.rotate(angle);
  cv.scale(sc, sc);
  VulvaOpt opt;
  opt.view = std::string(view);
  vulvaS(cv, 0, 0, open, eng, pos_herT(g), opt);
  cv.restore();
}

// ---- shaft + glans (js/poses.js:150-164) ----
static void pos_poseShaft(Canvas& cv, const Game& g, V2 base, V2 tip,
                          F64 pu = 1) {
  Tone T = pos_himT(g);
  LimbOpt opt;
  opt.belly = 1.04;
  limbS(cv, base, tip, 12 * pu, 10 * pu, T, opt);
  cv.setStrokeColorStr("rgba(150,85,65,.22)");
  cv.setLineWidth(2);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(base.x, base.y - 4);
  cv.quadraticCurveTo(lerp(base.x, tip.x, 0.5) + 2,
                      lerp(base.y, tip.y, 0.5) - 5, tip.x - 6, tip.y);
  cv.stroke();
  cv.setFillGrad(gRadial(cv, tip.x - 3 * pu, tip.y - 4 * pu, 1, tip.x, tip.y,
                         11 * pu,
                         {{0.0, "#e9aa8f"},
                          {0.6, "#c7846f"},
                          {1.0, "#b67562"}}));
  cv.beginPath();
  cv.ellipse(tip.x, tip.y, 9.5 * pu, 10.5 * pu, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,240,232,.28)");
  cv.beginPath();
  cv.ellipse(tip.x - 2.5 * pu, tip.y - 4 * pu, 3.2 * pu, 2.2 * pu, -0.4, 0,
             TAU);
  cv.fill();
}

// ---------------- side-view positions ----------------
// NOTE js/poses.js:180+: `sk`/`HIS` locals are dead (never read); omitted here.

// POS 1: LEGS-UP / DEEP MISSIONARY (js/poses.js:179-209)
static void pos_legsUpSide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)br;
  F64 D = g.depth, jig = g.breast.p * 0.85;
  pos_capsule(cv, {620, 490}, {680, 360}, 28, 20, pos_herFarT(g));
  pos_capsule(cv, {680, 360}, {770, 330}, 18, 12, pos_herFarT(g));
  pos_capsule(cv, {365, 520}, {600, 480}, 36, 32, pos_herT(g));
  pos_poseGlute(cv, g, 620, 485, 42, 36, 0, -0.25);
  F64 open = 4 + 9 * D;
  V2 V{655, 482};
  pos_poseVulva(cv, g, V.x, V.y, open, 0.3);
  pos_poseBreast(cv, g, 450, 478 + jig * 0.5, 18, E);
  pos_poseBreast(cv, g, 487, 472 + jig * 0.5, 17, E);
  pos_capsule(cv, {625, 485}, {710, 380}, 30, 22, pos_herT(g));
  pos_capsule(cv, {710, 380}, {805, 355}, 20, 13, pos_herFarT(g));
  pos_poseHead(cv, g, 295, 518, -1, E);
  pos_capsule(cv, {810, 560}, {760, 460}, 40, 34, pos_himT(g));
  pos_capsule(cv, {760, 460}, {695, 385}, 44, 38, pos_himT(g));
  pos_himHead(cv, g, 678, 350, -1);
  pos_capsule(cv, {720, 410}, {685, 445}, 18, 14, pos_himT(g));
  {
    HandOpt ho;
    ho.curl = 0.7;
    ho.spread = 0.3;
    handS(cv, g, 682, 448, 3.93, 1.05, pos_himT(g), ho);
  }
  V2 B{765, 510};
  F64 pu = 1 + 0.28 * g.shaftPulse;
  V2 tip{lerp(B.x, V.x, 0.3 + 0.68 * D), lerp(B.y, V.y, 0.3 + 0.68 * D)};
  pos_poseShaft(cv, g, B, tip, pu);
}

// POS 2: DOGGY STYLE (js/poses.js:212-249)
static void pos_doggySide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)br;
  F64 D = g.depth, jig = g.breast.p * 0.9;
  pos_capsule(cv, {365, 475}, {345, 540}, 17, 14, pos_herT(g));
  pos_capsule(cv, {345, 540}, {360, 578}, 13, 10, pos_herFarT(g));
  pos_capsule(cv, {368, 460}, {595, 425}, 35, 30, pos_herT(g));
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 480, 432, 68, 22, "rgba(255,230,205,.30)", -0.1);
  cv.restore();
  pos_poseGlute(cv, g, 625, 420, 42, 46, 0);
  {
    std::string col = pos_rgba3(hexToRgb(pos_herT(g).dk), 0.24);
    cv.setStrokeColorStr(col);
    cv.setLineWidth(2);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(642, 382);
    cv.quadraticCurveTo(652, 416, 642, 450);
    cv.stroke();
  }
  F64 open = 4 + 9 * D;
  V2 V{668, 442};
  pos_poseVulva(cv, g, V.x, V.y, open, 0.15);
  pos_poseBreast(cv, g, 398, 490 + jig * 0.5, 18, E);
  pos_poseBreast(cv, g, 434, 484 + jig * 0.5, 17, E);
  pos_capsule(cv, {608, 445}, {582, 560}, 32, 24, pos_herT(g));
  pos_capsule(cv, {582, 560}, {638, 575}, 22, 14, pos_herFarT(g));
  pos_capsule(cv, {330, 498}, {372, 464}, 13, 16, pos_herT(g));
  pos_poseHead(cv, g, 318, 496, -1, E);
  pos_capsule(cv, {835, 565}, {798, 465}, 36, 32, pos_himT(g));
  pos_capsule(cv, {798, 465}, {740, 360}, 42, 36, pos_himT(g));
  pos_himHead(cv, g, 724, 324, -1);
  pos_capsule(cv, {750, 380}, {698, 412}, 20, 16, pos_himT(g));
  pos_capsule(cv, {698, 412}, {660, 426}, 15, 12, pos_himT(g));
  {
    HandOpt ho;
    ho.curl = 0.7;
    ho.spread = 0.3;
    handS(cv, g, 658, 427, 4.36, 1.0, pos_himT(g), ho);
  }
  V2 B{788, 455};
  F64 pu = 1 + 0.28 * g.shaftPulse;
  V2 tip{lerp(B.x, V.x, 0.3 + 0.68 * D), lerp(B.y, V.y, 0.3 + 0.68 * D)};
  pos_poseShaft(cv, g, B, tip, pu);
}

// POS 3: PRONE BONE (js/poses.js:252-281)
static void pos_proneBoneSide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)br;
  F64 D = g.depth;
  pos_poseHead(cv, g, 290, 526, -1, E);
  pos_capsule(cv, {330, 528}, {610, 505}, 32, 28, pos_herT(g));
  pos_poseGlute(cv, g, 628, 495, 38, 36, 0);
  pos_capsule(cv, {628, 505}, {790, 535}, 28, 18, pos_herT(g));
  pos_capsule(cv, {790, 535}, {910, 545}, 18, 12, pos_herFarT(g));
  F64 open = 3 + 8 * D;
  V2 V{650, 490};
  pos_poseVulva(cv, g, V.x, V.y, open, 0.25);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 500, 492, 150, 15, "rgba(64,30,24,.34)", -0.06);
  cv.restore();
  pos_capsule(cv, {420, 495}, {680, 470}, 42, 36, pos_himT(g));
  pos_capsule(cv, {680, 470}, {830, 510}, 36, 26, pos_himT(g));
  pos_himHead(cv, g, 400, 470, -1);
  pos_capsule(cv, {480, 485}, {530, 515}, 20, 16, pos_himT(g));
  pos_capsule(cv, {530, 515}, {575, 525}, 15, 12, pos_himT(g));
  {
    HandOpt ho;
    ho.curl = 0.68;
    ho.spread = 0.3;
    handS(cv, g, 576, 525, 1.79, 0.95, pos_himT(g), ho);
  }
  V2 B{720, 495};
  F64 pu = 1 + 0.28 * g.shaftPulse;
  V2 tip{lerp(B.x, V.x, 0.3 + 0.68 * D), lerp(B.y, V.y, 0.3 + 0.68 * D)};
  pos_poseShaft(cv, g, B, tip, pu);
}

// POS 4: COWGIRL (js/poses.js:284-312)
static void pos_cowgirlSide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)br;
  F64 D = g.depth, jig = g.breast.p * 0.95;
  F64 hipsY = 510 - 42 * D;
  pos_himHead(cv, g, 950, 548, 1);
  pos_capsule(cv, {928, 545}, {710, 540}, 40, 35, pos_himT(g));
  pos_capsule(cv, {710, 540}, {555, 556}, 32, 24, pos_himT(g));
  F64 pu = 1 + 0.28 * g.shaftPulse;
  {
    LimbOpt opt;
    opt.belly = 1.04;
    limbS(cv, {706, 535}, {706, hipsY + 8}, 12 * pu, 10 * pu, pos_himT(g),
          opt);
  }
  pos_poseGlute(cv, g, 700, hipsY, 36, 28, 0);
  pos_capsule(cv, {700, hipsY - 10}, {688, hipsY - 170}, 35, 28,
              pos_herT(g));
  pos_poseBreast(cv, g, 668, hipsY - 135 + jig * 0.6, 18, E);
  pos_poseBreast(cv, g, 708, hipsY - 133 + jig * 0.6, 17, E);
  pos_capsule(cv, {688, hipsY - 158}, {658, hipsY - 68}, 17, 14,
              pos_herT(g));
  pos_capsule(cv, {658, hipsY - 68}, {638, hipsY - 36}, 13, 10,
              pos_herFarT(g));
  pos_capsule(cv, {692, hipsY - 158}, {732, hipsY - 68}, 17, 14,
              pos_herT(g));
  pos_capsule(cv, {732, hipsY - 68}, {750, hipsY - 36}, 13, 10,
              pos_herFarT(g));
  {
    HandOpt ho;
    ho.curl = 0.55;
    ho.spread = 0.3;
    handS(cv, g, 636, hipsY - 34, 3.70, 0.85, pos_herT(g), ho);
    handS(cv, g, 752, hipsY - 34, 2.63, 0.85, pos_herT(g), ho);
  }
  pos_capsule(cv, {700, hipsY}, {590, 560}, 28, 22, pos_herT(g));
  pos_capsule(cv, {700, hipsY}, {810, 560}, 28, 22, pos_herT(g));
  pos_poseHead(cv, g, 686, hipsY - 208, -1, E);
}

// POS 5: REVERSE COWGIRL (js/poses.js:315-336)
static void pos_revCowgirlSide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)br;
  F64 D = g.depth, jig = g.breast.p * 0.9;
  (void)jig;
  F64 hipsY = 510 - 42 * D;
  pos_himHead(cv, g, 450, 545, -1);
  pos_capsule(cv, {480, 545}, {710, 540}, 40, 35, pos_himT(g));
  pos_capsule(cv, {710, 540}, {880, 555}, 34, 26, pos_himT(g));
  F64 pu = 1 + 0.28 * g.shaftPulse;
  {
    LimbOpt opt;
    opt.belly = 1.04;
    limbS(cv, {706, 535}, {706, hipsY + 8}, 12 * pu, 10 * pu, pos_himT(g),
          opt);
  }
  pos_poseGlute(cv, g, 705, hipsY, 38, 30, 0);
  pos_capsule(cv, {705, hipsY - 10}, {718, hipsY - 170}, 36, 28,
              pos_herT(g));
  pos_poseGlute(cv, g, 688, hipsY + 4, 28, 32, 0, -0.2);
  pos_capsule(cv, {715, hipsY - 155}, {760, hipsY - 65}, 17, 14,
              pos_herT(g));
  pos_capsule(cv, {760, hipsY - 65}, {800, hipsY - 30}, 13, 10,
              pos_herFarT(g));
  {
    HandOpt ho;
    ho.curl = 0.5;
    ho.spread = 0.3;
    handS(cv, g, 802, hipsY - 28, 2.29, 0.9, pos_herT(g), ho);
  }
  pos_poseHead(cv, g, 724, hipsY - 205, 1, E);
}

// POS 6: SPOONING (js/poses.js:339-367)
static void pos_spoonSide(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth;
  pos_himHead(cv, g, 405, 496, -1);
  pos_capsule(cv, {440, 502}, {680, 506}, 38, 34, pos_himT(g));
  pos_capsule(cv, {680, 510}, {535, 562}, 34, 26, pos_himT(g));
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 500, 502, 140, 12, "rgba(64,30,24,.30)", 0.02);
  cv.restore();
  pos_poseHead(cv, g, 295, 520, -1, E);
  pos_capsule(cv, {330, 524}, {605, 526}, 34, 28, pos_herT(g));
  pos_poseGlute(cv, g, 620, 522, 34, 36, 0);
  pos_capsule(cv, {518, 498}, {560, 516}, 20, 16, pos_himT(g));
  pos_capsule(cv, {560, 516}, {602, 522}, 15, 12, pos_himT(g));
  {
    HandOpt ho;
    ho.curl = 0.68;
    ho.spread = 0.3;
    handS(cv, g, 604, 522, 1.71, 0.95, pos_himT(g), ho);
  }
  pos_capsule(cv, {605, 528}, {700, 548}, 26, 20, pos_herT(g));
  pos_capsule(cv, {700, 548}, {782, 556}, 18, 12, pos_herFarT(g));
  F64 open = 3 + 8 * D;
  V2 V{646, 522};
  pos_poseVulva(cv, g, V.x, V.y, open, 0.2);
  V2 B{670, 508};
  F64 pu = 1 + 0.28 * g.shaftPulse;
  V2 tip{lerp(B.x, V.x, 0.3 + 0.68 * D), lerp(B.y, V.y, 0.3 + 0.68 * D)};
  pos_poseShaft(cv, g, B, tip, pu);
}

// Side dispatcher (js/poses.js:167-176).
static void pos_drawSide(Canvas& cv, Game& g) {
  PosExpr E = pos_herExpression(g);
  F64 br = std::sin(g.t * TAU * (0.16 + g.pleasure * 0.004)) * 2.2;
  int p = g.pos;
  if (p < 0 || p >= COUNT) p = COUNT - 1; // JS else-branch -> spoon
  if (p == 1) pos_legsUpSide(cv, g, E, br);
  else if (p == 2) pos_doggySide(cv, g, E, br);
  else if (p == 3) pos_proneBoneSide(cv, g, E, br);
  else if (p == 4) pos_cowgirlSide(cv, g, E, br);
  else if (p == 5) pos_revCowgirlSide(cv, g, E, br);
  else pos_spoonSide(cv, g, E, br);
}

// ---------------- first-person view positions ----------------

// FPV 1: LEGS-UP (js/poses.js:384-401)
static void pos_legsUpFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth, pu = 1 + 0.28 * g.shaftPulse;
  pos_capsule(cv, {640, 680}, {640, 430}, 95, 72, pos_herT(g));
  pos_poseGlute(cv, g, 640, 570, 72, 48, 0);
  F64 open = 5 + 10 * D;
  pos_poseVulva(cv, g, 640, 548, open, 0, "front", 1.9);
  for (F64 s : {-1.0, 1.0}) {
    pos_capsule(cv, {640 + s * 65, 570}, {640 + s * 180, 360}, 44, 32,
                pos_herT(g));
    pos_capsule(cv, {640 + s * 180, 360}, {640 + s * 120, 220}, 30, 20,
                pos_herFarT(g));
  }
  F64 tipY = lerp(650.0, 548.0, D);
  pos_capsule(cv, {640, 795}, {640, tipY}, 26 * pu, 17 * pu, pos_himT(g));
  pos_poseGlans(cv, 640, tipY, 17, 12, pu);
}

// FPV 2: DOGGY (js/poses.js:404-427)
static void pos_doggyFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth, pu = 1 + 0.28 * g.shaftPulse;
  pos_capsule(cv, {640, 710}, {640, 390}, 92, 68, pos_herT(g));
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 640, 510, 32, 110, "rgba(255,230,205,.28)", 0);
  cv.restore();
  Tone T = pos_herT(g);
  for (F64 s : {-1.0, 1.0}) pos_poseGlute(cv, g, 640 + s * 48, 560, 50, 62, s);
  {
    std::string col = pos_rgba3(hexToRgb(T.dk), 0.26);
    cv.setStrokeColorStr(col);
    cv.setLineWidth(2.4);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(640, 506);
    cv.quadraticCurveTo(646, 552, 640, 600);
    cv.stroke();
  }
  F64 open = 5 + 10 * D;
  pos_poseVulva(cv, g, 640, 588, open, 0, "front", 2.1);
  for (F64 s : {-1.0, 1.0}) {
    pos_capsule(cv, {640 + s * 230, 770}, {640 + s * 86, 615}, 32, 25,
                pos_himFarT(g));
    HandOpt ho;
    ho.curl = 0.66;
    ho.spread = 0.3;
    handS(cv, g, 640 + s * 82, 612, -s * 0.75, 1.4, pos_himFarT(g), ho);
  }
  F64 tipY = lerp(695.0, 592.0, D);
  pos_capsule(cv, {640, 805}, {640, tipY}, 26 * pu, 17 * pu, pos_himT(g));
  if (D < 0.55) pos_poseGlans(cv, 640, tipY - 5, 17, 12, pu);
}

// FPV 3: PRONE BONE (js/poses.js:430-445)
static void pos_proneBoneFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth, pu = 1 + 0.28 * g.shaftPulse;
  pos_capsule(cv, {640, 710}, {640, 360}, 88, 64, pos_herT(g));
  pos_poseGlute(cv, g, 640, 580, 70, 54, 0);
  for (F64 s : {-1.0, 1.0})
    pos_capsule(cv, {640 + s * 38, 600}, {640 + s * 32, 740}, 34, 26,
                pos_herFarT(g));
  F64 open = 4 + 9 * D;
  pos_poseVulva(cv, g, 640, 584, open, 0, "front", 1.8);
  F64 tipY = lerp(680.0, 582.0, D);
  pos_capsule(cv, {640, 795}, {640, tipY}, 25 * pu, 16 * pu, pos_himT(g));
}

// FPV 4: COWGIRL (js/poses.js:448-495)
static void pos_cowgirlFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  F64 D = g.depth, jig = g.breast.p * 0.95;
  F64 bsz = 0.7 + g.ch.breastSize * 0.6;
  F64 hipsY = 600 - 60 * D;
  Tone HT = pos_himT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(640, 812, 220, 95, 0, 0, TAU); },
            HT, 640, 772, 20, 250);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(640, 812, 220, 95, 0, 0, TAU); },
           [&]() {
             fAO(cv, 640, 744, 130, 26, 0.24);
             fHi(cv, 596, 790, 92, 30, "rgba(255,238,220,0.16)");
           });
  F64 pu = 1 + 0.28 * g.shaftPulse;
  pos_capsule(cv, {640, 790}, {640, hipsY + 6}, 24 * pu, 16 * pu,
              pos_himT(g));
  for (F64 s : {-1.0, 1.0})
    pos_capsule(cv, {640 + s * 65, hipsY}, {s < 0 ? 515 : 765, 725}, 46, 36,
                pos_herT(g));
  pos_poseGlute(cv, g, 640, hipsY, 74, 42, 0);
  pos_capsule(cv, {640, hipsY - 10}, {640, 360}, 62, 48, pos_herT(g));
  for (F64 s : {-1.0, 1.0}) {
    F64 bx = 640 + s * 62, by = 330 + jig * 0.7 + br * 0.5;
    pos_poseBreast(cv, g, bx, by, 34 * bsz, E);
  }
  shade(cv, 640, 338 + jig * 0.7 + br * 0.5, 11, 40 * bsz,
        "rgba(160,92,70,0.24)", 0);
  std::string hc = g.ch.hairColor.empty() ? "#231318" : g.ch.hairColor;
  Tone FT = pos_herT(g);
  pos_hairMassS(cv, 640, 220, 56, 52, 0, hc);
  for (F64 s : {-1.0, 1.0})
    pos_tressS(cv, 640 + s * 34, 238, 640 + s * 48, 292, 640 + s * 46, 342,
               640 + s * 40, 384, 20, 7, hc, "rgba(255,200,210,0.12)");
  skFillRad(cv, [&](Canvas& c) { c.ellipse(640, 232, 32, 36, 0, 0, TAU); },
            FT, 640, 216, 4, 48);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(640, 232, 32, 36, 0, 0, TAU); },
           [&]() {
             fAO(cv, 640, 264, 26, 14, 0.22);
             fHi(cv, 640, 212, 20, 12, "rgba(255,240,225,0.24)");
           });
  cv.setStrokeColorStr("rgba(65,35,42,.95)");
  cv.setLineWidth(2.6);
  cv.setLineCapStr("round");
  for (F64 s : {-1.0, 1.0}) {
    cv.beginPath();
    cv.moveTo(640 + s * 8, 229);
    cv.quadraticCurveTo(640 + s * 16, 234, 640 + s * 24, 228);
    cv.stroke();
  }
  {
    std::string bc = g.ch.blushColor.empty() ? "#e86070" : g.ch.blushColor;
    std::string col = pos_rgba3(hexToRgb(bc), 0.10 + E.blush * 0.26);
    for (F64 s : {-1.0, 1.0}) shade(cv, 640 + s * 20, 243, 12, 8, col, 0);
  }
  F64 mo = E.mouth;
  cv.setFillColorStr(g.ch.lipColor.empty() ? "#b3555f" : g.ch.lipColor);
  cv.beginPath();
  cv.ellipse(640, 253, 6 + mo * 3, 3 + mo * 5, 0, 0, TAU);
  cv.fill();
  if (mo > 0.2) {
    cv.setFillColorStr("#5a1622");
    cv.beginPath();
    cv.ellipse(640, 253, 3 + mo * 2, 1.6 + mo * 3.2, 0, 0, TAU);
    cv.fill();
  }
  cv.setFillColorStr("rgba(255,225,225,.3)");
  cv.beginPath();
  cv.ellipse(638, 250, 2, 1.2, 0, 0, TAU);
  cv.fill();
}

// FPV 5: REVERSE COWGIRL (js/poses.js:498-511)
static void pos_revCowgirlFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth, pu = 1 + 0.28 * g.shaftPulse;
  (void)pu;
  F64 hipsY = 580 - 55 * D;
  pos_capsule(cv, {640, hipsY - 10}, {640, 330}, 64, 48, pos_herT(g));
  for (F64 s : {-1.0, 1.0})
    pos_poseGlute(cv, g, 640 + s * 52, hipsY, 54, 64, s);
  Tone T = pos_herT(g);
  {
    std::string col = pos_rgba3(hexToRgb(T.dk), 0.26);
    cv.setStrokeColorStr(col);
    cv.setLineWidth(2.4);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(640, hipsY - 48);
    cv.quadraticCurveTo(646, hipsY, 640, hipsY + 48);
    cv.stroke();
  }
  F64 open = 5 + 9 * D;
  pos_poseVulva(cv, g, 640, hipsY + 15, open, 0, "front", 2.0);
}

// FPV 6: SPOONING (js/poses.js:514-522)
static void pos_spoonFPV(Canvas& cv, Game& g, const PosExpr& E, F64 br) {
  (void)E;
  (void)br;
  F64 D = g.depth, pu = 1 + 0.28 * g.shaftPulse;
  pos_capsule(cv, {420, 730}, {700, 300}, 86, 60, pos_herT(g));
  pos_poseGlute(cv, g, 500, 622, 56, 72, 0, 0.2);
  F64 open = 4 + 8 * D;
  pos_poseVulva(cv, g, 558, 602, open, 0.2, "side", 1.5);
  V2 tip{lerp(560.0, 560.0, 0.3 + 0.68 * D), lerp(700.0, 602.0, 0.3 + 0.68 * D)};
  pos_capsule(cv, {560, 700}, tip, 20 * pu, 15 * pu, pos_himT(g));
}

// FPV dispatcher (js/poses.js:370-381, minus room/fluids wrappers).
static void pos_drawFPV(Canvas& cv, Game& g) {
  PosExpr E = pos_herExpression(g);
  F64 br = std::sin(g.t * TAU * (0.16 + g.pleasure * 0.004)) * 2;
  int p = g.pos;
  if (p < 0 || p >= COUNT) p = COUNT - 1; // JS else-branch -> spoon
  if (p == 1) pos_legsUpFPV(cv, g, E, br);
  else if (p == 2) pos_doggyFPV(cv, g, E, br);
  else if (p == 3) pos_proneBoneFPV(cv, g, E, br);
  else if (p == 4) pos_cowgirlFPV(cv, g, E, br);
  else if (p == 5) pos_revCowgirlFPV(cv, g, E, br);
  else pos_spoonFPV(cv, g, E, br);
}

// ---- exported API (ag_2dmods.h) ----

// Tab order over the 7 poses (js/poses.js:16-21).
void cycle(Game& g) {
  if (g.state != "play" || g.tired) return;
  int cur = g.pos % COUNT;
  if (cur < 0) cur += COUNT;
  g.pos = (cur + 1) % COUNT;
  g.nod = 1;
  pos_say(g, POS_LINES[g.pos], 1.8);
  pos_playMoan(0.38, 0.42, 1.16, 0.85);
}

// REAL pose names for the HUD button (js/poses.js:5-15).
std::string name(const Game& g) {
  int p = g.pos;
  if (p < 0 || p >= COUNT) p = 0;
  return std::string(POS_NAMES[p]);
}

void drawBodies(Canvas& cv, Game& g) {
  if (g.view == "fpv") pos_drawFPV(cv, g);
  else pos_drawSide(cv, g);
}

} // namespace ag::poses
