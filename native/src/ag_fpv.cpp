// Afterglow native — ag_fpv.cpp
// 1:1 port of js/fpv.js (actual 1120 lines; task brief said 988).
// Entry: ag::fpv::draw(Canvas&, Game&) — complete FPV pass: FPV room variant,
// body, head/face (eyes/brows/nose/mouth per openness/mood), breasts, torso,
// limbs, hands, shaft, sheets, fluids, light overlays via room pass,
// focus/zoom (fpvFocus/fpvZoom/fpvPanX/fpvPanY).
// Same Canvas API + conventions as ag_gfx.cpp: setFillColorStr,
// gLinear/gRadial with vector<StopStr>, setCompStr multiply/screen/soft-light,
// save/restore, transforms, clip, TAU, snprintf %.17g, <cmath>, file-local
// static Rng for visual-only randomness (R()/rr()).
#include "ag_2dmods.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ag {
namespace fpv {
namespace {

// ---- file-local visual RNG (JS Math.random for hearts scatter) ----
static Rng fpv_rng(0x46505631ull);
static F64 fpv_R() { return fpv_rng.next01(); }
static F64 fpv_rr(F64 a, F64 b) { return fpv_rng.range(a, b); }

// ---- fpv_herExpression: 1:1 of side.js:7-36 herExpression() ----
// (side.js not yet ported; local copy — dedupe when side lands.)
// TODO: Game::MoanMouth is {x,y,t} but JS mouths are {t0,dur,i}; below
// approximates t0=m.t, dur=1.1, i=0.5. Fix when core mouths match JS.
struct FpvExpr {
  F64 eye = 0.7, rolled = 0, mouth = 0, blush = 0, brow = 0, tilt = 0;
};
static FpvExpr fpv_herExpression(const Game& g) {
  F64 p = g.pleasure / 100.0;
  F64 ar = g.ar / 100.0;
  F64 t = g.t;
  F64 eye = lerp(0.72, 0.12, sm(0.08, 0.92, p));
  F64 rolled = 0;
  F64 mouth = p * 0.36;
  F64 blush = 0.12 + p * 0.44 + ar * 0.22;
  F64 brow = lerp(-0.06, 0.46, sm(0.20, 0.90, p));
  F64 tilt = 0.12 + p * 0.0035;
  for (const auto& m : g.mouths) {
    F64 u = (t - m.t) / 1.1;
    if (u > 0 && u < 1) mouth += std::pow(std::sin(3.141592653589793 * u), 0.75) * 0.5 * 0.65;
  }
  if (!g.speech.empty()) mouth += 0.28;
  if (g.blinkPh > 0) eye *= (1.0 - g.blinkPh);
  if (g.state == "orgasm") {
    F64 e = std::sin(3.141592653589793 * clamp(g.orgT / 5.2, 0.0, 1.0));
    rolled = 0.70 + e * 0.30;
    eye = lerp(eye, 0.06, e);
    mouth = mouth > 0.88 * e ? mouth : 0.88 * e;
    tilt = 0.50;
    blush = 1.0;
    brow = 0.65;
  }
  if (g.after > 0) {
    eye = eye < 0.16 ? eye : 0.16;
    mouth = mouth > 0.18 ? mouth : 0.18;
    blush = blush > 0.45 ? blush : 0.45;
  }
  if (g.state == "finish" && g.finishT > 1) {
    eye = 0.06;
    mouth = 0.22;
  }
  if (g.kiss > 0.6) mouth = mouth < 0.45 ? mouth : 0.45;
  return {clamp(eye, 0.0, 1.0), clamp(rolled, 0.0, 1.0),
          clamp(mouth, 0.0, 1.0), clamp(blush, 0.0, 1.0), brow, tilt};
}

// ---- missing skin tone helpers (js/skin.js:9-11 skMix/skLight/skDark) ----
static std::string fpv_skMix(std::string_view a, std::string_view b, F64 t) {
  return lerpHex(a, b, clamp(t, 0.0, 1.0));
}
static std::string fpv_skLight(std::string_view h, F64 t) {
  return fpv_skMix(h, "#fff4ea", t);
}
static std::string fpv_skDark(std::string_view h, F64 t) {
  return fpv_skMix(h, "#3a1410", t);
}

// ---- missing hair helpers (js/skin.js:308-340 tressS/hairMassS) ----
static void fpv_tress(Canvas& cv, F64 x0, F64 y0, F64 c1x, F64 c1y, F64 c2x,
                      F64 c2y, F64 x1, F64 y1, F64 w0, F64 w1,
                      const std::string& col, const std::string& sheen) {
  F64 px[4] = {x0, c1x, c2x, x1}, py[4] = {y0, c1y, c2y, y1};
  auto off = [&](F64 t, F64 w, F64& ox, F64& oy) {
    F64 mt = 1 - t;
    F64 bx = mt * mt * mt * px[0] + 3 * mt * mt * t * px[1] +
             3 * mt * t * t * px[2] + t * t * t * px[3];
    F64 by = mt * mt * mt * py[0] + 3 * mt * mt * t * py[1] +
             3 * mt * t * t * py[2] + t * t * t * py[3];
    F64 dx = 3 * mt * mt * (px[1] - px[0]) + 6 * mt * t * (px[2] - px[1]) +
             3 * t * t * (px[3] - px[2]);
    F64 dy = 3 * mt * mt * (py[1] - py[0]) + 6 * mt * t * (py[2] - py[1]) +
             3 * t * t * (py[3] - py[2]);
    F64 L = std::hypot(dx, dy);
    if (L < 1e-9) L = 1;
    ox = bx - dy / L * w;
    oy = by + dx / L * w;
  };
  auto wAt = [&](F64 t) { return lerp(w0, w1, t); };
  cv.beginPath();
  F64 ox = 0, oy = 0;
  off(0, wAt(0), ox, oy);
  cv.moveTo(ox, oy);
  for (F64 t = 0.2; t <= 1.001; t += 0.2) {
    off(t, wAt(t), ox, oy);
    cv.lineTo(ox, oy);
  }
  for (F64 t = 1.0; t >= -0.001; t -= 0.2) {
    off(t, -wAt(t), ox, oy);
    cv.lineTo(ox, oy);
  }
  cv.closePath();
  cv.setFillColorStr(col);
  cv.fill();
  if (!sheen.empty()) {
    cv.save();
    cv.setCompStr("soft-light");
    cv.setStrokeColorStr(sheen);
    cv.setLineWidth(w0 * 0.3 > 1 ? w0 * 0.3 : 1);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(x0, y0);
    cv.bezierCurveTo(c1x, c1y, c2x, c2y, x1, y1);
    cv.stroke();
    cv.restore();
  }
}
static void fpv_hairMass(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 rot,
                         const std::string& col) {
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, rot, 0, TAU);
  cv.setFillGrad(gRadial(cv, cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy,
                         rx * 1.2,
                         {{0, fpv_skLight(col, 0.22)},
                          {0.55, col},
                          {1, fpv_skDark(col, 0.4)}}));
  cv.fill();
}

// ---- handS (js/skin.js:101-138). Global ag::handS is still a no-op stub, so
// a file-local 1:1 copy keeps FPV hands visible. Remove when skin.js:101 lands.
static void fpv_handS(Canvas& cv, F64 x, F64 y, F64 ang, F64 s, const Tone& T,
                      const HandOpt& opt) {
  F64 curl = opt.curl, spread = opt.spread;
  cv.save();
  cv.translate(x, y);
  cv.rotate(ang);
  cv.scale(s, s);
  PathFn palm = [](Canvas& c) {
    c.moveTo(-9, -8);
    c.quadraticCurveTo(0, -11, 9, -8);
    c.quadraticCurveTo(12, 0, 9, 8);
    c.quadraticCurveTo(0, 12, -9, 8);
    c.quadraticCurveTo(-12, 0, -9, -8);
    c.closePath();
  };
  F64 lg[4] = {-10, -10, 10, 10};
  skFillShape(cv, palm, T, lg);
  skClipIn(cv, palm, [&]() {
    fHi(cv, -2, -3, 7, 5, "rgba(255,240,225,0.30)");
    fAO(cv, 0, 8, 9, 4, 0.3);
  });
  for (int i = 0; i < 4; i++) {
    F64 kx = -7 + i * 4.6, ky = -8;
    F64 fa = -3.141592653589793 / 2 + (i - 1.5) * spread * 0.5;
    F64 L = (i == 0 || i == 3) ? 13 : 16;
    F64 bend = curl * 1.1;
    V2 j1{kx + std::cos(fa) * L * 0.5, ky + std::sin(fa) * L * 0.5};
    V2 j2{kx + std::cos(fa + bend * 0.5) * L,
          ky + std::sin(fa + bend * 0.5) * L + curl * 5};
    LimbOpt lo;
    lo.line = false;
    lo.belly = 1.0;
    limbS(cv, {kx, ky}, j1, 2.6, 2.2, T, lo);
    limbS(cv, j1, j2, 2.2, 1.6, T, lo);
    cv.setFillColorStr("rgba(255,235,225,0.5)");
    cv.beginPath();
    cv.ellipse(j2.x, j2.y + 0.4, 1.3, 1.0, fa, 0, TAU);
    cv.fill();
  }
  F64 ta = 3.141592653589793 * 0.78;
  V2 t1{-8 + std::cos(ta) * 6, 2 + std::sin(ta) * 6};
  V2 t2{-8 + std::cos(ta - curl) * 12, 2 + std::sin(ta - curl) * 12};
  LimbOpt lo;
  lo.line = false;
  limbS(cv, {-8, 2}, t1, 3.2, 2.6, T, lo);
  limbS(cv, t1, t2, 2.6, 2.0, T, lo);
  cv.setFillColorStr("rgba(120,60,50,0.20)");
  for (int i = 0; i < 4; i++) {
    cv.beginPath();
    cv.arc(-7 + i * 4.6, -7.4, 0.9, 0, TAU);
    cv.fill();
  }
  skLine(cv, palm, T, 1.1, 0.3);
  cv.restore();
}

// ---- vulvaS (js/skin.js:206-274). Global ag::vulvaS is still a no-op stub,
// so a file-local 1:1 copy keeps the spot visible. Remove when skin.js:206 lands.
static void fpv_vulvaS(Canvas& cv, const Game& g, F64 cx, F64 cy, F64 open,
                       F64 eng, const Tone& T, const std::string& view) {
  cv.save();
  cv.beginPath();
  cv.ellipse(cx, cy - 4, 15, 13, 0, 0, TAU);
  cv.setFillGrad(gRadial(cv, cx - 4, cy - 9, 2, cx, cy - 4, 18,
                         {{0, T.hi}, {0.55, T.b}, {1, T.s}}));
  cv.fill();
  cv.restore();

  if (view == "front") {
    for (int si : {-1, 1}) {
      F64 s = (F64)si;
      cv.beginPath();
      cv.moveTo(cx + s * 2, cy - 13);
      cv.bezierCurveTo(cx + s * 9, cy - 9, cx + s * 9.5, cy + 4, cx + s * 3,
                       cy + 12);
      cv.bezierCurveTo(cx + s * 1.5, cy + 6, cx + s * 1.5, cy - 6, cx + s * 2,
                       cy - 13);
      cv.closePath();
      cv.setFillColorStr(T.s);
      cv.fill();
      fSh(cv, cx + s * 5, cy, 3.4, 10, "rgba(120,45,50,0.45)", 0);
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,0.9)",
                  (int)(205 + 30 * eng), (int)(95 + 20 * eng),
                  (int)(110 + 20 * eng));
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(cx, cy + 1, 2.6 + open * 0.25, 8 + open * 0.5, 0, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(70,20,30,0.9)");
    cv.beginPath();
    cv.ellipse(cx, cy + 3, 2.2, open * 0.6, 0, 0, TAU);
    cv.fill();
    cv.setFillColorStr(T.b);
    cv.beginPath();
    cv.ellipse(cx, cy - 10, 4, 3.2, 0, 3.141592653589793, 0);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(%d,105,118,0.95)",
                  (int)(215 + 25 * eng));
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(cx, cy - 9, 1.4 + 1.4 * eng, 0, TAU);
    cv.fill();
  } else {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%d,105,100,0.8)",
                  (int)(185 + 25 * eng));
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(3.4);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(cx + 1, cy - 12);
    cv.quadraticCurveTo(cx + 5, cy, cx + 1, cy + 11);
    cv.stroke();
    std::snprintf(buf, sizeof(buf), "rgba(%d,100,112,0.85)",
                  (int)(205 + 30 * eng));
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(cx + 2, cy, 2.4, 8 + open * 0.4, 0.1, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(70,20,30,0.85)");
    cv.beginPath();
    cv.ellipse(cx + 2.5, cy + 2, 2.0, open * 0.55, 0.1, 0, TAU);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(%d,105,118,0.95)",
                  (int)(215 + 25 * eng));
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(cx + 1, cy - 10, 1.4 + 1.4 * eng, 0, TAU);
    cv.fill();
  }
  if (g.ar > 22) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,255,255,%.17g)",
                  0.25 + 0.3 * eng);
    cv.save();
    cv.setCompStr("screen");
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(1.8);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(cx + (view == "front" ? 3 : 4), cy - 8);
    cv.quadraticCurveTo(cx + (view == "front" ? 5 : 6), cy,
                        cx + (view == "front" ? 3 : 4), cy + 8);
    cv.stroke();
    cv.restore();
  }
  std::string ph = g.ch.pubicHair;
  if (ph != "bare") {
    int n = (ph == "full") ? 9 : 5;
    std::string hc = fpv_skDark(g.ch.hairColor, 0.1);
    cv.save();
    cv.setLineCapStr("round");
    for (int i = 0; i < n; i++) {
      F64 a0 = 3.141592653589793 * (0.2 + ((F64)i / (n - 1)) * 0.6);
      F64 r0 = 7, r1 = (ph == "full") ? 16 : 12;
      cv.setStrokeColorStr(hc);
      cv.setGlobalAlpha(0.7);
      cv.setLineWidth(1.9 - (i % 3) * 0.4);
      cv.beginPath();
      cv.moveTo(cx + std::cos(a0) * r0 * 0.5,
                cy - 14 + std::sin(a0) * r0 * 0.3);
      cv.quadraticCurveTo(cx + std::cos(a0) * r1 * 0.8,
                          cy - 14 + std::sin(a0) * r1 * 0.6,
                          cx + std::cos(a0) * r1, cy - 14 + std::sin(a0) * r1);
      cv.stroke();
    }
    cv.restore();
  }
}

