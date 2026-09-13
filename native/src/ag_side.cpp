// Afterglow native — ag_side.cpp
// 1:1 port of js/side.js (missionary side-view couple, coherent anatomy).
// Entry points (see ag_2dmods.h): ag::side::draw (full side scene pass) and
// ag::side::herExpression (autonomic expression state, shared with 3D).
// Numeric order of operations mirrors JS exactly; Canvas calls map 1:1
// (X.* -> Canvas::* , gLinear/gRadial helpers, sp/hp shear, skin/gfx prims).
// Classic-script scope: no imports/exports; only this file + file-local
// static side_-prefixed helpers.
#include "ag_2dmods.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

// ---- file-local expression state (mirrors JS herExpression() return) ----
struct side_Expr {
  double eye = 0.72;
  double rolled = 0;
  double mouth = 0;
  double blush = 0.12;
  double brow = 0;
  double tilt = 0.12;
};

struct side_HerOut {
  side_Expr E;
  double wrap = 0;
  double br = 0;
};

// ---- tone helpers (js/skin.js skLight/skDark/herT/himT + side.js herFarT) ----
static std::string side_skLight(std::string_view h, double t) {
  return ag::lerpHex(h, "#fff4ea", t);
}
static std::string side_skDark(std::string_view h, double t) {
  return ag::lerpHex(h, "#3a1410", t);
}
static ag::Tone side_herT(const ag::Game& g) {
  ag::SkinPair sk = ag::chars::getSkin(g);
  return ag::herT(g, sk);
}
static ag::Tone side_himT(const ag::Game& g) {
  ag::SkinPair sk = ag::chars::getSkin(g);
  return ag::himT(g, sk);
}
static ag::Tone side_herFarT(const ag::Game& g) {
  ag::SkinPair sk = ag::chars::getSkin(g);
  return ag::skTone(side_skDark(sk.her, 0.30));
}
static ag::Tone side_himFarT(const ag::Game& g) {
  ag::SkinPair sk = ag::chars::getSkin(g);
  return ag::skTone(side_skDark(sk.him, 0.30));
}

// ---- js/skin.js:335-340 hairMassS (missing from ag_skin.h) ----
static void side_hairMassS(ag::Canvas& cv, double cx, double cy, double rx,
                           double ry, double rot, std::string_view col) {
  std::string c(col);
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, rot, 0, ag::TAU);
  cv.setFillGrad(ag::gRadial(cv, cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy,
                             rx * 1.2,
                             {{0, side_skLight(c, 0.22)},
                              {0.55, c},
                              {1, side_skDark(c, 0.4)}}));
  cv.fill();
}

