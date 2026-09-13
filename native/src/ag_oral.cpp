// Afterglow native — ag_oral.cpp
// 1:1 port of js/oral.js (690 lines): cinematic fellatio side + FPV.
// Shared helpers used here but owned elsewhere are copied file-local with an
// oral_ prefix (himHead/poseBreast/poseBreastSide/poseGlute <- poses.js,
// hairMassS/tressS <- skin.js, room/fluids/tremor <- fpv.js, herExpression
// <- side.js). TODO(dedupe): promote to ag_skin.h/ag_2dmods.h once all
// callers agree on homes.
#include "ag_2dmods.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace ag {
namespace oral {
namespace {

Rng oral_rng(9182);
std::string s_oralMode; // G.oralMode has no native field yet (see anim TODO)

struct Expr {
  F64 eye = 0.5, rolled = 0, mouth = 0, blush = 0.3, brow = 0, tilt = 0.12;
};

// js/side.js:7-36 herExpression (MoanMouth mapped t->t0, dur 0.8, i 0.6).
Expr oralExpr(const Game& g) {
  F64 p = g.pleasure / 100, ar = g.ar / 100, t = g.t;
  Expr e;
  e.eye = lerp(0.72, 0.12, sm(0.08, 0.92, p));
  e.mouth = p * 0.36;
  e.blush = 0.12 + p * 0.44 + ar * 0.22;
  e.brow = lerp(-0.06, 0.46, sm(0.20, 0.90, p));
  e.tilt = 0.12 + p * 0.0035;
  for (const auto& m : g.mouths) {
    F64 u = (t - m.t) / 0.8;
    if (u > 0 && u < 1) e.mouth += std::pow(std::sin(3.141592653589793 * u), 0.75) * 0.6 * 0.65;
  }
  if (!g.speech.empty()) e.mouth += 0.28;
  if (g.blinkPh > 0) e.eye *= (1 - g.blinkPh);
  if (g.state == "orgasm") {
    F64 oe = std::sin(3.141592653589793 * clamp(g.orgT / 5.2, 0.0, 1.0));
    e.rolled = 0.70 + oe * 0.30;
    e.eye = lerp(e.eye, 0.06, oe);
    e.mouth = std::max(e.mouth, 0.88 * oe);
    e.tilt = 0.50;
    e.blush = 1.0;
    e.brow = 0.65;
  }
  if (g.after > 0) {
    e.eye = std::min(e.eye, 0.16);
    e.mouth = std::max(e.mouth, 0.18);
    e.blush = std::max(e.blush, 0.45);
  }
  if (g.state == "finish" && g.finishT > 1) {
    e.eye = 0.06;
    e.mouth = 0.22;
  }
  if (g.kiss > 0.6) e.mouth = std::min(e.mouth, 0.45);
  e.eye = clamp(e.eye, 0.0, 1.0);
  e.rolled = clamp(e.rolled, 0.0, 1.0);
  e.mouth = clamp(e.mouth, 0.0, 1.0);
  e.blush = clamp(e.blush, 0.0, 1.0);
  return e;
}

Tone oralHerT(const Game& g) { return herT(g, chars::getSkin(g)); }
Tone oralHimT(const Game& g) { return himT(g, chars::getSkin(g)); }
Tone oralHerFarT(const Game& g) {
  return skTone(lerpHex(chars::getSkin(g).her, "#3a1410", 0.30));
}

struct OralK {
  F64 D = 0, vel = 0, pu = 1, wet = 0, bob = 0, suck = 0, swallow = 0;
  Expr E;
  SkinPair sk;
};
OralK oralK(const Game& g) {
  OralK k;
  k.D = clamp(g.depth, 0.0, 1.0);
  k.vel = g.vel;
  k.pu = 1 + 0.30 * g.shaftPulse;
  k.wet = clamp(0.18 + k.D * 0.55 + std::fabs(k.vel) * 0.45 + g.ar / 180, 0.0, 1.0);
  k.bob = std::sin(g.t * 7.4) * std::min(std::fabs(k.vel) * 9, 7.0);
  k.suck = clamp(std::fabs(k.vel) * 1.1 + k.D * 0.55, 0.0, 1.0);
  k.swallow = k.D > 0.52
                  ? std::max(0.0, std::sin(g.t * 2.6) * 0.55 + 0.45) * ((k.D - 0.52) / 0.48)
                  : 0;
  k.E = oralExpr(g);
  k.sk = chars::getSkin(g);
  return k;
}
struct OralC {
  std::string hair = "#231318", eye = "#4a2c33", lip = "#b3555f",
              blush = "#e86070", nip = "#c25f63";
};
OralC oralCol(const Game& g) {
  OralC c;
  c.hair = g.ch.hairColor;
  c.eye = g.ch.eyeColor;
  c.lip = g.ch.lipColor;
  c.blush = g.ch.blushColor;
  c.nip = g.ch.nippleColor;
  return c;
}

void lipRing(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 holeX, F64 holeY,
             F64 ang, const std::string& lip) {
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, ang, 0, TAU);
  cv.ellipse(cx, cy, holeX, holeY, ang, 0, TAU, true);
  cv.setFillColorStr(lip);
  cv.fillEvenOdd();
}
void oralSaliva(Canvas& cv, F64 ax, F64 ay, F64 bx, F64 by, F64 cpx, F64 cpy,
                F64 a) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(255,248,242,%.17g)", a);
  cv.setStrokeColorStr(buf);
  cv.setLineWidth(1.7);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(ax, ay);
  cv.quadraticCurveTo(cpx, cpy, bx, by);
  cv.stroke();
}
void oralCatch(Canvas& cv, F64 x, F64 y, F64 r) {
  cv.setFillColorStr("rgba(255,255,255,.93)");
  cv.beginPath();
  cv.arc(x, y, r, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,255,255,.45)");
  cv.beginPath();
  cv.arc(x + r * 0.9, y + r * 0.8, r * 0.42, 0, TAU);
  cv.fill();
}

// js/poses.js:83 himHead
void oralHimHead(Canvas& cv, const Game& g, F64 x, F64 y, F64 dir) {
  Tone T = oralHimT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, 18, 20, 0, 0, TAU); }, T,
            x - dir * 6, y - 7, 2, 26);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, 18, 20, 0, 0, TAU); }, [&]() {
    fAO(cv, x - dir * 12, y + 4, 8, 15, 0.22);
    fHi(cv, x + dir * 4, y - 10, 7, 8, "rgba(255,238,220,0.20)");
  });
  cv.setFillColorStr(lerpHex(T.base, "#3a1410", 0.42));
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

// js/poses.js:97 poseBreast
void oralPoseBreast(Canvas& cv, const Game& g, F64 x, F64 y, F64 r,
                    const Expr& E) {
  (void)E;
  if (!std::isfinite(r) || r <= 0) r = 18;
  F64 er = clamp(g.ar / 100, 0.0, 1.0);
  Tone T = oralHerT(g);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, x + r * 0.12, y + r * 1.06, r * 0.88, r * 0.34,
        "rgba(118,58,48,.30)", 0);
  cv.restore();
  cv.setFillGrad(gRadial(cv, x - r * 0.28, y - r * 0.44, r * 0.12, x, y,
                         r * 1.3,
                         {{0, T.hi}, {0.5, T.b}, {0.85, T.s}, {1, T.s}}));
  cv.beginPath();
  cv.ellipse(x, y, r * 0.95, r * 1.15, 0, 0, TAU);
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
                         {{0, "rgba(226,150,128,0.95)"},
                          {1, "rgba(196,112,96,0.72)"}}));
  cv.beginPath();
  cv.arc(x, y + r * 0.52, aer, 0, TAU);
  cv.fill();
  cv.setFillColorStr(g.ch.nippleColor);
  cv.beginPath();
  cv.arc(x, y + r * 0.52 + npr * 0.4, npr, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,240,235,.42)");
  cv.beginPath();
  cv.ellipse(x - npr * 0.35, y + r * 0.52 - npr * 0.1, npr * 0.45, npr * 0.32,
             0, 0, TAU);
  cv.fill();
}