// ---- fpvTremor (fpv.js:8-14) ----
static F64 fpv_tremor(const Game& g) {
  F64 org = (g.state == "orgasm")
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

// ---- drawFPVRoom (fpv.js:19-145) ----
static void fpv_room(Canvas& cv, Game& g) {
  cv.setFillColorStr("#0f090d");
  cv.fillRect(0, 0, VW, VH);

  cv.setFillGrad(gLinear(cv, 0, 0, 0, 300,
                         {{0, "#1a1016"}, {0.7, "#130c11"}, {1, "#0c070a"}}));
  cv.fillRect(0, 0, VW, 280);

  const F64 wx = 960, wy = 24, ww = 180, wh = 140;
  cv.setFillColorStr("rgba(120,145,170,0.06)");
  cv.fillRect(wx, wy, ww, wh);
  GradPtr moonG =
      gRadial(cv, wx + 130, wy + 35, 6, wx + 130, wy + 35, 65,
              {{0, "rgba(230,242,255,0.22)"},
               {0.4, "rgba(180,210,240,0.06)"},
               {1, "rgba(0,0,0,0)"}});
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(moonG);
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

// ---- drawFPVHead (fpv.js:150-486) ----
static void fpv_head(Canvas& cv, Game& g, const FpvExpr& E) {
  F64 breathe = std::sin(g.t * TAU * 0.33) * 1.8;
  F64 ks = 1 + g.kiss * 0.12;
  F64 hx = 640 + std::sin(g.t * 0.5) * 2.2;
  F64 hy = 148 + breathe * 0.45 + g.pleasure * 0.025;
  std::string hairCol = g.ch.hairColor;

  fpv_hairMass(cv, hx, hy + 16, 80, 50, 0, hairCol);
  cv.save();
  cv.beginPath();
  cv.ellipse(hx, hy + 16, 80, 50, 0, 0, TAU);
  cv.clip();
  cv.setStrokeColorStr(fpv_skDark(hairCol, 0.45));
  cv.setLineWidth(2.4);
  cv.setLineCapStr("round");
  for (int i = 0; i < 5; i++) {
    cv.beginPath();
    cv.moveTo(hx - 32 + i * 16, hy - 8);
    cv.quadraticCurveTo(hx - 52 + i * 26, hy + 24, hx - 70 + i * 35, hy + 58);
    cv.stroke();
  }
  cv.setCompStr("soft-light");
  shade(cv, hx, hy + 2, 62, 20, "rgba(255,235,220,0.20)", 0);
  cv.restore();

  for (int i = 0; i < 6; i++) {
    int s = i < 3 ? -1 : 1, k = i % 3;
    F64 bx = hx + s * (30 + k * 10), by = hy + 26 + k * 6;
    F64 c1x = hx + s * (48 + k * 12), c1y = by + 26;
    F64 c2x = hx + s * (56 + k * 14), c2y = by + 54;
    F64 tx = hx + s * (50 + k * 14), ty = hy + 96 + k * 12;
    fpv_tress(cv, bx, by, c1x, c1y, c2x, c2y, tx, ty, 13 - k * 2.2, 3, hairCol,
              "rgba(255,200,210,0.14)");
  }

  cv.save();
  cv.translate(hx, hy);
  cv.scale(ks, ks);
  cv.rotate(E.tilt * 0.35 + g.nod * 0.1 + std::sin(g.t * TAU * 0.33) * 0.015);

  SkinPair fpvSk = chars::getSkin(g);

  const F64 nkTop = 30;
  PathFn neckPath = [=](Canvas& c) {
    c.moveTo(-11, nkTop);
    c.bezierCurveTo(-12.5, 44, -13, 56, -16, 68);
    c.bezierCurveTo(-19, 78, -26, 84, -34, 88);
    c.bezierCurveTo(-20, 99, 20, 99, 34, 88);
    c.bezierCurveTo(26, 84, 19, 78, 16, 68);
    c.bezierCurveTo(13, 56, 12.5, 44, 11, nkTop);
    c.closePath();
  };
  cv.beginPath();
  neckPath(cv);
  cv.setFillGrad(gLinear(cv, 0, nkTop, 0, 92,
                         {{0, fpvSk.herSh},
                          {0.45, fpvSk.her},
                          {1, fpvSk.herSh}}));
  cv.fill();
  skClipIn(cv, neckPath, [&]() {
    fAO(cv, 0, nkTop + 3, 13, 7, 0.42);
    fSh(cv, -8, 52, 3.2, 16, "rgba(175,100,80,0.22)", 0.10);
    fSh(cv, 8, 52, 3.2, 16, "rgba(175,100,80,0.22)", -0.10);
    fHi(cv, 0, 58, 4.5, 15, "rgba(255,240,225,0.22)");
    fAO(cv, 0, 90, 6, 4, 0.35);
    fSh(cv, -27, 86, 12, 7, "rgba(175,100,80,0.16)", 0.5);
    fSh(cv, 27, 86, 12, 7, "rgba(175,100,80,0.16)", -0.5);
  });

  cv.setFillGrad(sg(cv, -52, 14, fpvSk.her, fpvSk.herSh));
  cv.beginPath();
  cv.ellipse(0, -2, 44, 49, 0, 0, TAU);
  cv.fill();
  cv.beginPath();
  cv.moveTo(-40, -8);
  cv.bezierCurveTo(-36, 35, -12, 46, 0, 47);
  cv.bezierCurveTo(12, 46, 36, 35, 40, -8);
  cv.closePath();
  cv.fill();

  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 26, 12, 22, 14, "rgba(170,95,72,0.17)", 0.38);
  shade(cv, -26, 12, 22, 14, "rgba(170,95,72,0.17)", -0.38);
  shade(cv, 0, 42, 26, 11, "rgba(170,95,72,0.16)", 0);
  cv.restore();

  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 0, -28, 18, 28, "rgba(255,240,225,0.42)", 0);
  shade(cv, 0, 40, 10, 6, "rgba(255,240,225,0.35)", 0);
  cv.restore();

  const F64 Eo = E.eye, roll = E.rolled;
  std::string eyeCol = g.ch.eyeColor;
  for (int si : {-1, 1}) {
    F64 s = (F64)si;
    F64 ex = s * 17.5, ey = -9;
    F64 openness = Eo * (1 - roll * 0.5);

    cv.save();
    cv.setCompStr("multiply");
    shade(cv, ex, ey + 1, 12, 8, "rgba(145,85,75,0.18)", s * 0.08);
    cv.restore();

    if (openness > 0.12) {
      cv.beginPath();
      cv.moveTo(ex - 10.5, ey + 1);
      cv.quadraticCurveTo(ex, ey - 7.5 * openness - 2, ex + 10.5, ey + 1);
      cv.quadraticCurveTo(ex, ey + 6.5 * openness, ex - 10.5, ey + 1);
      cv.closePath();
      cv.setFillColorStr("#faf6f3");
      cv.fill();

      cv.setFillColorStr("rgba(215,145,140,0.45)");
      cv.beginPath();
      cv.ellipse(ex - s * 9, ey + 1, 2.2, 1.8, 0, 0, TAU);
      cv.fill();

      F64 irisY = ey - 2.2 * openness - (roll * 3.5);
      F64 irisX = ex + roll * 3.8;
      F64 irisR = 5.2 * (openness > 0.28 ? openness : 0.28);
      cv.setFillColorStr(eyeCol);
      cv.beginPath();
      cv.arc(irisX, irisY, irisR, 0, TAU);
      cv.fill();

      cv.setFillColorStr("#110609");
      cv.beginPath();
      cv.arc(irisX, irisY, irisR * 0.52, 0, TAU);
      cv.fill();

      cv.setFillColorStr("rgba(255,255,255,0.92)");
      cv.beginPath();
      cv.arc(irisX + 1.5, irisY - 1.8, 1.5, 0, TAU);
      cv.fill();
      cv.setFillColorStr("rgba(255,255,255,0.45)");
      cv.beginPath();
      cv.arc(irisX - 1.6, irisY + 1.2, 0.8, 0, TAU);
      cv.fill();

      cv.setStrokeColorStr("rgba(46,20,26,0.96)");
      cv.setLineWidth(2.4);
      cv.beginPath();
      cv.moveTo(ex - 11, ey + 1);
      cv.quadraticCurveTo(ex, ey - 8 * openness - 2, ex + 11, ey + 1);
      cv.stroke();

      cv.setLineWidth(1.1);
      for (int l = 0; l < 5; l++) {
        F64 t = (F64)l / 4;
        F64 lxx = ex + (-9 + t * 18);
        F64 ly = ey - 6 * openness - std::sin(t * 3.141592653589793) * 2;
        cv.beginPath();
        cv.moveTo(lxx, ly);
        cv.lineTo(lxx + s * 1.5, ly - 3.8);
        cv.stroke();
      }
    } else {
      cv.setStrokeColorStr("rgba(46,20,26,0.96)");
      cv.setLineWidth(2.4);
      cv.beginPath();
      cv.moveTo(ex - 10.5, ey + 1);
      cv.quadraticCurveTo(ex, ey + 4.2, ex + 10.5, ey + 1);
      cv.stroke();
    }

    cv.setStrokeColorStr("rgba(46,20,26,0.85)");
    cv.setLineWidth(1.4);
    cv.beginPath();
    cv.moveTo(ex + (s > 0 ? 10.5 : -10.5), ey + 1);
    cv.lineTo(ex + (s > 0 ? 14 : -14), ey - 2.5);
    cv.stroke();

    cv.setStrokeColorStr(hairCol);
    cv.setLineWidth(2.1);
    cv.beginPath();
    cv.moveTo(ex - 9, ey - 14);
    cv.quadraticCurveTo(ex + 1, ey - 18 - E.brow * 6, ex + 10,
                        ey - 13 - E.brow * 8);
    cv.stroke();
  }

  if (g.pleasure > 70 || g.state == "orgasm") {
    F64 tearA = clamp((g.pleasure - 70) / 30, 0.0, 1.0) * 0.75;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(235,248,255,%.17g)", tearA);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(-22, -6, 2.2, 0, TAU);
    cv.fill();
    cv.beginPath();
    cv.arc(22, -6, 2.2, 0, TAU);
    cv.fill();
  }

  {
    auto brgb = hexToRgb(g.ch.blushColor);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", brgb[0], brgb[1],
                  brgb[2], 0.10 + E.blush * 0.26);
    std::string bc = buf;
    shade(cv, -27, 9, 17, 11, bc, 0.28);
    shade(cv, 27, 9, 17, 11, bc, -0.28);
  }

  cv.setStrokeColorStr("rgba(165,95,82,0.48)");
  cv.setLineWidth(1.8);
  cv.beginPath();
  cv.moveTo(-3, 0);
  cv.quadraticCurveTo(-6, 6, -3.5, 9);
  cv.quadraticCurveTo(0, 11, 3.5, 9);
  cv.quadraticCurveTo(6, 6, 3, 0);
  cv.stroke();
  cv.setFillColorStr("rgba(255,240,230,0.35)");
  cv.beginPath();
  cv.ellipse(0, 3, 2.4, 2, 0, 0, TAU);
  cv.fill();

  const F64 mo = E.mouth;
  std::string lipCol = g.ch.lipColor;
  if (mo > 0.05) {
    cv.setFillColorStr("#420f18");
    cv.beginPath();
    cv.ellipse(0, 29, 9 + mo * 4, 3 + mo * 11, 0, 0, TAU);
    cv.fill();

    if (mo > 0.22) {
      cv.setFillColorStr("rgba(252,246,244,0.92)");
      cv.beginPath();
      cv.ellipse(0, 24.5 + mo * 2, 6.5 + mo * 2, 2.6, 0, 0, TAU);
      cv.fill();
    }

    cv.setFillColorStr("rgba(205,100,118,0.95)");
    cv.beginPath();
    cv.ellipse(0, 32 + mo * 4.5, 7.5 + mo * 2.2, 2.4 + mo * 3.4, 0, 0, TAU);
    cv.fill();

    cv.setStrokeColorStr(lipCol);
    cv.setLineWidth(2.2);
    cv.beginPath();
    cv.moveTo(-10, 26.5);
    cv.quadraticCurveTo(-4, 23.5 - mo * 1.5, 0, 25.5);
    cv.quadraticCurveTo(4, 23.5 - mo * 1.5, 10, 26.5);
    cv.stroke();

    cv.setStrokeColorStr(lipCol);
    cv.setLineWidth(2.4);
    cv.beginPath();
    cv.moveTo(-9, 29 + mo * 4);
    cv.quadraticCurveTo(0, 34.5 + mo * 9.5, 9, 29 + mo * 4);
    cv.stroke();

    cv.setFillColorStr("rgba(255,250,248,0.55)");
    cv.beginPath();
    cv.ellipse(0, 33 + mo * 7.5, 4.2, 1.6, 0, 0, TAU);
    cv.fill();

    if (mo > 0.4 && (g.pleasure > 50 || g.oral > 0.3)) {
      cv.setStrokeColorStr("rgba(255,248,244,0.52)");
      cv.setLineWidth(1.3);
      cv.beginPath();
      cv.moveTo(-5, 27);
      cv.quadraticCurveTo(-3, 31 + mo * 4, -4, 33 + mo * 6);
      cv.stroke();
    }
  } else {
    cv.setStrokeColorStr(lipCol);
    cv.setLineWidth(2.4);
    cv.beginPath();
    cv.moveTo(-9, 28);
    cv.quadraticCurveTo(0, 30.5, 9, 28);
    cv.stroke();
    cv.setFillColorStr("rgba(255,245,240,0.45)");
    cv.beginPath();
    cv.ellipse(0, 29.5, 4.5, 1.6, 0, 0, TAU);
    cv.fill();
  }

  cv.setFillColorStr("rgba(85,42,45,0.7)");
  cv.beginPath();
  cv.arc(11, 21, 1.3, 0, TAU);
  cv.fill();

  cv.setFillColorStr(hairCol);
  cv.beginPath();
  cv.moveTo(-46, -12);
  cv.bezierCurveTo(-42, -48, -18, -60, 2, -58);
  cv.bezierCurveTo(24, -58, 44, -46, 46, -12);
  cv.quadraticCurveTo(30, -22, 14, -24);
  cv.quadraticCurveTo(0, -26, -14, -24);
  cv.quadraticCurveTo(-30, -22, -46, -12);
  cv.closePath();
  cv.fill();
  cv.setStrokeColorStr("rgba(0,0,0,0.35)");
  cv.setLineWidth(1.4);
  for (F64 sx : {-24.0, -8.0, 8.0, 24.0}) {
    cv.beginPath();
    cv.moveTo(sx, -52 + std::fabs(sx) * 0.15);
    cv.quadraticCurveTo(sx + 1, -38, sx - 1, -27);
    cv.stroke();
  }

  cv.save();
  cv.setCompStr("screen");
  cv.setStrokeColorStr("rgba(215,155,165,0.32)");
  cv.setLineWidth(2.4);
  cv.beginPath();
  cv.moveTo(-26, -48);
  cv.quadraticCurveTo(-8, -56, 14, -50);
  cv.stroke();
  cv.restore();

  if (g.blinkPh > 0) {
    cv.setStrokeColorStr("rgba(65,32,38,0.95)");
    cv.setLineWidth(3.2);
    for (int si : {-1, 1}) {
      F64 s = (F64)si;
      cv.beginPath();
      cv.moveTo(s * 17.5 - 11, -8);
      cv.lineTo(s * 17.5 + 11, -7.5);
      cv.stroke();
    }
  }

  cv.restore();
}