// ---- js/skin.js:308-333 tressS ribbon (missing from ag_skin.h) ----
static void side_tressS(ag::Canvas& cv, double x0, double y0, double c1x,
                        double c1y, double c2x, double c2y, double x1, double y1,
                        double w0, double w1, std::string_view col,
                        std::string_view sheen = {}) {
  const double P[4][2] = {{x0, y0}, {c1x, c1y}, {c2x, c2y}, {x1, y1}};
  auto off = [&](double t, double w, double& ox, double& oy) {
    double mt = 1 - t;
    double bx = mt * mt * mt * P[0][0] + 3 * mt * mt * t * P[1][0] +
                3 * mt * t * t * P[2][0] + t * t * t * P[3][0];
    double by = mt * mt * mt * P[0][1] + 3 * mt * mt * t * P[1][1] +
                3 * mt * t * t * P[2][1] + t * t * t * P[3][1];
    double dx = 3 * mt * mt * (P[1][0] - P[0][0]) +
                6 * mt * t * (P[2][0] - P[1][0]) + 3 * t * t * (P[3][0] - P[2][0]);
    double dy = 3 * mt * mt * (P[1][1] - P[0][1]) +
                6 * mt * t * (P[2][1] - P[1][1]) + 3 * t * t * (P[3][1] - P[2][1]);
    double L = std::hypot(dx, dy);
    if (L < 1e-9) L = 1;
    ox = bx - dy / L * w;
    oy = by + dx / L * w;
  };
  auto wAt = [&](double t) { return ag::lerp(w0, w1, t); };
  cv.beginPath();
  double px = 0, py = 0;
  off(0, wAt(0), px, py);
  cv.moveTo(px, py);
  for (double t = 0.2; t <= 1.001; t += 0.2) {
    off(t, wAt(t), px, py);
    cv.lineTo(px, py);
  }
  for (double t = 1.0; t >= -0.001; t -= 0.2) {
    off(t, -wAt(t), px, py);
    cv.lineTo(px, py);
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

// ---- js/skin.js:279-303 neckS + trapezius (missing from ag_skin.h) ----
static void side_neckS(ag::Canvas& cv, ag::V2 top, ag::V2 base, double w,
                       const ag::Tone& T, bool line = true) {
  double dx = base.x - top.x, dy = base.y - top.y;
  double L = std::hypot(dx, dy);
  if (L < 1e-9) L = 1;
  double nx = -dy / L, ny = dx / L;
  ag::PathFn path = [=](ag::Canvas& c) {
    c.moveTo(top.x + nx * w * 0.62, top.y + ny * w * 0.62);
    c.quadraticCurveTo(base.x + nx * w * 0.7, base.y + ny * w * 0.7 - 4,
                       base.x + nx * w * 1.5, base.y + ny * w * 1.5);
    c.lineTo(base.x - nx * w * 1.5, base.y - ny * w * 1.5);
    c.quadraticCurveTo(base.x - nx * w * 0.7, base.y - ny * w * 0.7 - 4,
                       top.x - nx * w * 0.62, top.y - ny * w * 0.62);
    c.closePath();
  };
  double lg[4] = {top.x + nx * w, top.y + ny * w, top.x - nx * w,
                  top.y - ny * w};
  ag::skFillShape(cv, path, T, lg);
  ag::skClipIn(cv, path, [&]() {
    cv.save();
    cv.setCompStr("multiply");
    cv.setStrokeColorStr("rgba(120,55,45,0.30)");
    cv.setLineWidth(w * 0.22);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(top.x - nx * w * 0.25, top.y - ny * w * 0.25 + 2);
    cv.quadraticCurveTo((top.x + base.x) / 2 - nx * w * 0.1,
                        (top.y + base.y) / 2 - ny * w * 0.1,
                        base.x - nx * w * 0.5, base.y - ny * w * 0.5);
    cv.stroke();
    cv.restore();
    ag::fAO(cv, top.x, top.y + 2, w * 0.9, w * 0.5, 0.35);
  });
  if (line) ag::skLine(cv, path, T, 1.2, 0.3);
}

// ---- js/side.js:7-36 herExpression (numeric 1:1) ----
// TODO(Game): C++ MoanMouth{x,y,t} lacks JS {t0,dur,i}; below maps t->t0 with
//   dur=0.8,i=0.6 until Game gains t0/dur/i (see audio.js playMoan push).
// TODO(Game): C++ Game::speech is std::string; JS speech is {txt,until}|null.
//   Below treats non-empty speech as active (+0.28 mouth).
static side_Expr side_calcExpr(const ag::Game& g) {
  double p = g.pleasure / 100.0;
  double ar = g.ar / 100.0;
  double t = g.t;
  side_Expr o;
  o.eye = ag::lerp(0.72, 0.12, ag::sm(0.08, 0.92, p));
  o.rolled = 0;
  o.mouth = p * 0.36;
  o.blush = 0.12 + p * 0.44 + ar * 0.22;
  o.brow = ag::lerp(-0.06, 0.46, ag::sm(0.20, 0.90, p));
  o.tilt = 0.12 + p * 0.0035;
  for (const auto& m : g.mouths) {
    double t0 = m.t; // TODO: picks m.t as t0 (see note above)
    double dur = 0.8;
    double inten = 0.6;
    double u = (t - t0) / dur;
    if (u > 0 && u < 1)
      o.mouth += std::pow(std::sin(ag::TAU / 2 * u), 0.75) * inten * 0.65;
  }
  if (!g.speech.empty()) o.mouth += 0.28;
  if (g.blinkPh > 0) o.eye *= (1 - g.blinkPh);
  if (g.state == "orgasm") {
    double e = std::sin(ag::TAU / 2 * ag::clamp(g.orgT / 5.2, 0.0, 1.0));
    o.rolled = 0.70 + e * 0.30;
    o.eye = ag::lerp(o.eye, 0.06, e);
    o.mouth = o.mouth > 0.88 * e ? o.mouth : 0.88 * e;
    o.tilt = 0.50;
    o.blush = 1.0;
    o.brow = 0.65;
  }
  if (g.after > 0) {
    o.eye = o.eye < 0.16 ? o.eye : 0.16;
    o.mouth = o.mouth > 0.18 ? o.mouth : 0.18;
    o.blush = o.blush > 0.45 ? o.blush : 0.45;
  }
  if (g.state == "finish" && g.finishT > 1) {
    o.eye = 0.06;
    o.mouth = 0.22;
  }
  if (g.kiss > 0.6) o.mouth = o.mouth < 0.45 ? o.mouth : 0.45;
  o.eye = ag::clamp(o.eye, 0.0, 1.0);
  o.rolled = ag::clamp(o.rolled, 0.0, 1.0);
  o.mouth = ag::clamp(o.mouth, 0.0, 1.0);
  o.blush = ag::clamp(o.blush, 0.0, 1.0);
  return o;
}

// ---- js/side.js:47-135 drawHer ----
static side_HerOut side_drawHer(ag::Canvas& cv, ag::Game& g) {
  side_HerOut out;
  out.E = side_calcExpr(g);
  ag::Tone T = side_herT(g);
  double t = g.t;
  double p = g.pleasure / 100.0;
  out.wrap = ag::sm(0.50, 0.88, p);
  double wrap = out.wrap;
  out.br = std::sin(t * ag::TAU * (0.16 + p * 0.0045)) * (2.4 + p * 1.6);
  double br = out.br;
  std::string hairCol = g.ch.hairColor;
  ag::V2 hc = ag::sp(g, 312, 504, 0.008);

  cv.save();
  cv.setCompStr("multiply");
  ag::shade(cv, hc.x - 30, hc.y + 44, 110, 34, "rgba(15,5,8,0.5)", 0);
  cv.restore();
  auto hairBed = [&](ag::Canvas& c) {
    c.moveTo(hc.x - 6, hc.y - 26);
    c.bezierCurveTo(hc.x - 60, hc.y - 20, hc.x - 104, hc.y + 12, hc.x - 96,
                    hc.y + 44);
    c.bezierCurveTo(hc.x - 88, hc.y + 66, hc.x - 30, hc.y + 70, hc.x + 16,
                    hc.y + 58);
    c.bezierCurveTo(hc.x + 44, hc.y + 50, hc.x + 40, hc.y + 20, hc.x + 26,
                    hc.y + 4);
    c.bezierCurveTo(hc.x + 16, hc.y - 10, hc.x + 6, hc.y - 20, hc.x - 6,
                    hc.y - 26);
    c.closePath();
  };
  cv.beginPath();
  hairBed(cv);
  cv.setFillGrad(ag::gLinear(cv, hc.x - 90, hc.y - 20, hc.x + 30, hc.y + 60,
                             {{0, side_skLight(hairCol, 0.16)},
                              {0.5, hairCol},
                              {1, side_skDark(hairCol, 0.35)}}));
  cv.fill();
  cv.save();
  cv.beginPath();
  hairBed(cv);
  cv.clip();
  cv.setStrokeColorStr(side_skDark(hairCol, 0.45));
  cv.setLineWidth(2.2);
  cv.setLineCapStr("round");
  for (int i = 0; i < 4; i++) {
    cv.beginPath();
    cv.moveTo(hc.x - 2, hc.y - 14 + i * 6);
    cv.quadraticCurveTo(hc.x - 56, hc.y + 4 + i * 12, hc.x - 88 + i * 10,
                        hc.y + 40 + i * 6);
    cv.stroke();
  }
  cv.setCompStr("soft-light");
  ag::shade(cv, hc.x - 46, hc.y + 6, 52, 20, "rgba(255,205,215,0.20)", -0.2);
  cv.restore();

  ag::Tone FT = side_herFarT(g);
  ag::V2 fShld = ag::sp(g, 386, 524, 0.02);
  ag::V2 fElb = ag::sp(g, 356, 548, 0.01);
  ag::V2 fWr = ag::sp(g, 322, 554, 0.0);
  ag::LimbOpt o1;
  o1.belly = 1.05;
  o1.aoA = 0.3;
  ag::limbS(cv, fShld, fElb, 12, 9.5, FT, o1);
  ag::LimbOpt o2;
  o2.belly = 1.02;
  ag::limbS(cv, fElb, fWr, 9.5, 7, FT, o2);
  ag::HandOpt ho1;
  ho1.curl = 0.12;
  ho1.spread = 0.4;
  ag::handS(cv, g, fWr.x, fWr.y, 2.7, 0.88, FT, ho1);

  ag::V2 fHip = ag::sp(g, 626, 522, 0.07);
  ag::V2 fKnee{744 + wrap * 10, 450 + wrap * 6};
  ag::V2 fAnk{696 + wrap * 8, 424 + wrap * 6};
  ag::LimbOpt o3;
  o3.belly = 1.12;
  o3.aoA = 0.3;
  ag::limbS(cv, fHip, fKnee, 23, 15, FT, o3);
  ag::LimbOpt o4;
  o4.belly = 1.18;
  ag::limbS(cv, fKnee, fAnk, 13, 7.5, FT, o4);
  ag::footS(cv, fAnk.x - 6, fAnk.y - 2, 2.6, 0.85, FT);

  ag::V2 nk = ag::sp(g, 362, 522, 0.015);
  ag::V2 ch = ag::sp(g, 448, 505 + br * 0.4, 0.03);
  ag::V2 wa = ag::sp(g, 540, 514, 0.05);
  ag::V2 mo = ag::sp(g, 652, 505, 0.085);
  ag::V2 hpPt = ag::sp(g, 620, 516, 0.07);
  const double backY = 542;
  ag::PathFn torso = [=](ag::Canvas& c) {
    c.moveTo(nk.x, nk.y - 8);
    c.bezierCurveTo(ch.x - 44, ch.y - 16, ch.x - 18, ch.y - 15, ch.x, ch.y - 12);
    c.bezierCurveTo(wa.x - 40, wa.y - 9, wa.x - 16, wa.y - 8, wa.x, wa.y - 7);
    c.bezierCurveTo(mo.x - 36, mo.y - 12, mo.x - 12, mo.y - 14, mo.x, mo.y - 11);
    c.bezierCurveTo(hpPt.x + 22, hpPt.y - 8, hpPt.x + 26, hpPt.y + 4,
                    hpPt.x + 16, hpPt.y + 14);
    c.bezierCurveTo(wa.x + 32, backY + 3, wa.x - 10, backY + 3, wa.x, backY + 2);
    c.bezierCurveTo(ch.x + 32, backY + 1, ch.x - 10, backY, ch.x, backY);
    c.bezierCurveTo(nk.x + 26, backY - 2, nk.x + 6, nk.y + 8, nk.x, nk.y + 8);
    c.closePath();
  };
  double lg[4] = {0, ch.y - 22, 0, backY + 6};
  ag::skFillShape(cv, torso, T, lg);
  ag::skClipIn(cv, torso, [&]() {
    ag::fAO(cv, wa.x, backY, 92, 10, 0.42);
    ag::fAO(cv, ch.x, backY - 2, 62, 9, 0.34);
    ag::fSh(cv, wa.x - 6, wa.y - 1, 34, 9, "rgba(150,80,64,0.20)", 0);
    ag::fHi(cv, ch.x - 4, ch.y - 8, 46, 10, "rgba(255,240,226,0.26)", -0.06);
    ag::fHi(cv, mo.x - 8, mo.y - 8, 26, 8, "rgba(255,240,226,0.24)", -0.1);
  });
  ag::skLine(cv, torso, T, 1.4, 0.3);

  ag::V2 nv = ag::sp(g, 566, 512, 0.045);
  cv.setFillColorStr("rgba(140,78,62,0.4)");
  cv.beginPath();
  cv.ellipse(nv.x, nv.y, 2.6, 3.6, 0.15, 0, ag::TAU);
  cv.fill();

  auto bl = ag::hexToRgb(g.ch.blushColor);
  char buf[96];
  std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", (double)bl[0],
                (double)bl[1], (double)bl[2], 0.08 + out.E.blush * 0.22);
  cv.setFillColorStr(buf);
  cv.beginPath();
  cv.ellipse(ch.x + 4, ch.y - 3, 40, 13, -0.1, 0, ag::TAU);
  cv.fill();
  return out;
}

// ---- js/side.js:140-228 drawHerHead ----
static void side_drawHerHead(ag::Canvas& cv, ag::Game& g, const side_Expr& E) {
  ag::Tone T = side_herT(g);
  ag::V2 neckB = ag::sp(g, 364, 520, 0.015);
  ag::V2 headC = ag::sp(g, 312, 504, 0.008);
  std::string hairCol = g.ch.hairColor;

  side_neckS(cv, ag::V2{headC.x + 12, headC.y + 14},
             ag::V2{neckB.x - 2, neckB.y - 2}, 12, T);

  cv.save();
  cv.translate(headC.x, headC.y - g.pleasure * 0.02);
  cv.rotate(-0.06 + std::sin(g.t * ag::TAU * 0.33) * 0.015 - g.nod * 0.06);

  side_hairMassS(cv, -2, -4, 38, 38, 0, hairCol);

  ag::PathFn face = [](ag::Canvas& c) {
    c.moveTo(-27, -24);
    c.bezierCurveTo(-33, -44, 20, -50, 31, -25);
    c.bezierCurveTo(39, -5, 35, 18, 21, 27);
    c.bezierCurveTo(11, 33, -8, 31, -17, 23);
    c.bezierCurveTo(-27, 15, -33, 4, -27, -24);
    c.closePath();
  };
  cv.beginPath();
  face(cv);
  cv.setFillGrad(ag::gRadial(cv, -8, -16, 4, 0, 0, 46,
                             {{0, T.hi}, {0.5, T.b}, {0.85, T.s}, {1, T.d}}));
  cv.fill();
  ag::skClipIn(cv, face, [&]() {
    ag::fSh(cv, -17, 10, 12, 10, "rgba(160,90,72,0.18)", 0.18);
    ag::fSh(cv, 19, 12, 10, 9, "rgba(160,90,72,0.16)", -0.18);
    ag::fHi(cv, -6, -22, 20, 10, "rgba(255,242,230,0.3)", -0.1);
  });

  auto brgb = ag::hexToRgb(g.ch.blushColor);
  char bbuf[96];
  std::snprintf(bbuf, sizeof(bbuf), "rgba(%g,%g,%g,%.17g)", (double)brgb[0],
                (double)brgb[1], (double)brgb[2], 0.18 + E.blush * 0.40);
  cv.setFillColorStr(bbuf);
  cv.beginPath();
  cv.ellipse(-13, -4, 11, 7, 0.15, 0, ag::TAU);
  cv.fill();
  cv.beginPath();
  cv.ellipse(13, -6, 11, 7, -0.15, 0, ag::TAU);
  cv.fill();

  cv.setStrokeColorStr("rgba(150,85,75,0.5)");
  cv.setLineWidth(1.5);
  cv.beginPath();
  cv.moveTo(-1, -12);
  cv.quadraticCurveTo(-3, -6, 0, -4);
  cv.stroke();
  cv.setFillColorStr("rgba(255,245,240,0.5)");
  cv.beginPath();
  cv.arc(0, -5, 1.3, 0, ag::TAU);
  cv.fill();

  double Eo = E.eye, roll = E.rolled;
  for (int si = 0; si < 2; si++) {
    double s = si == 0 ? -1 : 1;
    double ex = s * 12.5, ey = -17;
    double openness = Eo * (1 - roll * 0.5);
    if (openness > 0.10) {
      cv.setLineWidth(2.2);
      cv.setStrokeColorStr("#422428");
      cv.beginPath();
      cv.moveTo(ex - 7, ey + 1);
      cv.quadraticCurveTo(ex, ey - 5 * openness - 1, ex + 7, ey + 1);
      cv.quadraticCurveTo(ex, ey + 4 * openness + 1, ex - 7, ey + 1);
      cv.closePath();
      cv.setFillColorStr("#f8ede8");
      cv.fill();
      cv.stroke();
      cv.setFillColorStr(g.ch.eyeColor);
      cv.beginPath();
      double r0 = 4.0 * (openness > 0.3 ? openness : 0.3);
      cv.arc(ex + roll * 1.5, ey - 1.2 * openness - roll * 2, r0, 0, ag::TAU);
      cv.fill();
      cv.setFillColorStr("#180a0e");
      cv.beginPath();
      double r1 = 2.2 * (openness > 0.3 ? openness : 0.3);
      cv.arc(ex + roll * 1.5, ey - 1.2 * openness - roll * 2, r1, 0, ag::TAU);
      cv.fill();
      cv.setFillColorStr("rgba(255,255,255,0.95)");
      cv.beginPath();
      cv.arc(ex + roll * 1.5 - 1.2, ey - 2.0 * openness - roll * 2 - 1, 1.4, 0,
             ag::TAU);
      cv.fill();
      cv.setFillColorStr("rgba(255,255,255,0.5)");
      cv.beginPath();
      cv.arc(ex + roll * 1.5 + 1.6, ey - 0.6 * openness - roll * 2, 0.7, 0,
             ag::TAU);
      cv.fill();
    } else {
      cv.setLineWidth(2.2);
      cv.setStrokeColorStr("#422428");
      cv.beginPath();
      cv.moveTo(ex - 7, ey + 1);
      cv.quadraticCurveTo(ex, ey + 4.5, ex + 7, ey + 1);
      cv.stroke();
    }
    cv.setStrokeColorStr("#381c20");
    cv.setLineWidth(1.3);
    cv.beginPath();
    cv.moveTo(ex + s * 6, ey + 1);
    cv.lineTo(ex + s * 10, ey - 2.5);
    cv.stroke();
    cv.setStrokeColorStr(hairCol);
    cv.setLineWidth(1.8);
    cv.beginPath();
    cv.moveTo(ex - 7, ey - 8);
    cv.quadraticCurveTo(ex, ey - 11 - E.brow * 4, ex + 7, ey - 7 - E.brow * 5);
    cv.stroke();
  }

  double mo = E.mouth;
  std::string lipCol = g.ch.lipColor;
  if (mo > 0.08) {
    cv.setFillColorStr("#5c1622");
    cv.beginPath();
    cv.ellipse(0, 11, 7.5 + mo * 2.2, 2.5 + mo * 7.5, 0, 0, ag::TAU);
    cv.fill();
    if (mo > 0.22) {
      cv.setFillColorStr("rgba(255,248,245,0.95)");
      cv.beginPath();
      cv.ellipse(0, 8 + mo * 1.2, 5.2, 1.8, 0, 0, ag::TAU);
      cv.fill();
    }
    cv.setFillColorStr("rgba(215,95,115,0.95)");
    cv.beginPath();
    cv.ellipse(0, 12 + mo * 3.5, 5.6, 2.4, 0, 0, ag::TAU);
    cv.fill();
  } else {
    cv.setStrokeColorStr(lipCol);
    cv.setLineWidth(2.2);
    cv.beginPath();
    cv.moveTo(-7, 10);
    cv.quadraticCurveTo(0, 12, 7, 10);
    cv.stroke();
    cv.setFillColorStr(lipCol);
    cv.beginPath();
    cv.ellipse(0, 12, 5.0, 2.2, 0, 0, ag::TAU);
    cv.fill();
  }
  cv.setFillColorStr("rgba(255,250,250,0.5)");
  cv.beginPath();
  cv.ellipse(2, 12.5 + mo * 3, 2.4, 1.1, 0, 0, ag::TAU);
  cv.fill();

  side_tressS(cv, -28, -24, -12, -44, 12, -44, 28, -20, 11, 3, hairCol,
              "rgba(255,205,215,0.22)");
  side_tressS(cv, -30, -16, -38, -2, -38, 10, -32, 22, 7, 2, hairCol);
  side_tressS(cv, 30, -18, 38, -4, 38, 8, 34, 20, 7, 2, hairCol);
  cv.restore();
}

// ---- js/side.js:233-238 drawBreast ----
static void side_drawBreast(ag::Canvas& cv, ag::Game& g, const side_Expr&,
                            double br) {
  double jig = g.breast.p * 0.95;
  double bsz = 0.72 + g.ch.breastSize * 0.62;
  ag::V2 p = ag::sp(g, 452, 489 + br, 0.035);
  ag::BreastOpt opt;
  opt.jig = jig;
  ag::breastS(cv, g, p.x, p.y, 30 * bsz, 1.35, side_herT(g), opt);
}

// ---- js/side.js:243-332 drawHim ----
static void side_drawHim(ag::Canvas& cv, ag::Game& g, const side_Expr&) {
  double d = g.depth;
  ag::Tone T = side_himT(g);
  ag::Tone FT = side_himFarT(g);

  ag::V2 pel = ag::hp(g, 712, 468, 1.0);
  ag::V2 mid = ag::hp(g, 624, 428, 0.6);
  ag::V2 shld = ag::hp(g, 540, 404, 0.25);
  ag::V2 head = ag::hp(g, 472, 388, 0.2);

  ag::LimbOpt leg;
  leg.belly = 1.12;
  leg.aoA = 0.3;
  ag::limbS(cv, ag::V2{pel.x + 16, pel.y + 10}, ag::V2{830, 560}, 26, 16, FT,
            leg);
  ag::LimbOpt leg2;
  leg2.belly = 1.2;
  ag::limbS(cv, ag::V2{830, 560}, ag::V2{912, 568}, 15, 9, FT, leg2);
  ag::footS(cv, 918, 570, 0.3, 0.95, FT);

  ag::LimbOpt arm;
  arm.belly = 1.08;
  arm.aoA = 0.3;
  ag::limbS(cv, ag::V2{shld.x + 10, shld.y + 6}, ag::V2{536, 466}, 14, 10.5,
            FT, arm);
  ag::LimbOpt arm2;
  arm2.belly = 1.04;
  ag::limbS(cv, ag::V2{536, 466}, ag::V2{556, 502}, 10.5, 7.5, FT, arm2);
  ag::HandOpt fh;
  fh.curl = 0.45;
  ag::handS(cv, g, 558, 506, 1.7, 0.95, FT, fh);

  ag::PathFn torso = [=](ag::Canvas& c) {
    c.moveTo(pel.x + 12, pel.y - 16);
    c.bezierCurveTo(mid.x + 16, mid.y - 22, shld.x + 22, shld.y - 20,
                    shld.x + 4, shld.y - 14);
    c.bezierCurveTo(shld.x - 16, shld.y - 8, shld.x - 20, shld.y + 8,
                    shld.x - 12, shld.y + 18);
    c.bezierCurveTo(mid.x - 20, mid.y + 26, pel.x - 22, pel.y + 24, pel.x - 12,
                    pel.y + 22);
    c.bezierCurveTo(pel.x + 6, pel.y + 20, pel.x + 16, pel.y + 2, pel.x + 12,
                    pel.y - 16);
    c.closePath();
  };
  double lg[4] = {shld.x, shld.y - 22, pel.x, pel.y + 26};
  ag::skFillShape(cv, torso, T, lg);
  ag::skClipIn(cv, torso, [&]() {
    cv.save();
    cv.setCompStr("multiply");
    cv.setStrokeColorStr("rgba(80,38,20,0.4)");
    cv.setLineWidth(2.6);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(shld.x + 4, shld.y - 6);
    cv.quadraticCurveTo(mid.x + 8, mid.y - 4, pel.x + 4, pel.y - 8);
    cv.stroke();
    cv.restore();
    ag::fSh(cv, mid.x, mid.y + 16, 46, 11, "rgba(90,44,24,0.3)", 0.35);
    ag::fHi(cv, mid.x + 6, mid.y - 12, 50, 9, "rgba(255,225,190,0.26)", 0.35);
    ag::fAO(cv, shld.x, shld.y, 20, 14, 0.3);
  });
  ag::skLine(cv, torso, T, 1.4, 0.3);
  ag::fAO(cv, pel.x - 6, pel.y + 26, 40, 12, 0.34, 0.3);

  ag::LimbOpt nleg;
  nleg.belly = 1.14;
  nleg.aoA = 0.32;
  ag::limbS(cv, ag::V2{pel.x + 16, pel.y + 12}, ag::V2{800, 556}, 28, 18, T,
            nleg);
  ag::LimbOpt nleg2;
  nleg2.belly = 1.2;
  ag::limbS(cv, ag::V2{800, 556}, ag::V2{884, 564}, 17, 10, T, nleg2);
  ag::footS(cv, 890, 566, 0.28, 1.0, T);

  side_neckS(cv, ag::V2{head.x + 8, head.y + 12},
             ag::V2{shld.x + 2, shld.y - 4}, 15, T);
  cv.save();
  cv.translate(head.x, head.y);
  cv.rotate(0.45 + d * 0.06);
  ag::PathFn skull = [](ag::Canvas& c) {
    c.moveTo(-21, -21);
    c.bezierCurveTo(-27, -37, 27, -39, 29, -17);
    c.bezierCurveTo(31, -2, 27, 13, 15, 21);
    c.bezierCurveTo(2, 27, -15, 25, -21, 17);
    c.bezierCurveTo(-29, 8, -27, -9, -21, -21);
    c.closePath();
  };
  cv.beginPath();
  skull(cv);
  cv.setFillGrad(ag::gRadial(cv, -8, -16, 4, 0, 0, 40,
                             {{0, T.hi}, {0.55, T.b}, {1, T.s}}));
  cv.fill();
  cv.beginPath();
  cv.moveTo(-13, 17);
  cv.bezierCurveTo(-25, 25, -34, 17, -34, 6);
  cv.bezierCurveTo(-38, -2, -27, -9, -21, -11);
  cv.closePath();
  cv.fill();
  ag::skClipIn(cv, skull, [&]() {
    ag::fSh(cv, -6, -9, 19, 6, "rgba(100,55,30,0.3)", -0.15);
    ag::fHi(cv, -4, -21, 15, 8, "rgba(255,225,190,0.24)");
  });
  cv.setStrokeColorStr("rgba(60,30,20,0.75)");
  cv.setLineWidth(1.8);
  cv.beginPath();
  cv.moveTo(-16, -8);
  cv.quadraticCurveTo(-10, -6, -5, -8);
  cv.stroke();
  cv.setStrokeColorStr("rgba(120,65,35,0.5)");
  cv.setLineWidth(1.4);
  cv.beginPath();
  cv.moveTo(-14, -2);
  cv.quadraticCurveTo(-18, 6, -15, 11);
  cv.stroke();
  cv.setStrokeColorStr("rgba(120,50,45,0.6)");
  cv.setLineWidth(1.6);
  cv.beginPath();
  cv.moveTo(-16, 15);
  cv.quadraticCurveTo(-11, 16, -7, 15);
  cv.stroke();
  side_hairMassS(cv, 3, -20, 26, 16, -0.15, "#1c100a");
  side_tressS(cv, -14, -28, -6, -36, 6, -36, 14, -26, 8, 2, "#1c100a",
              "rgba(255,210,170,0.16)");
  side_tressS(cv, -22, -16, -28, -6, -28, 4, -24, 10, 6, 2, "#1c100a");
  side_tressS(cv, -20, -26, -26, -18, -28, -10, -26, -4, 6, 2, "#1c100a");
  cv.setFillColorStr(T.s);
  cv.beginPath();
  cv.ellipse(8, 4, 5.5, 7.5, 0.1, 0, ag::TAU);
  cv.fill();
  cv.setStrokeColorStr("rgba(95,50,28,0.4)");
  cv.setLineWidth(1.0);
  cv.beginPath();
  cv.moveTo(8, -1);
  cv.quadraticCurveTo(11, 4, 8, 9);
  cv.stroke();
  cv.restore();

  ag::LimbOpt nar;
  nar.belly = 1.1;
  nar.aoA = 0.32;
  ag::limbS(cv, shld, ag::V2{566, 462}, 16, 11.5, T, nar);
  ag::LimbOpt nar2;
  nar2.belly = 1.06;
  ag::limbS(cv, ag::V2{566, 462}, ag::V2{602, 498}, 11.5, 8, T, nar2);
  ag::HandOpt nh;
  nh.curl = 0.45;
  nh.spread = 0.4;
  ag::handS(cv, g, 606, 502, 1.9, 1.0, T, nh);
}

// ---- js/side.js:337-355 drawHerNear ----
static void side_drawHerNear(ag::Canvas& cv, ag::Game& g, const side_Expr&,
                             double wrap) {
  ag::Tone T = side_herT(g);
  ag::V2 hip = ag::sp(g, 626, 522, 0.07);
  ag::V2 K{680 + wrap * 6, 532 + wrap * 4};
  ag::V2 A{722 + wrap * 8, 548 + wrap * 4};

  ag::fAO(cv, ag::lerp(hip.x, K.x, 0.5), 548, 46, 9, 0.35);
  ag::LimbOpt t1;
  t1.belly = 1.14;
  t1.aoA = 0.3;
  ag::limbS(cv, hip, K, 26, 16, T, t1);
  ag::fHi(cv, K.x + 2, K.y - 6, 9, 11, "rgba(255,240,225,0.34)", 0.2);
  ag::LimbOpt t2;
  t2.belly = 1.2;
  ag::limbS(cv, K, A, 14.5, 8, T, t2);
  ag::footS(cv, A.x + 4, A.y + 2, 0.32 - wrap * 0.25, 0.95, T);

  ag::V2 sh = ag::sp(g, 396, 512, 0.02);
  ag::V2 el = ag::sp(g, 452, 470, 0.05);
  ag::V2 wr = ag::sp(g, 524, 436, 0.09);
  ag::LimbOpt a1;
  a1.belly = 1.06;
  a1.aoA = 0.3;
  ag::limbS(cv, sh, el, 12.5, 9.5, T, a1);
  ag::LimbOpt a2;
  a2.belly = 1.03;
  ag::limbS(cv, el, wr, 9.5, 6.5, T, a2);
  ag::HandOpt h;
  h.curl = 0.5;
  ag::handS(cv, g, wr.x, wr.y, -0.7, 0.92, T, h);
}

// ---- js/side.js:360-364 drawVulva ----
static void side_drawVulva(ag::Canvas& cv, ag::Game& g) {
  double eng = ag::clamp(g.ar / 100.0, 0.0, 1.0);
  double open = 3.5 + 9.5 * g.depth;
  ag::VulvaOpt opt;
  opt.view = "side";
  ag::vulvaS(cv, 652, 498, open, eng, side_herT(g), opt);
}

// ---- js/side.js:366-390 drawShaft ----
static void side_drawShaft(ag::Canvas& cv, ag::Game& g) {
  ag::V2 A = ag::hp(g, 700, 484, 1.0);
  ag::V2 V{653, 500};
  double ang = std::atan2(V.y - A.y, V.x - A.x);
  double pu = 1 + 0.36 * g.shaftPulse;
  double d = g.depth;
  ag::Tone T = side_himT(g);
  ag::LimbOpt lo;
  lo.belly = 1.02;
  lo.line = false;
  lo.hasLit = true;
  lo.litX = std::cos(ang - ag::TAU / 4);
  lo.litY = std::sin(ang - ag::TAU / 4);
  ag::limbS(cv, A, V, 12 * pu, 12.5 * pu, T, lo);
  cv.setStrokeColorStr("rgba(142,72,56,0.35)");
  cv.setLineWidth(2.2);
  cv.setLineCapStr("round");
  cv.beginPath();
  cv.moveTo(A.x - 4, A.y - 8);
  cv.quadraticCurveTo(ag::lerp(A.x, V.x, 0.5) - 2, ag::lerp(A.y, V.y, 0.5) - 7,
                      V.x - 6, V.y - 5);
  cv.stroke();
  if (d < 0.46) {
    ag::V2 gl{V.x + std::cos(ang) * 7, V.y + std::sin(ang) * 7};
    cv.setFillColorStr(side_skLight(T.b, 0.18));
    cv.beginPath();
    cv.ellipse(gl.x, gl.y, 9.8 * pu, 11.5 * pu, ang + 0.28, 0, ag::TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,250,245,0.4)");
    cv.beginPath();
    cv.arc(gl.x - 2, gl.y - 3, 2.4, 0, ag::TAU);
    cv.fill();
  }
  if (g.ar > 26) {
    cv.save();
    cv.setCompStr("screen");
    cv.setStrokeColorStr("rgba(255,255,255,0.32)");
    cv.setLineWidth(3.0);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(A.x, A.y - 8);
    cv.lineTo(V.x + 4, V.y - 9);
    cv.stroke();
    cv.restore();
  }
}

// ---- js/side.js:395-412 drawRubHand / drawRubFX ----
static void side_drawRubHand(ag::Canvas& cv, ag::Game& g, double hx, double hy,
                             double /*ph*/, bool small) {
  ag::Tone T = side_himT(g);
  ag::V2 sh = ag::hp(g, 500, 424, 0.2);
  ag::V2 el{(sh.x + hx) / 2 + 30, (sh.y + hy) / 2 - 22};
  ag::LimbOpt o1;
  o1.belly = 1.08;
  o1.aoA = 0.3;
  ag::limbS(cv, sh, el, 20, 15, T, o1);
  ag::LimbOpt o2;
  o2.belly = 1.05;
  ag::limbS(cv, el, ag::V2{hx, hy}, small ? 13 : 16, small ? 10 : 12, T, o2);
  ag::HandOpt h;
  h.curl = 0.42;
  ag::handS(cv, g, hx, hy, -0.6, small ? 0.8 : 1.0, T, h);
}

static void side_drawRubFX(ag::Canvas& cv, ag::Game& g, double br) {
  double rub = g.rub;
  if (rub < 0.03 || g.state == "climax" || g.state == "finish") return;
  int zone = g.rubZone;
  ag::V2 p = ag::sp(g, 448, 492 + br, 0.035);
  double hx = zone == 3 ? 652 : p.x + std::sin(g.t * 9) * 8 * rub;
  double hy = zone == 3 ? 496 : p.y + std::sin(g.t * 18) * 4 * rub;
  side_drawRubHand(cv, g, hx, hy, 0, zone == 3);
}

} // anonymous namespace (file-local side_ helpers)