// js/oral.js:669 poseBreastSide
void oralPoseBreastSide(Canvas& cv, const Game& g, F64 x, F64 y, F64 r,
                        const Expr& E) {
  (void)E;
  if (!std::isfinite(r) || r <= 0) r = 18;
  F64 er = clamp(g.ar / 100, 0.0, 1.0);
  Tone T = oralHerT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, r * 0.96, r * 1.16, 0.07, 0, TAU); },
            T, x - r * 0.3, y - r * 0.46, r * 0.1, r * 1.35);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, x, y + r * 0.55, r * 0.72, r * 0.42, "rgba(120,60,50,.18)", 0);
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, x - r * 0.28, y - r * 0.22, r * 0.3, r * 0.2,
        "rgba(255,235,218,.40)", -0.3);
  cv.restore();
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(210,130,115,%.17g)", 0.74 + er * 0.22);
  cv.setFillColorStr(buf);
  cv.beginPath();
  cv.arc(x, y + r * 0.52, 4.9 + er * 1.6, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(190,120,105,.65)");
  for (int i = 0; i < 6; i++) {
    F64 a = i / 6.0 * TAU + 0.3;
    cv.beginPath();
    cv.arc(x + std::cos(a) * 4.2, y + r * 0.52 + std::sin(a) * 4.2, 0.7, 0, TAU);
    cv.fill();
  }
  cv.setFillColorStr(g.ch.nippleColor);
  cv.beginPath();
  cv.arc(x, y + r * 0.52 + 1.2, 2.5 + 1.9 * er, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,240,236,.35)");
  cv.beginPath();
  cv.arc(x - 1, y + r * 0.52, 1.1, 0, TAU);
  cv.fill();
}

// js/poses.js:123 poseGlute
void oralPoseGlute(Canvas& cv, const Game& g, F64 x, F64 y, F64 rx, F64 ry,
                   F64 dir, F64 rot) {
  Tone T = oralHerT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(x, y, rx, ry, rot, 0, TAU); }, T,
            x - rx * 0.3, y - ry * 0.42, rx * 0.1,
            std::max(rx, ry) * 1.35);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(x, y, rx, ry, rot, 0, TAU); }, [&]() {
    if (dir) fAO(cv, x - dir * rx * 0.6, y + ry * 0.08, rx * 0.3, ry * 0.82, 0.28);
    fAO(cv, x, y + ry * 0.86, rx * 0.78, ry * 0.24, 0.22);
    fHi(cv, x - rx * 0.3, y - ry * 0.4, rx * 0.44, ry * 0.4,
        "rgba(255,240,225,0.26)");
    fSSS(cv, x + rx * 0.42, y + ry * 0.28, rx * 0.4, ry * 0.5, 0);
  });
}

// js/skin.js:335 hairMassS
void oralHairMass(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 rot,
                  const std::string& col) {
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, rot, 0, TAU);
  cv.setFillGrad(gRadial(
      cv, cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy, rx * 1.2,
      {{0, lerpHex(col, "#fff4ea", 0.22)}, {0.55, col}, {1, lerpHex(col, "#3a1410", 0.4)}}));
  cv.fill();
}
// js/skin.js:308 tressS
void oralTress(Canvas& cv, F64 x0, F64 y0, F64 c1x, F64 c1y, F64 c2x, F64 c2y,
               F64 x1, F64 y1, F64 w0, F64 w1, const std::string& col,
               const std::string& sheen) {
  F64 P[4][2] = {{x0, y0}, {c1x, c1y}, {c2x, c2y}, {x1, y1}};
  auto off = [&](F64 t, F64 w, F64& ox, F64& oy) {
    F64 mt = 1 - t;
    F64 bx = mt * mt * mt * P[0][0] + 3 * mt * mt * t * P[1][0] +
             3 * mt * t * t * P[2][0] + t * t * t * P[3][0];
    F64 by = mt * mt * mt * P[0][1] + 3 * mt * mt * t * P[1][1] +
             3 * mt * t * t * P[2][1] + t * t * t * P[3][1];
    F64 dx = 3 * mt * mt * (P[1][0] - P[0][0]) + 6 * mt * t * (P[2][0] - P[1][0]) +
             3 * t * t * (P[3][0] - P[2][0]);
    F64 dy = 3 * mt * mt * (P[1][1] - P[0][1]) + 6 * mt * t * (P[2][1] - P[1][1]) +
             3 * t * t * (P[3][1] - P[2][1]);
    F64 L = std::hypot(dx, dy);
    if (!(L > 0)) L = 1;
    ox = bx - dy / L * w;
    oy = by + dx / L * w;
  };
  cv.beginPath();
  F64 ox = 0, oy = 0;
  off(0, w0, ox, oy);
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
    cv.setLineWidth(std::max(1.0, w0 * 0.3));
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(x0, y0);
    cv.bezierCurveTo(c1x, c1y, c2x, c2y, x1, y1);
    cv.stroke();
    cv.restore();
  }
}

// js/fpv.js:8 fpvTremor
F64 oralFPVTremor(const Game& g) {
  F64 org = g.state == "orgasm"
                ? std::sin(3.141592653589793 * clamp(g.orgT / 5.2, 0.0, 1.0))
                : 0;
  F64 highAr = clamp((g.ar - 60) / 40, 0.0, 1.0);
  F64 shiver = org * (std::sin(g.t * 38) * 0.65 + std::sin(g.t * 54) * 0.35 +
                      std::cos(g.t * 19) * 0.2) *
               5.2;
  F64 arousalTremor =
      highAr * (std::sin(g.t * 22) * 0.15 + std::cos(g.t * 31) * 0.1) * 2.0;
  return shiver + arousalTremor;
}