// ---- drawFPVBody (fpv.js:491-730) ----
static void fpv_body(Canvas& cv, Game& g, const FpvExpr& E, F64 br) {
  F64 trem = fpv_tremor(g);
  F64 bounce = g.depth * 8 + g.impact * 6;
  const F64 cx = 640, topY = 206 + bounce * 0.3;
  Tone T = herT(g, chars::getSkin(g));
  auto brgb = hexToRgb(g.ch.blushColor);
  F64 bsz = 0.72 + g.ch.breastSize * 0.62;
  F64 bdy = 0.82 + g.ch.bodyScale * 0.42;

  // Legs spread toward camera, foreshortened in perspective.
  F64 spread = (1.0 + g.pleasure * 0.0016) * bdy;
  for (int si : {-1, 1}) {
    F64 s = (F64)si;
    V2 hip{cx + s * 64 * bdy, 556 + bounce};
    V2 knee{cx + s * (204 * spread) + trem * s, 642 + bounce * 0.6};
    V2 foot{cx + s * (274 * spread), 758};

    LimbOpt legLo;
    legLo.belly = 1.16;
    limbS(cv, knee, foot, 34 * bdy, 22 * bdy, T, legLo);
    LimbOpt thLo;
    thLo.belly = 1.1;
    thLo.aoA = 0.32;
    limbS(cv, hip, knee, 46 * bdy, 37 * bdy, T, thLo);

    cv.save();
    cv.beginPath();
    cv.ellipse(knee.x, knee.y, 34 * bdy, 28 * bdy, s * 0.42, 0, TAU);
    cv.setFillGrad(gRadial(cv, knee.x - s * 8, knee.y - 10, 4, knee.x, knee.y,
                           36 * bdy, {{0, T.b}, {0.7, T.b}, {1, T.s}}));
    cv.fill();
    cv.restore();
    fHi(cv, knee.x, knee.y - 10, 13 * bdy, 15, "rgba(255,238,220,0.08)");

    cv.setStrokeColorStr("rgba(185,115,95,0.16)");
    cv.setLineWidth(2.4);
    cv.beginPath();
    cv.moveTo(hip.x + s * 22, hip.y - 28);
    cv.bezierCurveTo(knee.x - s * 10, knee.y - 62, knee.x + s * 4,
                     knee.y - 26, knee.x + s * 2, knee.y - 12);
    cv.stroke();

    cv.setStrokeColorStr("rgba(165,95,75,0.22)");
    cv.setLineWidth(2.2);
    cv.beginPath();
    cv.moveTo(cx + s * 44 * bdy, 528 + bounce);
    cv.quadraticCurveTo(cx + s * 115 * bdy, 566 + bounce, cx + s * 158 * bdy,
                        620);
    cv.stroke();
  }

  // Arms under the torso at rest so the shoulder joint reads connected.
  F64 a2 = sm(55, 80, g.pleasure);
  auto drawArms = [&]() {
    for (int si : {-1, 1}) {
      F64 s = (F64)si;
      V2 sh{cx + s * 58 * bdy, 248 + bounce * 0.3};
      V2 el{cx + s * 90 * bdy, lerp(352, 300, a2)};
      V2 ha{lerp(cx + s * 116 * bdy, cx + s * 64 * bdy, a2),
            lerp(462, 190, a2)};
      LimbOpt aLo;
      aLo.belly = 1.08;
      aLo.aoA = 0.3;
      limbS(cv, sh, el, 17, 13, T, aLo);
      LimbOpt fLo;
      fLo.belly = 1.05;
      limbS(cv, el, ha, 13, 10, T, fLo);
      F64 A = lerp(2.2, -0.9, a2);
      HandOpt ho;
      ho.curl = lerp(0.5, 0.65, a2);
      ho.spread = 0.35;
      fpv_handS(cv, ha.x, ha.y, s < 0 ? A : 3.141592653589793 - A, 1.0, T,
                ho);
    }
  };
  if (a2 < 0.5) drawArms();

  // Torso: hourglass, elevated ribcage, taut abdomen.
  PathFn torso = [=](Canvas& c) {
    c.moveTo(cx - 24 * bdy, topY);
    c.bezierCurveTo(cx - 70 * bdy, 258, cx - 78 * bdy, 308, cx - 74 * bdy, 335);
    c.bezierCurveTo(cx - 72 * bdy, 372, cx - 56 * bdy, 408, cx - 54 * bdy, 435);
    c.bezierCurveTo(cx - 52 * bdy, 465, cx - 84 * bdy, 532, cx - 82 * bdy, 546);
    c.bezierCurveTo(cx - 44 * bdy, 570, cx - 18 * bdy, 572, cx, 572);
    c.bezierCurveTo(cx + 18 * bdy, 572, cx + 44 * bdy, 570, cx + 82 * bdy, 546);
    c.bezierCurveTo(cx + 84 * bdy, 532, cx + 52 * bdy, 465, cx + 54 * bdy, 435);
    c.bezierCurveTo(cx + 56 * bdy, 408, cx + 72 * bdy, 372, cx + 74 * bdy, 335);
    c.bezierCurveTo(cx + 78 * bdy, 308, cx + 70 * bdy, 258, cx + 24 * bdy,
                    topY);
    c.closePath();
  };
  F64 tlg[4] = {cx - 60, topY - 30, cx + 40, 580};
  skFillShape(cv, torso, T, tlg);
  skClipIn(cv, torso, [&]() {
    fAO(cv, cx - 66 * bdy, 400, 16, 90, 0.30);
    fAO(cv, cx + 66 * bdy, 400, 16, 90, 0.30);
    fAO(cv, cx - 74 * bdy, 540, 26, 34, 0.15);
    fAO(cv, cx + 74 * bdy, 540, 26, 34, 0.15);
    fSh(cv, cx - 44 * bdy, 372, 30, 12, "rgba(160,90,70,0.22)", 0.18);
    fSh(cv, cx + 44 * bdy, 372, 30, 12, "rgba(160,90,70,0.22)", -0.18);
    fHi(cv, cx, 290, 24 * bdy, 95, "rgba(255,235,215,0.20)");
    fHi(cv, cx, 476, 30 * bdy, 52, "rgba(255,235,215,0.18)");
  });
  skLine(cv, torso, T, 1.4, 0.28);

  {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", brgb[0], brgb[1],
                  brgb[2], 0.05 + E.blush * 0.16);
    shade(cv, cx, 314, 60 * bdy, 30, buf, 0);
  }

  if (g.ar > 30) {
    F64 ra = (g.ar - 30) / 70;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", brgb[0], brgb[1],
                  brgb[2], 0.04 + 0.08 * ra);
    std::string rc = buf;
    const F64 px[4] = {cx - 30, cx + 30, cx - 24, cx + 24};
    const F64 py[4] = {318, 318, 418, 418};
    const F64 prx[4] = {26, 26, 28, 28};
    const F64 pry[4] = {16, 16, 18, 18};
    for (int i = 0; i < 4; i++)
      shade(cv, px[i], py[i], prx[i], pry[i], rc, 0);
  }

  F64 navelY = 450 + bounce * 0.5;
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, cx, navelY, 5.5, 7.5, "rgba(145,85,65,0.48)", 0);
  cv.restore();
  cv.setStrokeColorStr("rgba(155,95,75,0.52)");
  cv.setLineWidth(1.8);
  cv.beginPath();
  cv.ellipse(cx, navelY, 4.4, 6.2, 0, 0, TAU);
  cv.stroke();

  cv.setStrokeColorStr("rgba(155,95,75,0.12)");
  cv.setLineWidth(1.6);
  cv.beginPath();
  cv.moveTo(cx, 410 + bounce * 0.4);
  cv.lineTo(cx, navelY - 7);
  cv.moveTo(cx, navelY + 7);
  cv.lineTo(cx, 486 + bounce * 0.5);
  cv.stroke();

  cv.setStrokeColorStr("rgba(165,95,75,0.38)");
  cv.setLineWidth(2.0);
  cv.beginPath();
  cv.moveTo(cx - 38, 230);
  cv.quadraticCurveTo(cx - 14, 238, cx + 2, 234);
  cv.moveTo(cx + 38, 230);
  cv.quadraticCurveTo(cx + 14, 238, cx - 2, 234);
  cv.stroke();

  if (a2 >= 0.5) drawArms();

  // Breasts: supine gravity spread, lateral drape, erect nipples.
  F64 jig = g.breast.p * 0.95;
  F64 squash = sm(0.86, 1.0, g.depth);
  F64 erect = clamp(0.35 + 0.65 * (g.ar / 100), 0.0, 1.0);
  std::string nipCol = g.ch.nippleColor;

  for (int si : {-1, 1}) {
    F64 s = (F64)si;
    F64 bx = cx + s * (30 * bdy + bsz * 4.5);
    F64 by = 336 + br * 0.65 + jig * 0.58 + bounce * 0.4;
    F64 baseW = 34 * bsz * (1 + squash * 0.08) - jig * 0.1;
    F64 baseH = 40 * bsz * (1 - squash * 0.12) + jig * 0.15;

    cv.save();
    cv.translate(bx, by);
    cv.rotate(s * 0.10);

    cv.save();
    cv.setCompStr("multiply");
    shade(cv, s * 8, 16, baseW * 1.12, baseH * 1.0, "rgba(160,92,70,0.26)",
          s * 0.10);
    cv.restore();

    PathFn dome = [=](Canvas& c) {
      c.moveTo(0, -baseH * 0.88);
      c.bezierCurveTo(s * 6 - baseW * 0.55, -baseH * 0.6, -baseW * 1.08,
                      -baseH * 0.05, -baseW * 0.88, baseH * 0.46);
      c.bezierCurveTo(-baseW * 0.68, baseH * 0.94, -baseW * 0.15,
                      baseH * 1.02, 0, baseH);
      c.bezierCurveTo(baseW * 0.58, baseH * 0.96, baseW * 1.10, baseH * 0.56,
                      baseW * 0.96, 0);
      c.bezierCurveTo(baseW * 0.84, -baseH * 0.56, s * 8 + baseW * 0.46,
                      -baseH * 0.84, 0, -baseH * 0.88);
      c.closePath();
    };
    cv.beginPath();
    dome(cv);
    cv.setFillGrad(gRadial(cv, -s * baseW * 0.22, -baseH * 0.28, baseW * 0.12,
                           0, 0, baseW * 1.45,
                           {{0, T.hi}, {0.5, T.b}, {1, T.s}}));
    cv.fill();
    skClipIn(cv, dome, [&]() {
      fAO(cv, -s * baseW * 0.15, baseH * 0.86, baseW * 0.8, baseH * 0.24, 0.30);
      fSh(cv, s * baseW * 0.72, baseH * 0.1, baseW * 0.3, baseH * 0.7,
          "rgba(160,90,70,0.20)", s * 0.1);
    });

    fHi(cv, -s * 5, -baseH * 0.16, baseW * 0.5, baseH * 0.42,
        "rgba(255,238,220,0.30)");

    cv.setStrokeColorStr("rgba(155,90,72,0.34)");
    cv.setLineWidth(1.9);
    cv.beginPath();
    cv.ellipse(s * 2, baseH * 0.74, baseW * 0.46, baseH * 0.22, 0, 0.2,
               3.141592653589793 - 0.2);
    cv.stroke();

    F64 aDistX = s * 3.5, aDistY = -baseH * 0.06;
    F64 bszMax = bsz > 0.84 ? bsz : 0.84;
    F64 aer = (9.0 + 2.2 * clamp(g.ar / 100, 0.0, 1.0)) * bszMax;

    cv.setFillColorStr("rgba(206,128,110,0.36)");
    cv.beginPath();
    cv.ellipse(aDistX, aDistY, aer + 2.4, aer * 0.86, s * 0.1, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(216,138,118,0.62)");
    cv.beginPath();
    cv.ellipse(aDistX, aDistY, aer, aer * 0.82, s * 0.1, 0, TAU);
    cv.fill();

    cv.setFillColorStr("rgba(188,118,102,0.70)");
    for (int i = 0; i < 7; i++) {
      F64 ta = (F64)i / 7 * TAU + 0.35;
      cv.beginPath();
      cv.arc(aDistX + std::cos(ta) * aer * 0.72,
             aDistY + std::sin(ta) * aer * 0.62, 0.85, 0, TAU);
      cv.fill();
    }

    F64 bsz9 = bsz * 0.9 > 0.85 ? bsz * 0.9 : 0.85;
    F64 nr = (3.4 + 2.0 * erect) * bsz9;
    cv.setFillColorStr(nipCol);
    cv.beginPath();
    cv.ellipse(aDistX, aDistY, nr, nr * (1 + 0.18 * erect), s * 0.08, 0, TAU);
    cv.fill();

    cv.setFillColorStr("rgba(255,245,245,0.42)");
    cv.beginPath();
    cv.arc(aDistX - 1.1, aDistY - 1.1, nr * 0.30, 0, TAU);
    cv.fill();

    cv.restore();
  }

  cv.save();
  cv.setCompStr("multiply");
  shade(cv, cx, 342 + br * 0.65 + jig * 0.58 + bounce * 0.4, 10, 36 * bsz,
        "rgba(160,92,70,0.22)", 0);
  cv.restore();

  // The spot (shared front-view renderer).
  F64 vy = 554 + bounce;
  F64 eng = clamp(g.ar / 100, 0.0, 1.0);
  F64 iopen = 4.5 + 10.5 * g.depth;

  if (g.ar > 20) {
    cv.save();
    cv.setCompStr("screen");
    shade(cv, cx, vy, 28 + 12 * eng, 22, "rgba(255,160,165,0.15)", 0);
    cv.restore();
  }

  fpv_vulvaS(cv, g, cx, vy, iopen, eng, T, "front");
}

