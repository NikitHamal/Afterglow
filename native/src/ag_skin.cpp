// Afterglow native — ag_skin.cpp (tone + limbS ported 1:1).
#include "ag_skin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ag {

Tone skTone(std::string_view base) {
  Tone T;
  T.base = std::string(base);
  T.hi = lerpHex(base, "#fff4ea", 0.42);
  T.b = std::string(base);
  T.s = lerpHex(base, "#3a1410", 0.26);
  T.d = lerpHex(base, "#3a1410", 0.5);
  T.dk = T.d;
  return T;
}
Tone herT(const Game& g, const SkinPair& sk) {
  Tone T = skTone(sk.her);
  T.base = sk.her;
  T.sh = sk.herSh;
  T.dk = sk.herDk;
  (void)g;
  return T;
}
Tone himT(const Game& g, const SkinPair& sk) {
  Tone T = skTone(sk.him);
  T.base = sk.him;
  T.sh = sk.himSh;
  T.dk = sk.himSh;
  (void)g;
  return T;
}

void skFillShape(Canvas& cv, PathFn fn, const Tone& T, const F64 lg[4]) {
  cv.beginPath();
  fn(cv);
  cv.setFillGrad(gLinear(cv, lg[0], lg[1], lg[2], lg[3],
                         {{0, T.hi}, {0.38, T.b}, {0.78, T.s}, {1, T.d}}));
  cv.fill();
}
void skFillRad(Canvas& cv, PathFn fn, const Tone& T, F64 cx, F64 cy, F64 r0,
               F64 r1) {
  cv.beginPath();
  fn(cv);
  cv.setFillGrad(gRadial(cv, cx, cy, r0, cx, cy, r1,
                         {{0, T.hi}, {0.45, T.b}, {0.82, T.s}, {1, T.d}}));
  cv.fill();
}
void skClipIn(Canvas& cv, PathFn fn, std::function<void()> inner) {
  cv.save();
  cv.beginPath();
  fn(cv);
  cv.clip();
  inner();
  cv.restore();
}
void fSh(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
         F64 rot) {
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, x, y, rx, ry, col, rot);
  cv.restore();
}
void fHi(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
         F64 rot) {
  cv.save();
  cv.setCompStr("screen");
  shade(cv, x, y, rx, ry, col, rot);
  cv.restore();
}
void fSSS(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, F64 rot) {
  cv.save();
  cv.setCompStr("soft-light");
  shade(cv, x, y, rx, ry, "rgba(255,120,90,0.40)", rot);
  cv.restore();
}
void fAO(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, F64 a, F64 rot) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(30,10,12,%.17g)", a);
  fSh(cv, x, y, rx, ry, buf, rot);
}
void skLine(Canvas& cv, PathFn fn, const Tone& T, F64 w, F64 a) {
  cv.save();
  cv.beginPath();
  fn(cv);
  auto rgb = hexToRgb(T.dk);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", rgb[0], rgb[1],
                rgb[2], a);
  cv.setStrokeColorStr(buf);
  cv.setLineWidth(w);
  cv.stroke();
  cv.restore();
}