// js/fpv.js:19 drawFPVRoom
void oralFPVRoom(Canvas& cv, const Game& g) {
  cv.setFillColorStr("#0f090d");
  cv.fillRect(0, 0, VW, VH);
  cv.setFillGrad(gLinear(cv, 0, 0, 0, 300,
                         {{0, "#1a1016"}, {0.7, "#130c11"}, {1, "#0c070a"}}));
  cv.fillRect(0, 0, VW, 280);
  const F64 wx = 960, wy = 24, ww = 180, wh = 140;
  cv.setFillColorStr("rgba(120,145,170,0.06)");
  cv.fillRect(wx, wy, ww, wh);
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gRadial(cv, wx + 130, wy + 35, 6, wx + 130, wy + 35, 65,
                         {{0, "rgba(230,242,255,0.22)"},
                          {0.4, "rgba(180,210,240,0.06)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.beginPath();
  cv.arc(wx + 130, wy + 35, 65, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(235,245,255,0.85)");
  cv.beginPath();
  cv.arc(wx + 130, wy + 35, 16, 0, TAU);
  cv.fill();
  cv.restore();
  cv.setStrokeColorStr("rgba(25,16,22,0.85)");
  cv.setLineWidth(4);
  cv.strokeRect(wx, wy, ww, wh);
  cv.beginPath();
  cv.moveTo(wx + ww / 2, wy);
  cv.lineTo(wx + ww / 2, wy + wh);
  cv.moveTo(wx, wy + wh * 0.45);
  cv.lineTo(wx + ww, wy + wh * 0.45);
  cv.stroke();
  const F64 lx = 180, ly = 110;
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gRadial(cv, lx, ly, 8, lx, ly, 460,
                         {{0, "rgba(255,195,125,0.28)"},
                          {0.35, "rgba(255,165,95,0.12)"},
                          {0.7, "rgba(255,140,80,0.03)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.beginPath();
  cv.arc(lx, ly, 460, 0, TAU);
  cv.fill();
  cv.restore();
  cv.setFillColorStr("#2d1820");
  cv.beginPath();
  cv.moveTo(148, 134);
  cv.lineTo(212, 134);
  cv.lineTo(198, 98);
  cv.lineTo(162, 98);
  cv.closePath();
  cv.fill();
  cv.setFillColorStr("rgba(255,215,145,0.7)");
  cv.beginPath();
  cv.moveTo(162, 100);
  cv.lineTo(198, 100);
  cv.lineTo(206, 132);
  cv.lineTo(154, 132);
  cv.closePath();
  cv.fill();
  cv.setFillGrad(gLinear(cv, 0, 260, 0, VH,
                         {{0, "#421d28"},
                          {0.35, "#2e131b"},
                          {0.75, "#1e0a11"},
                          {1, "#11050a"}}));
  cv.fillRect(0, 250, VW, VH - 250);
  F64 bounce = g.depth * 8 + g.impact * 6;
  cv.save();
  cv.setCompStr("multiply");
  cv.setStrokeColorStr("rgba(12,3,6,0.52)");
  cv.setLineWidth(3.2);
  for (int i = 0; i < 7; i++) {
    F64 y = 380 + i * 46;
    cv.beginPath();
    cv.moveTo(70 + ((i * 61) % 120), y);
    cv.bezierCurveTo(460, y - 26 + ((i * 29) % 24) + bounce * 0.4, 820,
                     y + 16 - ((i * 19) % 20), 1210 - ((i * 53) % 140), y);
    cv.stroke();
  }
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  cv.setStrokeColorStr("rgba(255,220,195,0.22)");
  cv.setLineWidth(4.5);
  for (int i = 0; i < 5; i++) {
    F64 y = 390 + i * 50;
    cv.beginPath();
    cv.moveTo(100 + ((i * 53) % 90), y - 6);
    cv.bezierCurveTo(520, y - 28 + ((i * 23) % 20), 760, y + 8, 1160, y - 4);
    cv.stroke();
  }
  cv.restore();
  cv.setFillGrad(gLinear(cv, 640 - 170, 120, 640 + 170, 214,
                         {{0, "#43332a"},
                          {0.35, "#382b22"},
                          {0.75, "#281e17"},
                          {1, "#1d1611"}}));
  cv.beginPath();
  cv.ellipse(640, 170, 170, 44, -0.01, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  cv.setFillGrad(gRadial(cv, 640, 158, 6, 640, 158, 44,
                         {{0, "rgba(80,55,40,0.35)"},
                          {0.65, "rgba(90,60,45,0.14)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.beginPath();
  cv.ellipse(640, 158, 70, 22, 0, 0, TAU);
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 640, 485 + bounce * 0.4, 210, 140, "rgba(10,3,7,0.58)", 0);
  cv.restore();
}

// js/fpv.js:1006 drawFPVFluids
void oralFPVFluids(Canvas& cv, const Game& g) {
  for (const auto& d : g.drips) {
    F64 y = lerp(560, 655, d.p);
    F64 x = 640 + (d.x - 648) * 0.5 + std::sin(d.p * 9.5 + d.j) * 1.6;
    cv.setFillColorStr("rgba(255,248,244,0.72)");
    cv.beginPath();
    cv.ellipse(x, y, 2.6, 4.2, 0, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,255,255,0.95)");
    cv.beginPath();
    cv.arc(x - 0.7, y - 1.2, 1.0, 0, TAU);
    cv.fill();
    cv.setStrokeColorStr("rgba(255,248,244,0.32)");
    cv.setLineWidth(1.3);
    cv.beginPath();
    cv.moveTo(x, y - 4);
    cv.lineTo(x, y - 10);
    cv.stroke();
  }
  for (const auto& s : g.glisten) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,250,245,%.17g)", s.a * 0.65);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(640 + (s.x - 640) * 0.3, 556 + (s.y - 546) * 0.4, 2.8, 1.8, 0,
               0, TAU);
    cv.fill();
  }
  for (const auto& j : g.jets) {
    if (j.view != 'f') continue;
    F64 spd = std::hypot(j.vx, j.vy);
    F64 a = std::atan2(j.vy, j.vx);
    F64 al = clamp(j.life * 2.4, 0.0, 1.0);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,250,246,%.17g)", 0.85 * al);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(j.x, j.y, 3.5 + spd * 0.007, 2.2, a, 0, TAU);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(255,250,246,%.17g)", 0.35 * al);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(j.x - std::cos(a) * 8, j.y - std::sin(a) * 8, 2.2, 1.5, a, 0,
               TAU);
    cv.fill();
  }
  {
    int i = 0;
    for (const auto& s : g.sweat) {
      F64 a = 0.45 * s.life;
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(235,248,255,%.17g)", a);
      cv.setFillColorStr(buf);
      cv.beginPath();
      if (i % 2) cv.ellipse(640 + (s.x - 318) * 1.5, 140 + (s.y - 462) * 0.5, 1.5, 2.4, 0, 0, TAU);
      else cv.ellipse(640 + (s.x - 318) * 2.2, 300 + (s.y - 462) * 1.2, 1.5, 2.4, 0, 0, TAU);
      cv.fill();
      i++;
    }
  }
  for (const auto& h : g.hearts) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,105,140,%.17g)",
                  0.58 * std::min(1.0, h.life));
    cv.setFillColorStr(buf);
    heartPath(cv, 640 + (h.x - 655) + std::sin(h.ph) * 8,
              340 + (h.y - 485) * 0.8, h.s);
    cv.fill();
  }
}

void drawOralSide(Canvas& cv, Game& g) {
  OralK K = oralK(g);
  OralC C = oralCol(g);
  F64 D = K.D, vel = K.vel, pu = K.pu, wet = K.wet, bob = K.bob,
      suck = K.suck, swallow = K.swallow;
  Expr E = K.E;
  SkinPair sk = K.sk;
  Tone herT = oralHerT(g), himT = oralHimT(g), herFarT = oralHerFarT(g);

  oralHimHead(cv, g, 286, 540, -1);
  capsuleTone(cv, {332, 534}, {692, 528}, 46, 36);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 478, 526, 96, 24, "rgba(110,60,40,.20)", 0);
  shade(cv, 560, 532, 64, 16, "rgba(90,48,32,.14)", 0);
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 508, 520, 84, 32, "rgba(255,214,172,.40)", 0);
  cv.restore();
  capsuleTone(cv, {692, 534}, {566, 446}, 38, 28);
  capsuleTone(cv, {566, 446}, {444, 560}, 28, 18);
  capsuleTone(cv, {692, 538}, {506, 568}, 34, 22);
  footS(cv, 500, 566, 2.98, 1.15, himT);
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 618, 498, 44, 54, "rgba(70,36,22,.24)", -0.28);
  cv.restore();

  V2 base{708, 522};
  F64 ang = -0.56, len = 96;
  V2 tip{base.x + std::cos(ang) * len, base.y + std::sin(ang) * len};
  F64 mouthX = lerp(tip.x + 10, base.x + 22, D) + bob * 0.15;
  F64 mouthY = lerp(tip.y - 8, base.y - 18, D) + bob * 0.35;
  F64 inside = lerp(10, 38, D);
  (void)inside;

  capsuleTone(cv, base, tip, 13.4 * pu, 11.2 * pu);
  cv.setStrokeColorStr("rgba(142,72,56,.40)");
  cv.setLineWidth(2.3);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(base.x + 5, base.y - 7);
  cv.quadraticCurveTo(lerp(base.x, tip.x, 0.46) + 3, lerp(base.y, tip.y, 0.46) - 7,
                      tip.x - 10, tip.y + 3);
  cv.stroke();
  cv.setLineWidth(1.35);
  cv.beginPath();
  cv.moveTo(lerp(base.x, tip.x, 0.28), base.y - 3);
  cv.quadraticCurveTo(lerp(base.x, tip.x, 0.58), lerp(base.y, tip.y, 0.58) + 3,
                      lerp(base.x, tip.x, 0.8), tip.y - 1);
  cv.stroke();
  cv.setFillGrad(gRadial(cv, tip.x - 3.4 * pu, tip.y - 4 * pu, 1, tip.x, tip.y,
                         12.6 * pu,
                         {{0, "#eaa98e"}, {0.58, "#d89078"}, {1, "#ae6854"}}));
  cv.beginPath();
  cv.ellipse(tip.x, tip.y, 10.4 * pu, 11.4 * pu, ang + 0.26, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  cv.setFillColorStr("rgba(150,62,50,.26)");
  cv.beginPath();
  cv.ellipse(tip.x - 1, tip.y + 1.2, 9.2 * pu, 4.6 * pu, ang + 0.12, 0, TAU);
  cv.fill();
  cv.restore();
  cv.setStrokeColorStr("rgba(176,86,70,.42)");
  cv.setLineWidth(1.5);
  cv.beginPath();
  cv.moveTo(tip.x - 3, tip.y + 6);
  cv.quadraticCurveTo(tip.x, tip.y + 9.5, tip.x + 4, tip.y + 5);
  cv.stroke();
  cv.setFillColorStr("rgba(255,250,244,.40)");
  cv.beginPath();
  cv.ellipse(tip.x - 3, tip.y - 3, 2.9, 4.3, ang, 0, TAU);
  cv.fill();
  if (wet > 0.12) {
    cv.save();
    cv.setCompStr("screen");
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,242,234,%.17g)", 0.18 + wet * 0.30);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(2.5);
    cv.beginPath();
    cv.moveTo(mouthX - 2, mouthY - 5);
    cv.lineTo(base.x + 14, base.y - 10);
    cv.stroke();
    cv.restore();
  }

  capsuleTone(cv, {988, 578}, {908, 570}, 18, 13);
  footS(cv, 994, 576, 0.12, 1.0, herFarT);
  capsuleTone(cv, {908, 570}, {876, 478}, 36, 30);
  capsuleTone(cv, {876, 478}, {824, 368}, 37, 30);
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 848, 424, 28, 58, "rgba(255,228,198,.36)", 0.08);
  cv.restore();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 862, 502, 20, 42, "rgba(108,52,42,.22)", 0.14);
  cv.restore();
  {
    PathFn hip = [](Canvas& c) {
      c.moveTo(824, 372);
      c.quadraticCurveTo(858, 400, 872, 454);
      c.quadraticCurveTo(890, 510, 868, 548);
      c.quadraticCurveTo(820, 500, 808, 430);
      c.quadraticCurveTo(802, 390, 824, 372);
      c.closePath();
    };
    F64 lg[4] = {806, 392, 886, 530};
    skFillShape(cv, hip, herT, lg);
  }
  oralPoseGlute(cv, g, 884, 500, 30, 36, 0, 0);

  F64 jig = g.breast.p * 0.78 + std::sin(g.t * 8) * vel * 1.15;
  oralPoseBreastSide(cv, g, 802, 408 + jig, 20, E);
  oralPoseBreastSide(cv, g, 836, 402 + jig * 0.86, 18, E);

  capsuleTone(cv, {826, 376}, {770, 450}, 18.5, 15);
  capsuleTone(cv, {770, 450}, {base.x + 8, base.y - 16}, 14, 11);
  handS(cv, g, base.x + 12, base.y - 14, ang + 3.141592653589793, 1.0, herT,
        HandOpt{0.78, 0.24});

  capsuleTone(cv, {830, 378}, {804, 456}, 17, 14);
  capsuleTone(cv, {804, 456}, {786, 536}, 13, 10);
  handS(cv, g, 784, 538, 3.36, 0.9, herT, HandOpt{0.3, 0.34});

  F64 headAng = -0.46 + 0.22 * D;
  F64 hx = mouthX + 21, hy = mouthY - 6;

  oralHairMass(cv, hx + 44, hy - 14, 30, 36, headAng + 0.04, C.hair);
  oralHairMass(cv, hx + 54, hy + 28, 22, 46, -0.2, C.hair);
  oralHairMass(cv, hx + 22, hy - 28, 22, 15, headAng - 0.22, C.hair);
  const std::string hSheen = "rgba(255,205,215,0.14)";
  oralTress(cv, hx + 8, hy - 20, hx, hy - 12, hx - 3, hy - 2, hx + 2, hy + 10,
            4.4, 1.2, C.hair, hSheen);
  oralTress(cv, hx + 56, hy + 6, hx + 64, hy + 22, hx + 66, hy + 40, hx + 58,
            hy + 56, 5, 1.4, C.hair, hSheen);
  oralTress(cv, hx + 36, hy - 34, hx + 24, hy - 41, hx + 12, hy - 38, hx + 6,
            hy - 26, 4.6, 1.2, C.hair, hSheen);
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, hx + 28, hy - 22, 20, 16, "rgba(255,236,222,.24)", headAng);
  cv.restore();

  {
    PathFn nk = [=](Canvas& c) {
      c.moveTo(hx + 2, hy - 6);
      c.quadraticCurveTo(hx + 10, hy - 28, hx + 14, hy - 50);
      c.quadraticCurveTo(hx + 26, hy - 52, hx + 26, hy - 38);
      c.quadraticCurveTo(hx + 20, hy - 16, hx + 16, hy + 2);
      c.closePath();
    };
    F64 lg[4] = {hx, hy - 50, hx + 26, hy};
    skFillShape(cv, nk, herT, lg);
  }
  if (D > 0.56) {
    F64 b = (D - 0.56) * 16 + swallow * 4;
    cv.setFillColorStr(herT.b);
    cv.beginPath();
    cv.ellipse(hx + 6, hy - 26, 9 + b * 0.42, 7.2 + b * 0.65, -0.5, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,220,198,.16)");
    cv.beginPath();
    cv.ellipse(hx + 4, hy - 30, 4.2, 3.2, -0.4, 0, TAU);
    cv.fill();
  }

  Tone FT = herT;
  skFillRad(cv, [&](Canvas& c) { c.ellipse(hx, hy, 20, 24, headAng, 0, TAU); },
            FT, hx - 7, hy - 9, 2, 30);
  cv.setFillColorStr(FT.s);
  cv.beginPath();
  cv.moveTo(hx - 10, hy + 8);
  cv.quadraticCurveTo(hx - 16, hy + 18, hx - 6, hy + 24);
  cv.quadraticCurveTo(hx + 6, hy + 26, hx + 12, hy + 16);
  cv.closePath();
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, hx - 6, hy + 10, 12, 8, "rgba(150,80,60,.18)", 0.2);
  shade(cv, hx + 10, hy - 4, 10, 12, "rgba(140,75,55,.12)", 0.3);
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, hx - 8, hy - 16, 8, 16, "rgba(255,236,214,.42)", -0.15);
  cv.restore();

  cv.setFillColorStr(sk.herSh);
  cv.beginPath();
  cv.ellipse(hx + 17, hy - 2, 5.4, 8.4, headAng + 0.28, 0, TAU);
  cv.fill();
  cv.setStrokeColorStr("rgba(150,90,80,.4)");
  cv.setLineWidth(1.2);
  cv.beginPath();
  cv.arc(hx + 17, hy - 2, 3.2, -1.1, 1.6);
  cv.stroke();

  bool eyeShut = E.eye < 0.16;
  F64 ex = hx - 7.2, ey = hy - 6.2;
  if (eyeShut) {
    cv.setStrokeColorStr("rgba(58,28,36,.96)");
    cv.setLineWidth(2.5);
    cv.beginPath();
    cv.moveTo(ex - 8, ey + 1);
    cv.quadraticCurveTo(ex, ey + 4, ex + 8, ey);
    cv.stroke();
    cv.setLineWidth(1.2);
    for (int i = 0; i < 5; i++) {
      F64 lxv = ex - 7 + i * 3.2;
      cv.beginPath();
      cv.moveTo(lxv, ey);
      cv.lineTo(lxv - 1.1, ey - 3.4);
      cv.stroke();
    }
  } else {
    cv.setFillColorStr("#f7f3f0");
    cv.beginPath();
    cv.ellipse(ex, ey, 5.6, 3.6, 0.2, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(214,150,146,.38)");
    cv.beginPath();
    cv.ellipse(ex - 3.8, ey + 0.2, 1.7, 1.4, 0, 0, TAU);
    cv.fill();
    cv.setFillColorStr(C.eye);
    cv.beginPath();
    cv.arc(ex - 0.6, ey - 0.5, 2.7, 0, TAU);
    cv.fill();
    cv.setFillColorStr("#11080a");
    cv.beginPath();
    cv.arc(ex - 0.6, ey - 0.55, 1.4, 0, TAU);
    cv.fill();
    oralCatch(cv, ex - 1.5, ey - 1.4, 1.05);
    cv.setStrokeColorStr("rgba(48,22,28,.95)");
    cv.setLineWidth(2.1);
    cv.beginPath();
    cv.moveTo(ex - 9, ey - 1.2);
    cv.quadraticCurveTo(ex, ey - 4.8, ex + 8, ey - 0.4);
    cv.stroke();
    cv.setLineWidth(1.1);
    for (int i = 0; i < 5; i++) {
      F64 lxv = ex - 8 + i * 3.4, ly = ey - 2.2 - std::sin(i * 0.7) * 0.7;
      cv.beginPath();
      cv.moveTo(lxv, ly);
      cv.lineTo(lxv - 1.3, ly - 3.4);
      cv.stroke();
    }
  }
  cv.setStrokeColorStr(C.hair);
  cv.setLineWidth(1.9);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(ex - 9, ey - 9);
  cv.quadraticCurveTo(ex, ey - 13 - E.brow * 5, ex + 9, ey - 8 - E.brow * 6);
  cv.stroke();

  {
    auto rgb = hexToRgb(C.blush);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", rgb[0], rgb[1],
                  rgb[2], 0.12 + E.blush * 0.42);
    cv.setFillColorStr(buf);
  }
  cv.beginPath();
  cv.ellipse(hx + 1, hy + 7, 10, 6.5, 0.1, 0, TAU);
  cv.fill();
  if (suck > 0.2) {
    cv.save();
    cv.setCompStr("multiply");
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(125,60,50,%.17g)", 0.16 + suck * 0.24);
    shade(cv, hx + 5, hy + 9, 8, 6, buf, 0.2);
    cv.restore();
  }

  F64 jaw = 5.8 + 5.4 * D + swallow * 1.2;
  cv.setFillColorStr("#3a0c14");
  cv.beginPath();
  cv.ellipse(mouthX - 1, mouthY, 6.2, jaw * 0.62, ang, 0, TAU);
  cv.fill();

  if (D < 0.55) {
    cv.setFillColorStr("#c46a78");
    cv.beginPath();
    cv.ellipse(mouthX - 2, mouthY + jaw * 0.25, 5.5, 3.2 + (1 - D) * 2, ang + 0.1,
               0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,200,210,.28)");
    cv.beginPath();
    cv.ellipse(mouthX - 3, mouthY + jaw * 0.15, 2.2, 1.2, ang, 0, TAU);
    cv.fill();
  }

  F64 shaftR = 11.5 * pu;
  lipRing(cv, mouthX, mouthY, shaftR + 7.2, jaw * 0.92, shaftR * 0.95,
          jaw * 0.48, ang, C.lip);
  cv.setStrokeColorStr("rgba(130,48,58,.5)");
  cv.setLineWidth(1.35);
  cv.beginPath();
  cv.moveTo(mouthX - 7, mouthY - jaw * 0.48);
  cv.quadraticCurveTo(mouthX - 1, mouthY - jaw * 0.72, mouthX + 4, mouthY - jaw * 0.44);
  cv.stroke();
  cv.setFillColorStr("rgba(255,248,244,.58)");
  cv.beginPath();
  cv.ellipse(mouthX + 1.6, mouthY + jaw * 0.36, 3.6, 2.1, -0.3, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,255,255,.20)");
  cv.beginPath();
  cv.ellipse(mouthX - 2.2, mouthY - jaw * 0.28, 2.3, 1.15, -0.2, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  cv.setStrokeColorStr("rgba(80,30,30,.28)");
  cv.setLineWidth(1.6);
  cv.beginPath();
  cv.ellipse(mouthX, mouthY, shaftR * 1.02, jaw * 0.5, ang, 0, TAU);
  cv.stroke();
  cv.restore();

  if (D > 0.1) {
    oralSaliva(cv, mouthX - 4, mouthY + 5, mouthX - 17, mouthY + 4, mouthX - 12,
               mouthY + 12, 0.40 + wet * 0.28);
    if (wet > 0.4) {
      oralSaliva(cv, mouthX + 3, mouthY + 6, mouthX + 7, mouthY + 22, mouthX + 9,
                 mouthY + 12, 0.35 + wet * 0.2);
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(255,250,244,%.17g)", 0.4 + wet * 0.25);
      cv.setFillColorStr(buf);
      cv.beginPath();
      cv.ellipse(mouthX + 7, mouthY + 24, 1.9, 2.8, 0, 0, TAU);
      cv.fill();
    }
    if (wet > 0.65) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(255,248,242,%.17g)", 0.3 + wet * 0.2);
      cv.setFillColorStr(buf);
      cv.beginPath();
      cv.ellipse(mouthX - 2, mouthY + jaw * 0.7 + 6, 2.4, 3.4, 0, 0, TAU);
      cv.fill();
    }
  }

  {
    F64 om = g.oral != 0 ? g.oral : 1;
    if (oral_rng.next01() < 0.032 * om)
      g.hearts.push_back({hx + oral_rng.range(-16, 16),
                          hy - 26 + oral_rng.range(-8, 8), 1.25,
                          oral_rng.next01() * TAU, oral_rng.range(0.38, 0.72)});
  }
}