// ---- smoothing state (fpv.js:735-736) + fpvSmTo (738-746) ----
struct FpvSm {
  F64 x = 0, y = 0;
  std::string v;
  bool ok = false;
};
static FpvSm fpv_smL, fpv_smR;
static V2 fpv_smTo(FpvSm& o, const Game& g, F64 tx, F64 ty) {
  if (!o.ok || o.v != g.view) {
    o.x = tx;
    o.y = ty;
    o.v = g.view;
    o.ok = true;
  } else {
    o.x = lerp(o.x, tx, 0.22);
    o.y = lerp(o.y, ty, 0.22);
  }
  return {o.x, o.y};
}

// ---- drawFPVKnead (fpv.js:748-787) ----
static void fpv_knead(Canvas& cv, const Game& g, F64 hx, F64 hy, F64 rub,
                      bool small) {
  int nf = small ? 2 : 3;
  cv.setFillColorStr("#b88255");
  for (int i = 0; i < nf; i++) {
    F64 a = (small ? 0.6 : 0.52) + i * (small ? 0.7 : 0.52) +
            std::sin(g.t * 9 + i) * 0.16;
    cv.beginPath();
    cv.ellipse(hx + std::cos(a) * (small ? 14 : 19),
               hy + std::sin(a) * (small ? 11 : 15), small ? 5.5 : 7.5,
               small ? 3.8 : 4.8, a, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,235,225,0.7)");
    cv.beginPath();
    cv.arc(hx + std::cos(a) * (small ? 15 : 20),
           hy + std::sin(a) * (small ? 12 : 16), 1.2, 0, TAU);
    cv.fill();
    cv.setFillColorStr("#b88255");
  }

  {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,225,205,%.17g)", 0.34 * rub);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(2.6);
    cv.beginPath();
    cv.ellipse(hx, hy, small ? 24 : 42, small ? 22 : 40, 0, g.t * 9,
               g.t * 9 + 1.8);
    cv.stroke();
  }

  for (int i = 0; i < 2; i++) {
    F64 u = std::fmod(g.t * 1.6 + i * 0.5, 1.0);
    if (u < 0) u += 1.0;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,145,170,%.17g)",
                  (1 - u) * 0.35 * rub);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(2.0);
    cv.beginPath();
    cv.ellipse(hx, hy, (small ? 11 : 18) + u * (small ? 28 : 46),
               (small ? 10 : 16) + u * (small ? 26 : 42), 0, 0, TAU);
    cv.stroke();
  }
}