void limbS(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, const Tone& T,
           const LimbOpt& opt) {
  if (!std::isfinite(a.x) || !std::isfinite(a.y)) return;
  F64 dx = b.x - a.x, dy = b.y - a.y, len = std::hypot(dx, dy);
  if (len < 0.001) return;
  F64 ux = dx / len, uy = dy / len;
  F64 px = -uy, py = ux;
  F64 bow = opt.bow;
  F64 mx = (a.x + b.x) / 2 + px * bow, my = (a.y + b.y) / 2 + py * bow;
  F64 rm = ((r1 + r2) / 2) * opt.belly;
  PathFn path = [=](Canvas& c) {
    c.moveTo(a.x + px * r1, a.y + py * r1);
    c.quadraticCurveTo(mx + px * rm, my + py * rm, b.x + px * r2,
                       b.y + py * r2);
    c.quadraticCurveTo(b.x + ux * r2 * 1.32, b.y + uy * r2 * 1.32,
                       b.x - px * r2, b.y - py * r2);
    c.quadraticCurveTo(mx - px * rm, my - py * rm, a.x - px * r1,
                       a.y - py * r1);
    c.quadraticCurveTo(a.x - ux * r1 * 1.32, a.y - uy * r1 * 1.32,
                       a.x + px * r1, a.y + py * r1);
    c.closePath();
  };
  F64 R = std::max(r1, r2) * 1.15;
  F64 cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
  F64 litX = opt.hasLit ? opt.litX : -0.62;
  F64 litY = opt.hasLit ? opt.litY : -0.78;
  F64 lg[4] = {cx + litX * R, cy + litY * R, cx - litX * R, cy - litY * R};
  skFillShape(cv, path, T, lg);
  F64 ang = std::atan2(uy, ux);
  skClipIn(cv, path, [&]() {
    fHi(cv, cx + litX * rm * 0.5, cy + litY * rm * 0.5, len * 0.42, rm * 0.5,
        "rgba(255,240,225,0.30)", ang);
    fSSS(cv, cx - litX * rm * 0.35, cy - litY * rm * 0.35, len * 0.4, rm * 0.6,
         ang);
    fSh(cv, cx - litX * rm * 0.85, cy - litY * rm * 0.85, len * 0.5, rm * 0.62,
        "rgba(70,26,22,0.42)", ang);
    if (opt.aoA) fAO(cv, a.x, a.y, r1 * 1.25, r1 * 1.25, opt.aoA);
    if (opt.aoB) fAO(cv, b.x, b.y, r2 * 1.25, r2 * 1.25, opt.aoB);
  });
  if (opt.line) skLine(cv, path, T, 1.3, 0.32);
}

// js/skin.js:101 — hand: palm + four articulated fingers + thumb.
void handS(Canvas& cv, Game& g, F64 x, F64 y, F64 ang, F64 s, const Tone& T,
           const HandOpt& opt) {
  (void)g;
  constexpr F64 PI = 3.14159265358979323846;
  F64 curl = opt.curl, spread = opt.spread;
  cv.save();
  cv.translate(x, y);
  cv.rotate(ang);
  cv.scale(s, s);
  PathFn palm = [=](Canvas& c) {
    c.moveTo(-9, -8);
    c.quadraticCurveTo(0, -11, 9, -8);
    c.quadraticCurveTo(12, 0, 9, 8);
    c.quadraticCurveTo(0, 12, -9, 8);
    c.quadraticCurveTo(-12, 0, -9, -8);
    c.closePath();
  };
  F64 palmLg[4] = {-10, -10, 10, 10};
  skFillShape(cv, palm, T, palmLg);
  skClipIn(cv, palm, [&]() {
    fHi(cv, -2, -3, 7, 5, "rgba(255,240,225,0.30)");
    fAO(cv, 0, 8, 9, 4, 0.3);
  });
  for (int i = 0; i < 4; i++) {
    F64 kx = -7 + i * 4.6, ky = -8;
    F64 fa = -PI / 2 + (i - 1.5) * spread * 0.5;
    F64 L = (i == 0 || i == 3) ? 13 : 16;
    F64 bend = curl * 1.1;
    V2 k0{kx, ky};
    V2 j1{kx + std::cos(fa) * L * 0.5, ky + std::sin(fa) * L * 0.5};
    V2 j2{kx + std::cos(fa + bend * 0.5) * L,
          ky + std::sin(fa + bend * 0.5) * L + curl * 5};
    LimbOpt fb;
    fb.line = false;
    fb.belly = 1.0;
    limbS(cv, k0, j1, 2.6, 2.2, T, fb);
    limbS(cv, j1, j2, 2.2, 1.6, T, fb);
    cv.setFillColorStr("rgba(255,235,225,0.5)"); // nail
    cv.beginPath();
    cv.ellipse(j2.x, j2.y + 0.4, 1.3, 1.0, fa, 0, TAU);
    cv.fill();
  }
  F64 ta = PI * 0.78;
  V2 tb{-8, 2};
  V2 t1{-8 + std::cos(ta) * 6, 2 + std::sin(ta) * 6};
  V2 t2{-8 + std::cos(ta - curl) * 12, 2 + std::sin(ta - curl) * 12};
  LimbOpt tl;
  tl.line = false;
  limbS(cv, tb, t1, 3.2, 2.6, T, tl);
  limbS(cv, t1, t2, 2.6, 2.0, T, tl);
  cv.setFillColorStr("rgba(120,60,50,0.20)"); // knuckle hints
  for (int i = 0; i < 4; i++) {
    cv.beginPath();
    cv.arc(-7 + i * 4.6, -7.4, 0.9, 0, TAU);
    cv.fill();
  }
  skLine(cv, palm, T, 1.1, 0.3);
  cv.restore();
}