namespace ag::side {

// js/app.js:17-30 side branch + js/app.js:42-53 drawSoloSide, routed here.
// NOTE: drawRoom/drawBed already ported (ag_gfx); drawFluids/drawLight stay
// with the app pass (ag_port_todo app::draw) and are NOT emitted here.
// TODO(app): route pos!=0 to poses::drawBodies and oral>0.03 to oral::draw;
//   this file renders the missionary couple + solo showcase only.
void draw(Canvas& cv, Game& g) {
  drawRoom(cv, g);
  drawBed(cv, g);
  if (g.state == "intro") return;
  if (g.solo) {
    // js/app.js drawSoloSide: gentle sway + roll, him/shaft skipped.
    double t = g.t;
    cv.save();
    cv.translate(std::sin(t * 1.1) * 10, -std::fabs(std::sin(t * 1.1)) * 6);
    cv.translate(640, 360);
    cv.rotate(std::sin(t * 1.1) * 0.012);
    cv.translate(-640, -360);
    side_HerOut out = side_drawHer(cv, g);
    side_drawHerHead(cv, g, out.E);
    side_drawBreast(cv, g, out.E, out.br);
    side_drawHerNear(cv, g, out.E, out.wrap);
    side_drawRubFX(cv, g, out.br);
    cv.restore();
    return;
  }
  side_HerOut out = side_drawHer(cv, g);
  side_drawVulva(cv, g);
  side_drawShaft(cv, g);
  side_drawHerHead(cv, g, out.E);
  side_drawBreast(cv, g, out.E, out.br);
  side_drawHim(cv, g, out.E);
  side_drawHerNear(cv, g, out.E, out.wrap);
  side_drawRubFX(cv, g, out.br);
}

// js/side.js:7-36 herExpression — autonomic face state shared with 3D.
// Header keeps void(Canvas&,Game&) shape; the computed state drives the 2D
// head above and the 3D face rig polls the same math via side_calcExpr.
// TODO(header): expose SideExpr return (e.g. Expr herExpression(const Game&))
//   so 3D can consume it without recomputation; current void form only
//   exercises the math for parity.
void herExpression(Canvas& cv, Game& g) {
  (void)cv;
  side_Expr e = side_calcExpr(g);
  (void)e;
}

} // namespace ag::side