void drawOralFPV(Canvas& cv, Game& g) {
  oralFPVRoom(cv, g);
  OralK K = oralK(g);
  OralC C = oralCol(g);
  F64 D = K.D, vel = K.vel, pu = K.pu, wet = K.wet, bob = K.bob,
      suck = K.suck, swallow = K.swallow;
  Expr E = K.E;
  (void)vel;
  SkinPair sk = K.sk;
  (void)sk;
  F64 trem = oralFPVTremor(g);
  F64 swayX = std::sin(g.t * 0.9) * 2.2 + trem * 0.3;
  F64 swayY = std::sin(g.t * TAU * 0.16) * 1.6;

  cv.save();
  cv.translate(swayX, swayY + bob * 0.15);

  F64 z = 1 + D * 0.22;
  F64 mouthY = lerp(478, 668, D) + bob;
  F64 hx = 640, hy = mouthY - 52 * z;

  oralHairMass(cv, hx, hy - 36 * z, 86 * z, 72 * z, 0, C.hair);
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    oralHairMass(cv, hx + s * 78 * z, hy + 58 * z, 34 * z, 96 * z, s * 0.16,
                 C.hair);
  }
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, hx - 10, hy - 52 * z, 46 * z, 24 * z, "rgba(255,232,218,.22)", 0);
  cv.restore();
  const std::string hSheen = "rgba(255,205,215,0.14)";
  oralTress(cv, hx - 78 * z, hy - 18 * z, hx - 104 * z, hy + 6 * z, hx - 110 * z,
            hy + 34 * z, hx - 96 * z, hy + 58 * z, 7 * z, 2 * z, C.hair, hSheen);
  oralTress(cv, hx + 78 * z, hy - 16 * z, hx + 106 * z, hy + 4 * z, hx + 112 * z,
            hy + 32 * z, hx + 98 * z, hy + 56 * z, 7 * z, 2 * z, C.hair, hSheen);
  cv.setFillColorStr(C.hair);
  cv.beginPath();
  cv.moveTo(hx - 70 * z, hy - 28 * z);
  cv.quadraticCurveTo(hx, hy - 78 * z, hx + 70 * z, hy - 28 * z);
  cv.quadraticCurveTo(hx + 40 * z, hy - 8 * z, hx + 18 * z, hy - 22 * z);
  cv.quadraticCurveTo(hx, hy - 48 * z, hx - 16 * z, hy - 20 * z);
  cv.quadraticCurveTo(hx - 42 * z, hy - 6 * z, hx - 70 * z, hy - 28 * z);
  cv.fill();

  Tone herT = oralHerT(g);
  {
    PathFn sh = [=](Canvas& c) {
      c.moveTo(hx - 46 * z, hy + 38 * z);
      c.quadraticCurveTo(hx - 130 * z, hy + 70 * z, hx - 160 * z, hy + 140 * z);
      c.quadraticCurveTo(hx, hy + 168 * z, hx + 160 * z, hy + 140 * z);
      c.quadraticCurveTo(hx + 130 * z, hy + 70 * z, hx + 46 * z, hy + 38 * z);
      c.closePath();
    };
    F64 lg[4] = {hx, hy + 32 * z, hx, hy + 166 * z};
    skFillShape(cv, sh, herT, lg);
    skClipIn(cv, sh, [&]() {
      fAO(cv, hx, hy + 52 * z, 54 * z, 16 * z, 0.20);
      fAO(cv, hx - 120 * z, hy + 124 * z, 46 * z, 32 * z, 0.16);
      fAO(cv, hx + 120 * z, hy + 124 * z, 46 * z, 32 * z, 0.16);
    });
  }
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, hx, hy + 90 * z, 40 * z, 50 * z, "rgba(255,228,200,.28)", 0);
  cv.restore();
  cv.setStrokeColorStr("rgba(160,95,75,.32)");
  cv.setLineWidth(1.8);
  cv.beginPath();
  cv.moveTo(hx - 40 * z, hy + 48 * z);
  cv.quadraticCurveTo(hx - 8 * z, hy + 56 * z, hx, hy + 50 * z);
  cv.stroke();
  cv.beginPath();
  cv.moveTo(hx + 40 * z, hy + 48 * z);
  cv.quadraticCurveTo(hx + 8 * z, hy + 56 * z, hx, hy + 50 * z);
  cv.stroke();

  F64 jig = g.breast.p * 0.7;
  F64 bsz = 0.7 + g.ch.breastSize * 0.55;
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    F64 bx = hx + s * (54 + bsz * 8) * z, by = hy + (92 + jig) * z;
    oralPoseBreast(cv, g, bx, by, 30 * bsz * z, E);
    shade(cv, bx + s * 24 * z, by + 4 * z, 16 * z, 22 * z,
          "rgba(150,80,60,0.20)", s * 0.1);
  }
  shade(cv, hx, hy + (98 + jig) * z, 10 * z, 28 * bsz * z,
        "rgba(160,92,70,0.22)", 0);

  skFillRad(cv, [&](Canvas& c) { c.ellipse(hx, hy + 42 * z, 22 * z, 20 * z, 0, 0, TAU); },
            herT, hx, hy + 30 * z, 2 * z, 30 * z);
  if (D > 0.58) {
    F64 b = (D - 0.58) * 12 + swallow * 5;
    cv.setFillColorStr(herT.b);
    cv.beginPath();
    cv.ellipse(hx, hy + 50 * z, (16 + b * 0.3) * z, (10 + b * 0.5) * z, 0, 0,
               TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,225,200,.14)");
    cv.beginPath();
    cv.ellipse(hx - 4 * z, hy + 46 * z, 5 * z, 3.4 * z, -0.2, 0, TAU);
    cv.fill();
  }

  Tone FT = herT;
  skFillRad(cv, [&](Canvas& c) { c.ellipse(hx, hy, 50 * z, 46 * z, 0, 0, TAU); },
            FT, hx - 14 * z, hy - 16 * z, 4 * z, 64 * z);
  F64 jawDrop = (8 + 10 * D + swallow * 3) * z;
  cv.setFillColorStr(FT.b);
  cv.beginPath();
  cv.moveTo(hx - 28 * z, hy + 16 * z);
  cv.quadraticCurveTo(hx - 22 * z, hy + 28 * z + jawDrop * 0.4, hx,
                      hy + 30 * z + jawDrop * 0.55);
  cv.quadraticCurveTo(hx + 22 * z, hy + 28 * z + jawDrop * 0.4, hx + 28 * z,
                      hy + 16 * z);
  cv.closePath();
  cv.fill();
  skClipIn(cv, [&](Canvas& c) { c.ellipse(hx, hy, 50 * z, 46 * z, 0, 0, TAU); },
           [&]() {
             fAO(cv, hx + 40 * z, hy + 6 * z, 20 * z, 30 * z, 0.14);
             fAO(cv, hx, hy + 36 * z, 32 * z, 14 * z, 0.16);
           });
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, hx, hy - 28 * z, 36 * z, 14 * z, "rgba(150,90,70,.08)", 0);
  shade(cv, hx, hy + 22 * z, 22 * z, 10 * z, "rgba(150,80,60,.14)", 0);
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, hx - 8, hy - 18 * z, 12 * z, 28 * z, "rgba(255,236,214,.38)", 0);
  cv.restore();

  {
    auto rgb = hexToRgb(C.blush);
    for (int si = 0; si < 2; si++) {
      F64 s = si == 0 ? -1 : 1;
      cv.setFillColorStr(FT.s);
      cv.beginPath();
      cv.ellipse(hx + s * 48 * z, hy - 6 * z, 7 * z, 12 * z, s * 0.12, 0, TAU);
      cv.fill();
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", rgb[0], rgb[1],
                    rgb[2], 0.08 + E.blush * 0.18);
      shade(cv, hx + s * 48 * z, hy - 4 * z, 7 * z, 9 * z, buf, 0);
    }
  }
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    auto rgb = hexToRgb(C.blush);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", rgb[0], rgb[1],
                  rgb[2], 0.10 + E.blush * 0.28);
    shade(cv, hx + s * 30 * z, hy + 4 * z, 18 * z, 12 * z, buf, s * 0.14);
  }
  if (suck > 0.18) {
    cv.save();
    cv.setCompStr("multiply");
    for (int si = 0; si < 2; si++) {
      F64 s = si == 0 ? -1 : 1;
      auto rgb = hexToRgb(C.blush);
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(120,55,48,%.17g)", 0.15 + suck * 0.26);
      (void)rgb;
      shade(cv, hx + s * 34 * z, hy + 16 * z, 11 * z, 8 * z, buf, s * 0.16);
    }
    cv.restore();
  }

  bool eyeShut = E.eye < 0.14;
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    F64 ex = hx + s * 22 * z, ey = hy - 16 * z;
    cv.setFillColorStr("rgba(140,80,70,.12)");
    cv.beginPath();
    cv.ellipse(ex, ey + 2 * z, 11 * z, 8 * z, s * 0.06, 0, TAU);
    cv.fill();
    if (eyeShut) {
      cv.setStrokeColorStr("rgba(48,22,28,.96)");
      cv.setLineWidth(2.4 * z);
      cv.beginPath();
      cv.moveTo(ex - 9 * z, ey);
      cv.quadraticCurveTo(ex, ey + 3.5 * z, ex + 9 * z, ey);
      cv.stroke();
    } else {
      cv.setFillColorStr("#faf7f4");
      cv.beginPath();
      cv.ellipse(ex, ey, 9.2 * z, 6.2 * z, s * 0.07, 0, TAU);
      cv.fill();
      cv.setFillColorStr("rgba(210,140,135,.42)");
      cv.beginPath();
      cv.ellipse(ex - s * 7.4 * z, ey + 0.6 * z, 2 * z, 1.6 * z, 0, 0, TAU);
      cv.fill();
      cv.setFillColorStr(C.eye);
      cv.beginPath();
      cv.arc(ex, ey - 1.6 * z, 4.6 * z, 0, TAU);
      cv.fill();
      cv.setStrokeColorStr("rgba(0,0,0,.16)");
      cv.setLineWidth(0.7 * z);
      for (int r = 0; r < 7; r++) {
        F64 a = r / 7.0 * TAU;
        cv.beginPath();
        cv.moveTo(ex, ey - 1.6 * z);
        cv.lineTo(ex + std::cos(a) * 3.4 * z, ey - 1.6 * z + std::sin(a) * 3.4 * z);
        cv.stroke();
      }
      cv.setFillColorStr("#10080c");
      cv.beginPath();
      cv.arc(ex, ey - 1.6 * z, 2.45 * z, 0, TAU);
      cv.fill();
      oralCatch(cv, ex + 1.7 * z, ey - 3.0 * z, 1.55 * z);
      cv.setStrokeColorStr("rgba(46,20,26,.96)");
      cv.setLineWidth(2.4 * z);
      cv.beginPath();
      cv.moveTo(ex - s * 9.5 * z, ey + 1.8 * z);
      cv.quadraticCurveTo(ex, ey - 8.2 * z, ex + s * 9.5 * z, ey);
      cv.stroke();
      cv.setLineWidth(1.15 * z);
      for (int i = 0; i < 7; i++) {
        F64 t = i / 6.0, lxv = ex + s * (-9 + t * 18) * z,
            ly = ey - (6.2 - std::sin(t * 3.141592653589793) * 1.6) * z;
        cv.beginPath();
        cv.moveTo(lxv, ly);
        cv.lineTo(lxv + s * 0.3 * z, ly - 4.2 * z);
        cv.stroke();
      }
    }
    cv.setStrokeColorStr(C.hair);
    cv.setLineWidth(2.0 * z);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(ex - s * 11 * z, ey - 12 * z);
    cv.quadraticCurveTo(ex, ey - 15 * z - E.brow * 6, ex + s * 11 * z,
                        ey - 11 * z - E.brow * 5);
    cv.stroke();
  }

  if (D > 0.60) {
    F64 ta = 0.5 + (D - 0.60) * 1.2;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(220,238,250,%.17g)", ta);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(hx - 24 * z, hy - 12 * z, 2.5 * z, 0, TAU);
    cv.fill();
    cv.beginPath();
    cv.arc(hx + 24 * z, hy - 12 * z, 2.5 * z, 0, TAU);
    cv.fill();
    if (D > 0.80) {
      cv.beginPath();
      cv.ellipse(hx - 25 * z, hy - 4 * z, 1.7 * z, 4.2 * z, 0.16, 0, TAU);
      cv.fill();
      cv.beginPath();
      cv.ellipse(hx + 25 * z, hy - 4 * z, 1.7 * z, 4.2 * z, -0.16, 0, TAU);
      cv.fill();
    }
  }

  cv.setFillColorStr("rgba(180,100,88,.40)");
  cv.beginPath();
  cv.ellipse(hx, hy + 4 * z, 4.4 * z, 3.2 * z, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,230,214,.22)");
  cv.beginPath();
  cv.ellipse(hx - 1, hy + 2.2 * z, 2.2 * z, 1.5 * z, 0, 0, TAU);
  cv.fill();
  cv.setStrokeColorStr("rgba(150,90,80,.28)");
  cv.setLineWidth(1.2 * z);
  cv.beginPath();
  cv.moveTo(hx - 3.5 * z, hy + 6 * z);
  cv.quadraticCurveTo(hx, hy + 8.5 * z, hx + 3.5 * z, hy + 6 * z);
  cv.stroke();

  F64 lipStretch = (15 + 8 * D) * z;
  F64 lipOpen = (11 + 10 * D + swallow * 2) * z;
  cv.setFillGrad(gRadial(cv, hx, mouthY + 4, 2, hx, mouthY, lipStretch * 0.78,
                         {{0, "#2a0810"}, {0.55, "#541420"}, {1, "#7a2834"}}));
  cv.beginPath();
  cv.ellipse(hx, mouthY, lipStretch * 0.78, lipOpen * 0.70, 0, 0, TAU);
  cv.fill();

  if (D < 0.62) {
    F64 tVis = 1 - D / 0.62;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(196,96,110,%.17g)", 0.55 + 0.4 * tVis);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(hx, mouthY + lipOpen * 0.28, 11 * z * tVis + 6 * z,
               (4 + 6 * tVis) * z, 0, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,190,200,.22)");
    cv.beginPath();
    cv.ellipse(hx, mouthY + lipOpen * 0.18, 3.5 * z, 1.6 * z, 0, 0, TAU);
    cv.fill();
  }

  cv.restore();

  Tone AT = oralHimT(g);
  skFillRad(cv, [&](Canvas& c) { c.ellipse(640, 828, 242, 112, 0, 0, TAU); },
            AT, 640, 786, 20, 290);
  skClipIn(cv, [&](Canvas& c) { c.ellipse(640, 828, 242, 112, 0, 0, TAU); },
           [&]() {
             fAO(cv, 640, 730, 180, 30, 0.22);
             fHi(cv, 556, 800, 120, 34, "rgba(255,238,220,0.14)");
           });
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    capsuleTone(cv, {640 + s * 168, 826}, {640 + s * 98, 642}, 58, 44);
  }
  cv.setFillColorStr("rgba(55,32,26,.38)");
  for (int i = 0; i < 10; i++) {
    cv.beginPath();
    cv.ellipse(640 + (i - 4.5) * 7, 806 + ((i * 3) % 7) - 4, 2.6, 7.5,
               (i % 5 - 2) * 0.12, 0, TAU);
    cv.fill();
  }

  V2 shaftBase{640, 808};
  F64 lipsY = mouthY;
  F64 visibleTip = lipsY + 4;
  F64 wb = 26 * pu, wt = 17 * pu;
  cv.setFillGrad(gLinear(cv, 640 - wb, 0, 640 + wb, 0,
                         {{0, "#a96c44"},
                          {0.28, "#d99a6d"},
                          {0.48, "#f0c096"},
                          {0.70, "#dd9c6f"},
                          {1, "#96613e"}}));
  cv.beginPath();
  cv.moveTo(640 - wb, shaftBase.y);
  cv.quadraticCurveTo(640 - wb + 4, (shaftBase.y + visibleTip) / 2, 640 - wt,
                      visibleTip);
  cv.quadraticCurveTo(640, visibleTip - 6, 640 + wt, visibleTip);
  cv.quadraticCurveTo(640 + wb - 4, (shaftBase.y + visibleTip) / 2, 640 + wb,
                      shaftBase.y);
  cv.closePath();
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 624, (shaftBase.y + visibleTip) / 2, 12,
        std::fabs(shaftBase.y - visibleTip) / 2, "rgba(90,50,28,.22)", 0);
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 636, (shaftBase.y + visibleTip) / 2, 14,
        std::fabs(shaftBase.y - visibleTip) / 2, "rgba(255,225,195,.36)", 0);
  cv.restore();
  cv.setStrokeColorStr("rgba(140,80,60,.36)");
  cv.setLineWidth(2.4);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(628, 790);
  cv.quadraticCurveTo(622, 720, 632, visibleTip + 16);
  cv.stroke();
  cv.setLineWidth(1.5);
  cv.beginPath();
  cv.moveTo(654, 780);
  cv.quadraticCurveTo(658, 710, 650, visibleTip + 20);
  cv.stroke();

  if (D < 0.38) {
    F64 gy = visibleTip - 2;
    cv.setFillGrad(gRadial(cv, 634, gy - 5, 2, 640, gy, 20 * pu,
                           {{0, "#e9aa8f"}, {0.6, "#c88870"}, {1, "#a5614e"}}));
    cv.beginPath();
    cv.ellipse(640, gy, 17.5 * pu, 13 * pu, 0, 0, TAU);
    cv.fill();
    cv.save();
    cv.setCompStr("multiply");
    cv.setFillColorStr("rgba(150,60,50,.22)");
    cv.beginPath();
    cv.ellipse(640, gy + 3, 15 * pu, 5, 0, 0, TAU);
    cv.fill();
    cv.restore();
    cv.setFillColorStr("rgba(255,252,248,.38)");
    cv.beginPath();
    cv.ellipse(634, gy - 4, 4.6, 6.6, -0.18, 0, TAU);
    cv.fill();
    cv.setStrokeColorStr("rgba(120,60,55,.55)");
    cv.setLineWidth(1.8);
    cv.beginPath();
    cv.moveTo(640, gy - 12);
    cv.lineTo(640, gy - 5);
    cv.stroke();
  }

  if (wet > 0.15) {
    cv.save();
    cv.setCompStr("screen");
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,245,238,%.17g)", 0.16 + wet * 0.28);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(3.2);
    cv.beginPath();
    cv.moveTo(626, 760);
    cv.quadraticCurveTo(628, (760 + visibleTip) / 2, 630, visibleTip + 8);
    cv.stroke();
    cv.restore();
  }

  F64 inLen = lerp(8, 22, D);
  cv.setFillColorStr(D < 0.38 ? "#c88870" : "#d4a078");
  cv.beginPath();
  cv.ellipse(640, lipsY - inLen * 0.35, wt * 0.92, inLen * 0.45, 0, 0, TAU);
  cv.fill();

  cv.save();
  cv.translate(swayX, swayY + bob * 0.15);
  lipRing(cv, hx, mouthY, lipStretch, lipOpen, wt * 0.95, lipOpen * 0.42, 0,
          C.lip);
  cv.setStrokeColorStr("rgba(130,48,58,.48)");
  cv.setLineWidth(1.5 * z);
  cv.beginPath();
  cv.moveTo(hx - lipStretch * 0.72, mouthY - lipOpen * 0.35);
  cv.quadraticCurveTo(hx - 6 * z, mouthY - lipOpen * 0.62, hx,
                      mouthY - lipOpen * 0.38);
  cv.quadraticCurveTo(hx + 6 * z, mouthY - lipOpen * 0.62,
                      hx + lipStretch * 0.72, mouthY - lipOpen * 0.35);
  cv.stroke();
  cv.setFillColorStr("rgba(255,248,244,.62)");
  cv.beginPath();
  cv.ellipse(hx - 6 * z, mouthY - lipOpen * 0.40, 5 * z, 2.3 * z, 0, 0, TAU);
  cv.fill();
  cv.beginPath();
  cv.ellipse(hx, mouthY + lipOpen * 0.46, 6.2 * z, 2.6 * z, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,255,255,.18)");
  cv.beginPath();
  cv.ellipse(hx + 8 * z, mouthY - lipOpen * 0.12, 2.8 * z, 1.5 * z, 0, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  cv.setStrokeColorStr("rgba(70,24,24,.30)");
  cv.setLineWidth(2);
  cv.beginPath();
  cv.ellipse(hx, mouthY, wt * 1.05, lipOpen * 0.44, 0, 0, TAU);
  cv.stroke();
  cv.restore();

  if (D > 0.08) {
    oralSaliva(cv, hx - lipStretch * 0.7, mouthY + 4, 632, mouthY + 28 + D * 10,
               628, mouthY + 16, 0.48 + wet * 0.28);
    oralSaliva(cv, hx + lipStretch * 0.7, mouthY + 4, 648, mouthY + 28 + D * 10,
               652, mouthY + 16, 0.48 + wet * 0.28);
    if (wet > 0.42) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "rgba(255,250,244,%.17g)",
                    0.32 + wet * 0.28);
      cv.setFillColorStr(buf);
      cv.beginPath();
      cv.ellipse(636, mouthY + 34 + D * 8, 2.3, 3.4, 0, 0, TAU);
      cv.fill();
      cv.beginPath();
      cv.ellipse(645, mouthY + 30 + D * 8, 1.7, 2.6, 0, 0, TAU);
      cv.fill();
    }
  }
  cv.restore();

  F64 stroke = std::sin(g.t * 8.5) * std::min(std::fabs(vel) * 14, 12.0);
  for (int si = 0; si < 2; si++) {
    F64 s = si == 0 ? -1 : 1;
    F64 hY = lerp(lipsY + 86, lipsY + 42, D * 0.55) + stroke * s * 0.15;
    capsuleTone(cv, {640 + s * 186, 770}, {640 + s * 42, hY}, 26, 19);
    cv.setFillColorStr(sk.her);
    cv.beginPath();
    cv.ellipse(640 + s * 40, hY, 16, 12, s * 0.4, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,216,196,.22)");
    for (int k = 0; k < 3; k++) {
      cv.beginPath();
      cv.ellipse(640 + s * (32 + k * 5), hY - 6 + k * 2.2, 2.5, 1.8, 0, 0, TAU);
      cv.fill();
    }
    cv.setStrokeColorStr(sk.herSh);
    cv.setLineWidth(2.4);
    cv.setLineCapStr("round");
    for (int f = 0; f < 4; f++) {
      cv.beginPath();
      cv.arc(640 + s * (28 + f * 3.4), hY - 2 + f * 2.6, 4.6,
             s > 0 ? 0 : 3.141592653589793,
             s > 0 ? 3.141592653589793 : TAU);
      cv.stroke();
    }
    cv.setFillColorStr("rgba(255,228,218,.62)");
    cv.beginPath();
    cv.ellipse(640 + s * 52, hY + 2, 3.6, 2.7, s * 0.3, 0, TAU);
    cv.fill();
  }

  oralFPVFluids(cv, g);
  {
    F64 om = g.oral != 0 ? g.oral : 1;
    if (oral_rng.next01() < 0.034 * om)
      g.hearts.push_back({640 + oral_rng.range(-26, 26),
                          hy - 36 + oral_rng.range(-10, 10), 1.25,
                          oral_rng.next01() * TAU, oral_rng.range(0.38, 0.74)});
  }

  cv.setFillColorStr("rgba(150,90,60,.18)");
  cv.beginPath();
  cv.ellipse(50, 770, 230, 130, 0.3, 0, TAU);
  cv.fill();
  cv.beginPath();
  cv.ellipse(1230, 770, 230, 130, -0.3, 0, TAU);
  cv.fill();
}

} // namespace — file-local helpers end; toggle/draw need external linkage