// ---- drawFPVHands (fpv.js:789-852) ----
// TODO: Heart has no vy (JS pushes vy:-rr(20,36)); push drops vy — mech update
// won't advect these hearts until core Heart gains vy.
static void fpv_hands(Canvas& cv, Game& g, F64 br) {
  F64 rub = g.rub, kiss = g.kiss;
  int zone = g.rubZone | 0;
  F64 jig = g.breast.p * 0.95;
  F64 bounce = g.depth * 8 + g.impact * 6;
  F64 bsz = 0.72 + g.ch.breastSize * 0.62;
  F64 bdy = 0.82 + g.ch.bodyScale * 0.42;

  V2 LB{640 - (30 * bdy + bsz * 4.5), 336 + br * 0.65 + jig * 0.58 + bounce * 0.4};
  V2 RB{640 + (30 * bdy + bsz * 4.5), 336 + br * 0.65 + jig * 0.58 + bounce * 0.4};
  V2 LOW{640, 544 + bounce};

  bool lWork = zone == 0 || zone == 1;
  bool rWork = zone == 0 || zone == 2;
  bool lowWork = zone == 3;
  bool embracing = kiss >= 0.5;
  bool active = rub > 0.12 && !embracing;

  V2 lBase{170, 755}, lEl{370, 626};
  V2 lT{495, 536 + g.depth * 6};
  if (active && lWork) {
    lT = {LB.x + std::sin(g.t * 9 + 3.141592653589793) * 12 * rub,
          LB.y + std::cos(g.t * 7.4 + 3.141592653589793) * 10 * rub};
  }
  V2 lHand = fpv_smTo(fpv_smL, g, lT.x, lT.y);
  lHand = {lerp(lHand.x, 568, kiss), lerp(lHand.y, 248, kiss)};

  Tone HT = himT(g, chars::getSkin(g));
  LimbOpt hLo;
  hLo.belly = 1.08;
  hLo.aoA = 0.3;
  limbS(cv, lBase, lEl, 30, 24, HT, hLo);
  LimbOpt hLo2;
  hLo2.belly = 1.05;
  limbS(cv, lEl, lHand, 22, 17, HT, hLo2);
  HandOpt hd;
  hd.curl = 0.62;
  hd.spread = 0.3;
  fpv_handS(cv, lHand.x, lHand.y, 3.141592653589793 - 0.6, 1.32, HT, hd);

  V2 rBase{1110, 755}, rEl{910, 626};
  V2 rT{785, 536 + g.depth * 6};
  if (active) {
    if (rWork)
      rT = {RB.x + std::sin(g.t * 9) * 12 * rub,
            RB.y + std::cos(g.t * 7.4) * 10 * rub};
    else if (lowWork)
      rT = {LOW.x + std::sin(g.t * 11) * 7.5 * rub,
            LOW.y + std::cos(g.t * 9) * 6.5 * rub};
  }
  V2 rHand = fpv_smTo(fpv_smR, g, rT.x, rT.y);
  rHand = {lerp(rHand.x, 712, kiss), lerp(rHand.y, 248, kiss)};

  limbS(cv, rBase, rEl, 30, 24, HT, hLo);
  limbS(cv, rEl, rHand, 22, 17, HT, hLo2);
  fpv_handS(cv, rHand.x, rHand.y, 3.141592653589793 + 0.6, 1.32, HT, hd);

  if (!active) return;

  cv.save();
  cv.setGlobalAlpha(clamp(rub * 1.4, 0.0, 1.0));
  if (lWork) {
    fpv_knead(cv, g, lHand.x, lHand.y, rub, false);
    if (fpv_R() < 0.025 * rub)
      g.hearts.push_back(
          {592 + fpv_rr(-15, 15), 460 + fpv_rr(-8, 8), 1.2,
           fpv_R() * TAU, fpv_rr(0.4, 0.7)});
  }
  if (rWork) {
    fpv_knead(cv, g, rHand.x, rHand.y, rub, false);
    if (fpv_R() < 0.025 * rub)
      g.hearts.push_back(
          {688 + fpv_rr(-15, 15), 460 + fpv_rr(-8, 8), 1.2,
           fpv_R() * TAU, fpv_rr(0.4, 0.7)});
  }
  if (lowWork) {
    fpv_knead(cv, g, rHand.x, rHand.y, rub, true);
    if (fpv_R() < 0.035 * rub)
      g.hearts.push_back(
          {640 + fpv_rr(-12, 12), 695 + fpv_rr(-8, 8), 1.2,
           fpv_R() * TAU, fpv_rr(0.4, 0.7)});
  }
  cv.restore();
}