// js/skin.js:143 — foot: heel, arch, forefoot, toes.
void footS(Canvas& cv, F64 x, F64 y, F64 ang, F64 s, const Tone& T) {
  cv.save();
  cv.translate(x, y);
  cv.rotate(ang);
  cv.scale(s, s);
  PathFn path = [=](Canvas& c) {
    c.moveTo(-12, -4);
    c.quadraticCurveTo(-2, -8, 8, -5);
    c.quadraticCurveTo(16, -3, 17, 1);
    c.quadraticCurveTo(15, 6, 6, 6);
    c.quadraticCurveTo(-6, 7, -12, 4);
    c.quadraticCurveTo(-15, 0, -12, -4);
    c.closePath();
  };
  F64 lg[4] = {-12, -8, 12, 6};
  skFillShape(cv, path, T, lg);
  skClipIn(cv, path, [&]() {
    fHi(cv, 2, -3, 9, 3, "rgba(255,240,225,0.3)");
    fAO(cv, -9, 3, 5, 3, 0.3);
  });
  for (int i = 0; i < 5; i++) {
    F64 tx = 15 - i * 1.2, ty = 2 + i * 1.1;
    cv.setFillColorStr(T.b);
    cv.beginPath();
    cv.arc(tx, ty, 2.2 - i * 0.3, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(255,235,228,0.7)");
    cv.beginPath();
    cv.ellipse(tx + 0.3, ty - 0.5, 0.9, 0.6, 0.2, 0, TAU);
    cv.fill();
  }
  skLine(cv, path, T, 1.2, 0.3);
  cv.restore();
}

// js/skin.js:165 — breast: integrated teardrop with gravity, areola, specular.
void breastS(Canvas& cv, Game& g, F64 cx, F64 cy, F64 r, F64 ang,
             const Tone& T, const BreastOpt& opt) {
  F64 ar = clamp(g.ar / 100, 0.0, 1.0);
  F64 jig = opt.jig;
  cv.save();
  cv.translate(cx, cy + jig);
  cv.rotate(ang);
  PathFn path = [=](Canvas& c) {
    // teardrop: chest-wall attach (right) sweeping out to apex (left)
    c.moveTo(r * 0.9, -r * 0.85);
    c.bezierCurveTo(-r * 0.2, -r * 1.05, -r * 1.25, -r * 0.5, -r * 1.15,
                    r * 0.15);
    c.bezierCurveTo(-r * 1.05, r * 0.85, -r * 0.2, r * 1.05, r * 0.9, r * 0.8);
    c.bezierCurveTo(r * 1.15, r * 0.3, r * 1.15, -r * 0.3, r * 0.9, -r * 0.85);
    c.closePath();
  };
  // radial form light from upper-outer (custom stops, not skFillRad)
  cv.beginPath();
  path(cv);
  cv.setFillGrad(gRadial(cv, -r * 0.45, -r * 0.5, r * 0.1, 0, 0, r * 1.5,
                         {{0, T.hi}, {0.42, T.b}, {0.8, T.s}, {1, T.d}}));
  cv.fill();
  skClipIn(cv, path, [&]() {
    fHi(cv, -r * 0.42, -r * 0.48, r * 0.5, r * 0.36,
        "rgba(255,244,232,0.42)", -0.5); // crest specular
    fSh(cv, r * 0.1, r * 0.85, r * 0.95, r * 0.4, "rgba(90,35,30,0.5)",
        0.12); // under-breast shadow
    fSSS(cv, -r * 0.9, r * 0.35, r * 0.5, r * 0.4, 0.6); // rim SSS
    fAO(cv, r * 0.85, 0, r * 0.4, r * 0.9, 0.35); // chest-wall contact
  });
  // areola + nipple on the apex, squashed by viewing angle
  F64 aer = r * (0.34 + 0.06 * ar);
  cv.save();
  cv.translate(-r * 0.92, r * 0.02);
  cv.rotate(0.1);
  {
    auto rgb = hexToRgb(g.ch.nippleColor);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "rgba(%g,%g,%g,%.17g)", rgb[0], rgb[1],
                  rgb[2], 0.55);
    cv.setFillColorStr(buf);
  }
  cv.beginPath();
  cv.ellipse(0, 0, aer * 0.62, aer, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr(g.ch.nippleColor);
  cv.beginPath();
  cv.ellipse(-aer * 0.15, 0, aer * 0.3, aer * 0.42 + ar * 1.5, 0, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,255,255,0.5)");
  cv.beginPath();
  cv.arc(-aer * 0.3, -aer * 0.25, aer * 0.14, 0, TAU);
  cv.fill();
  cv.restore();
  skLine(cv, path, T, 1.3, 0.3);
  cv.restore();
}

// js/skin.js:206 — vulva: soft mons + labia folds (filled, not outlines).
// Game-carried state (G.ar / G.char) arrives via VulvaOpt (see ag_skin.h).
void vulvaS(Canvas& cv, F64 cx, F64 cy, F64 open, F64 eng, const Tone& T,
            const VulvaOpt& opt) {
  constexpr F64 PI = 3.14159265358979323846;
  bool front = (opt.view == "front");
  PathFn mons = [=](Canvas& c) { c.ellipse(cx, cy - 4, 15, 13, 0, 0, TAU); };
  cv.save();
  cv.beginPath();
  mons(cv);
  cv.setFillGrad(gRadial(cv, cx - 4, cy - 9, 2, cx, cy - 4, 18,
                         {{0, T.hi}, {0.55, T.b}, {1, T.s}}));
  cv.fill();
  cv.restore();

  char buf[64];
  if (front) {
    // outer labia as two soft filled folds
    for (int k = 0; k < 2; k++) {
      F64 s = k == 0 ? -1 : 1;
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
    // inner minora roseate
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)",
                  (int)(205 + 30 * eng), (int)(95 + 20 * eng),
                  (int)(110 + 20 * eng), 0.9);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(cx, cy + 1, 2.6 + open * 0.25, 8 + open * 0.5, 0, 0, TAU);
    cv.fill();
    // introitus
    cv.setFillColorStr("rgba(70,20,30,0.9)");
    cv.beginPath();
    cv.ellipse(cx, cy + 3, 2.2, open * 0.6, 0, 0, TAU);
    cv.fill();
    // clitoral hood + glans
    cv.setFillColorStr(T.b);
    cv.beginPath();
    cv.ellipse(cx, cy - 10, 4, 3.2, 0, PI, 0);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)",
                  (int)(215 + 25 * eng), 105, 118, 0.95);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(cx, cy - 9, 1.4 + 1.4 * eng, 0, TAU);
    cv.fill();
  } else {
    // side: single cleft with swollen labia edge
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)",
                  (int)(185 + 25 * eng), 105, 100, 0.8);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(3.4);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(cx + 1, cy - 12);
    cv.quadraticCurveTo(cx + 5, cy, cx + 1, cy + 11);
    cv.stroke();
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)",
                  (int)(205 + 30 * eng), 100, 112, 0.85);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(cx + 2, cy, 2.4, 8 + open * 0.4, 0.1, 0, TAU);
    cv.fill();
    cv.setFillColorStr("rgba(70,20,30,0.85)");
    cv.beginPath();
    cv.ellipse(cx + 2.5, cy + 2, 2.0, open * 0.55, 0.1, 0, TAU);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)",
                  (int)(215 + 25 * eng), 105, 118, 0.95);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(cx + 1, cy - 10, 1.4 + 1.4 * eng, 0, TAU);
    cv.fill();
  }
  // wet specular
  if (opt.ar > 22) {
    cv.save();
    cv.setCompStr("screen");
    std::snprintf(buf, sizeof(buf), "rgba(255,255,255,%.17g)",
                  0.25 + 0.3 * eng);
    cv.setStrokeColorStr(buf);
    cv.setLineWidth(1.8);
    cv.setLineCapStr("round");
    cv.beginPath();
    cv.moveTo(cx + (front ? 3 : 4), cy - 8);
    cv.quadraticCurveTo(cx + (front ? 5 : 6), cy, cx + (front ? 3 : 4), cy + 8);
    cv.stroke();
    cv.restore();
  }
  // pubic hair (kept per customization) — soft dark tresses over mons
  const std::string& ph = opt.pubicHair;
  if (ph != "bare") {
    bool full = (ph == "full");
    int n = full ? 9 : 5;
    std::string hc = lerpHex(opt.hairColor, "#3a1410", 0.1); // skDark 0.1
    cv.save();
    cv.setLineCapStr("round");
    for (int i = 0; i < n; i++) {
      F64 a0 = PI * (0.2 + (F64)i / (n - 1) * 0.6);
      F64 r0 = 7, r1 = full ? 16 : 12;
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

// js/skin.js:279 — neck + trapezius: connects skull to shoulder line.
void neckS(Canvas& cv, V2 top, V2 base, F64 w, const Tone& T,
           const NeckOpt& opt) {
  F64 dx = base.x - top.x, dy = base.y - top.y;
  F64 L = std::hypot(dx, dy);
  if (!(L > 0)) L = 1; // js: Math.hypot(...) || 1
  F64 nx = -dy / L, ny = dx / L;
  PathFn path = [=](Canvas& c) {
    c.moveTo(top.x + nx * w * 0.62, top.y + ny * w * 0.62);
    // trapezius flare out to shoulder at base
    c.quadraticCurveTo(base.x + nx * w * 0.7, base.y + ny * w * 0.7 - 4,
                       base.x + nx * w * 1.5, base.y + ny * w * 1.5);
    c.lineTo(base.x - nx * w * 1.5, base.y - ny * w * 1.5);
    c.quadraticCurveTo(base.x - nx * w * 0.7, base.y - ny * w * 0.7 - 4,
                       top.x - nx * w * 0.62, top.y - ny * w * 0.62);
    c.closePath();
  };
  F64 lg[4] = {top.x + nx * w, top.y + ny * w, top.x - nx * w, top.y - ny * w};
  skFillShape(cv, path, T, lg);
  skClipIn(cv, path, [&]() {
    // sternocleidomastoid groove
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
    fAO(cv, top.x, top.y + 2, w * 0.9, w * 0.5, 0.35); // under-jaw shadow
  });
  if (opt.line) skLine(cv, path, T, 1.2, 0.3);
}

// js/skin.js:308 — hair: silky tapered ribbon tress.
void tressS(Canvas& cv, F64 x0, F64 y0, F64 c1x, F64 c1y, F64 c2x, F64 c2y,
            F64 x1, F64 y1, F64 w0, F64 w1, std::string_view col,
            std::string_view sheen) {
  F64 px[4] = {x0, c1x, c2x, x1};
  F64 py[4] = {y0, c1y, c2y, y1};
  auto off = [&](F64 t, F64 w, F64& ox, F64& oy) {
    // approximate normal along cubic at t
    F64 mt = 1 - t;
    F64 bx = mt * mt * mt * px[0] + 3 * mt * mt * t * px[1] +
             3 * mt * t * t * px[2] + t * t * t * px[3];
    F64 by = mt * mt * mt * py[0] + 3 * mt * mt * t * py[1] +
             3 * mt * t * t * py[2] + t * t * t * py[3];
    F64 dx = 3 * mt * mt * (px[1] - px[0]) + 6 * mt * t * (px[2] - px[1]) +
             3 * t * t * (px[3] - px[2]);
    F64 dy = 3 * mt * mt * (py[1] - py[0]) + 6 * mt * t * (py[2] - py[1]) +
             3 * t * t * (py[3] - py[2]);
    F64 len = std::hypot(dx, dy);
    if (!(len > 0)) len = 1;
    ox = bx - dy / len * w;
    oy = by + dx / len * w;
  };
  F64 ox, oy;
  cv.beginPath();
  off(0, lerp(w0, w1, 0), ox, oy);
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

// js/skin.js:335 — rounded scalp / hair mass with soft top light.
void hairMassS(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 rot,
               std::string_view col) {
  std::string hi = lerpHex(col, "#fff4ea", 0.22); // skLight 0.22
  std::string dk = lerpHex(col, "#3a1410", 0.4);  // skDark 0.4
  cv.beginPath();
  cv.ellipse(cx, cy, rx, ry, rot, 0, TAU);
  cv.setFillGrad(gRadial(cv, cx - rx * 0.3, cy - ry * 0.5, rx * 0.1, cx, cy,
                         rx * 1.2,
                         {{0, hi}, {0.55, std::string(col)}, {1, dk}}));
  cv.fill();
}

} // namespace ag