const char* pickLine(std::initializer_list<const char*> xs) {
  size_t n = 0;
  for (auto x : xs) (void)x, n++;
  size_t i = (size_t)(oral_rng.next01() * (F64)n) % n;
  size_t k = 0;
  for (auto x : xs) {
    if (k == i) return x;
    k++;
  }
  return "";
}

void toggle(Game& g) {
  if (g.state != "play" || g.tired) return;
  g.nod = 1;
  if (!g.oralT || g.oralT == 0) {
    g.oralT = 1;
    s_oralMode = "lick";
    g.speech = pickLine({"taste me... \xe2\x99\xa5",
                         "eat me... I need you down there... \xe2\x99\xa5",
                         "please, lick me... \xe2\x99\xa5", "right there... \xe2\x99\xa5"});
    audio::playOnce("moan02", 0.42 * 0.90);
  } else if (g.oralT == 1) {
    g.oralT = 2;
    s_oralMode = "blow";
    g.speech = pickLine({"let me taste you... \xe2\x99\xa5",
                         "mmh... I want you in my mouth... \xe2\x99\xa5",
                         "relax... let me use my lips... \xe2\x99\xa5",
                         "I need this..."});
    audio::playOnce("moan02", 0.38 * 0.85);
  } else {
    g.oralT = 0;
    s_oralMode = "";
    g.speech = "mmh... want me to ride you instead...? \xe2\x99\xa5";
    // playLipPop: no bank stem — silent until synth lands (TODO audio).
  }
  g.oral = g.oralT > 0 ? 1 : 0; // immediate routing; mech keeps lerping
}

void draw(Canvas& cv, Game& g) {
  drawRoom(cv, g);
  drawBed(cv, g);
  if (g.view == "fpv") drawOralFPV(cv, g);
  else drawOralSide(cv, g);
}

} // namespace oral
} // namespace ag
