// Afterglow native — ag_skin.cpp (tone + limbS ported 1:1).
#include "ag_skin.h"

#include <cmath>
#include <cstdio>

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

} // namespace ag