// ---- drawFPVShaft (fpv.js:857-1001) ----
static void fpv_shaft(Canvas& cv, Game& g) {
  F64 d = g.depth;
  F64 sway = std::sin(g.t * 1.7) * 3.2;
  F64 tipX = 640 + sway, tipY;
  if (g.state == "climax" || g.state == "finish") {
    tipY = 542 + std::sin(g.t * 30) * 2;
  } else {
    tipY = lerp(652, 552, clamp(d / 0.35, 0.0, 1.0)) -
           6 * clamp((d - 0.35) / 0.65, 0.0, 1.0);
  }

  const F64 wb = 27, wt = 17, baseY = 794;
  F64 pu = 1 + 0.36 * g.shaftPulse;

  if (d < 0.55 && g.state != "finish") {
    F64 bs = 1 - d * 0.88;
    cv.setFillGrad(sg(cv, 695, 775, "#c58a66", "#a56840"));
    cv.beginPath();
    cv.ellipse(604, 762, 25 * bs, 21 * bs, 0.22, 0, TAU);
    cv.fill();
    cv.setFillGrad(sg(cv, 695, 775, "#ce8f69", "#a56840"));
    cv.beginPath();
    cv.ellipse(676, 762, 23 * bs, 20 * bs, -0.22, 0, TAU);
    cv.fill();

    cv.setStrokeColorStr("rgba(85,48,28,0.38)");
    cv.setLineWidth(2.0);
    cv.beginPath();
    cv.moveTo(640, 752);
    cv.quadraticCurveTo(640, 768, 640, 780);
    cv.stroke();
  }

  cv.setFillGrad(sg(cv, 640, 716, "#d8976a", "#9a5f3a"));
  cv.beginPath();
  cv.ellipse(640, 762, 72, 48, 0, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 640, 742, 46, 14, "rgba(85,45,24,0.30)", 0);
  cv.restore();

  cv.setFillGrad(gLinear(cv, 640 - wb, 0, 640 + wb, 0,
                         {{0, "#a56840"},
                          {0.24, "#d18f60"},
                          {0.48, "#eeb488"},
                          {0.72, "#d69364"},
                          {1, "#925c38"}}));
  cv.beginPath();
  cv.moveTo(640 - wb, baseY);
  cv.quadraticCurveTo(640 - wb + 6, (baseY + tipY) / 2, tipX - wt * pu,
                      tipY + 8);
  cv.quadraticCurveTo(tipX, tipY - 2, tipX + wt * pu, tipY + 8);
  cv.quadraticCurveTo(640 + wb - 6, (baseY + tipY) / 2, 640 + wb, baseY);
  cv.closePath();
  cv.fill();

  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 640 - 17, (baseY + tipY) / 2, 12, std::fabs(baseY - tipY) / 2,
        "rgba(85,45,24,0.25)", 0);
  cv.restore();

  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, 640 - 4, (baseY + tipY) / 2, 14, std::fabs(baseY - tipY) / 2,
        "rgba(255,230,200,0.38)", 0);
  cv.restore();

  {
    F64 contactA = 0.18 + 0.26 * clamp(d, 0.0, 1.0);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(22,6,10,%.17g)", contactA);
    cv.save();
    cv.setCompStr("multiply");
    shade(cv, 640, 556, 30 - 8 * clamp(d, 0.0, 1.0), 10, buf, 0);
    cv.restore();
  }

  if (g.ar > 35) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(255,245,240,%.17g)",
                  0.10 + 0.14 * (g.ar / 100));
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(2.4);
    cv.beginPath();
    cv.ellipse(640, 556, 24, 8, 0, 0, TAU);
    cv.stroke();
  }

  cv.setStrokeColorStr("rgba(135,75,55,0.40)");
  cv.setLineWidth(3.2);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(632, 765);
  cv.quadraticCurveTo(626, (765 + tipY) / 2 + 18, 635, tipY + 48);
  cv.stroke();

  cv.setStrokeColorStr("rgba(135,75,55,0.28)");
  cv.setLineWidth(2.1);
  cv.beginPath();
  cv.moveTo(630, 705);
  cv.quadraticCurveTo(642, 682, 648, 658);
  cv.stroke();

  if (tipY > 580 || g.state == "climax") {
    cv.setFillColorStr("#c7846f");
    cv.beginPath();
    cv.ellipse(tipX, tipY + 8, (wt + 3.0) * pu, 7.5 * pu, 0, 0, TAU);
    cv.fill();

    cv.setFillGrad(gLinear(cv, tipX - wt, 0, tipX + wt, 0,
                           {{0, "#c7846f"},
                            {0.45, "#e9aa8f"},
                            {1, "#b67562"}}));
    cv.beginPath();
    cv.ellipse(tipX, tipY - 4, (wt - 0.5) * pu, 16 * pu, 0, 0, TAU);
    cv.fill();

    cv.setStrokeColorStr("rgba(115,55,50,0.65)");
    cv.setLineWidth(2.2);
    cv.beginPath();
    cv.moveTo(tipX, tipY - 17);
    cv.lineTo(tipX, tipY - 8);
    cv.stroke();

    cv.setFillColorStr("rgba(255,255,255,0.35)");
    cv.beginPath();
    cv.ellipse(tipX - 6, tipY - 8, 3.8, 7.5, -0.2, 0, TAU);
    cv.fill();
  }

  if (g.ar > 26) {
    cv.save();
    cv.setCompStr("screen");
    cv.setStrokeColorStr("rgba(255,255,255,0.32)");
    cv.setLineWidth(4.2);
    cv.beginPath();
    cv.moveTo(622, 745);
    cv.quadraticCurveTo(627, (745 + tipY) / 2, tipX - 10, tipY + 12);
    cv.stroke();
    cv.restore();
  }
}

// ---- drawFPVFluids (fpv.js:1006-1063) ----
static void fpv_fluids(Canvas& cv, Game& g) {
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

  for (const auto& gl : g.glisten) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,250,245,%.17g)", gl.a * 0.65);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(640 + (gl.x - 640) * 0.3, 556 + (gl.y - 546) * 0.4, 2.8, 1.8,
               0, 0, TAU);
    cv.fill();
  }

  for (const auto& j : g.jets) {
    if (j.view != 'f') continue;
    F64 spd = std::hypot(j.vx, j.vy);
    F64 a = std::atan2(j.vy, j.vx);
    F64 al = clamp(j.life * 2.4, 0.0, 1.0);

    char buf[48];
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

  int si = 0;
  for (const auto& s : g.sweat) {
    F64 a = 0.45 * s.life;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(235,248,255,%.17g)", a);
    cv.setFillColorStr(buf);
    cv.beginPath();
    if (si % 2) {
      cv.ellipse(640 + (s.x - 318) * 1.5, 140 + (s.y - 462) * 0.5, 1.5, 2.4,
                 0, 0, TAU);
    } else {
      cv.ellipse(640 + (s.x - 318) * 2.2, 300 + (s.y - 462) * 1.2, 1.5, 2.4,
                 0, 0, TAU);
    }
    cv.fill();
    si++;
  }

  for (const auto& h : g.hearts) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,105,140,%.17g)",
                  0.58 * (h.life < 1 ? h.life : 1));
    cv.setFillColorStr(buf);
    heartPath(cv, 640 + (h.x - 655) + std::sin(h.ph) * 8,
              340 + (h.y - 485) * 0.8, h.s);
    cv.fill();
  }
}

} // namespace

// ---- drawFPV main compositor + cinematic camera (fpv.js:1068-1120) ----
void draw(Canvas& cv, Game& g) {
  fpv_room(cv, g);

  FpvExpr E = fpv_herExpression(g);
  F64 br = std::sin(g.t * TAU * (0.16 + g.pleasure * 0.004)) * 2.2;

  // Camera targets: full (1.0x), breasts (1.65x), hips/the spot (1.78x),
  // face (1.85x).
  F64 tZoom = 1.0, tPanX = 640, tPanY = 360;
  if (g.fpvFocus == "breasts") {
    tZoom = 1.65;
    tPanX = 640;
    tPanY = 348;
  } else if (g.fpvFocus == "hips") {
    tZoom = 1.78;
    tPanX = 640;
    tPanY = 548;
  } else if (g.fpvFocus == "face") {
    tZoom = 1.85;
    tPanX = 640;
    tPanY = 172;
  }

  g.fpvZoom = lerp(g.fpvZoom, tZoom, 0.08);
  g.fpvPanX = lerp(g.fpvPanX, tPanX, 0.08);
  g.fpvPanY = lerp(g.fpvPanY, tPanY, 0.08);

  F64 curZ = g.fpvZoom, px = g.fpvPanX, py = g.fpvPanY;

  // POV camera dynamics: respiration sway + thrust tremor + tachycardia pulse.
  F64 swayX =
      std::sin(g.t * 0.9) * 2.6 + (g.tired ? std::sin(g.t * 7.5) * 1.3 : 0);
  F64 swayY = std::sin(g.t * TAU * 0.16) * 2.1 + g.kiss * 10 - g.depth * 4.2;

  F64 beat = 0;
  if (g.pleasure > 72) {
    F64 bpm = 74 + (g.pleasure - 72) * 1.25 + g.orgasms * 8.5;
    beat = (std::sin(g.t * bpm * 3.141592653589793 / 30) > 0
                ? std::sin(g.t * bpm * 3.141592653589793 / 30)
                : 0) *
           0.95;
  }

  cv.save();
  cv.translate(640, 360);
  cv.scale(curZ, curZ);
  cv.translate(-px + swayX + beat * 0.75, -py + swayY + beat * 0.55);

  fpv_body(cv, g, E, br);
  fpv_hands(cv, g, br);
  fpv_shaft(cv, g);
  fpv_head(cv, g, E);
  fpv_fluids(cv, g);

  cv.restore();

  // Out-of-focus foreground framing: broad shoulders leaning in.
  cv.save();
  cv.setFillColorStr("rgba(145,85,55,0.18)");
  cv.beginPath();
  cv.ellipse(60, 762, 230, 125, 0.32, 0, TAU);
  cv.fill();
  cv.beginPath();
  cv.ellipse(1220, 762, 230, 125, -0.32, 0, TAU);
  cv.fill();

  cv.setFillColorStr("rgba(10,3,6,0.38)");
  cv.beginPath();
  cv.ellipse(38, 785, 210, 95, 0.32, 0, TAU);
  cv.fill();
  cv.beginPath();
  cv.ellipse(1242, 785, 210, 95, -0.32, 0, TAU);
  cv.fill();
  cv.restore();
}

} // namespace fpv
} // namespace ag
