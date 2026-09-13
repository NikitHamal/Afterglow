// Afterglow native — ag_gfx.cpp
// 1:1 port of js/gfx.js. Numeric order of operations is preserved so frames
// match the web build up to float rasterization (±1 LSB).
#include "ag_gfx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ag {

V2 hp(const Game& g, F64 x, F64 y, F64 f) {
  F64 d = g.depth;
  return {x - 98 * d * f, y + 26 * d * f};
}
V2 sp(const Game& g, F64 x, F64 y, F64 f) {
  F64 d = g.depth * 0.14 + g.impact * 0.06;
  return {x - 98 * d * f, y + 26 * d * f};
}
F64 chaos(const Game& g, F64 x) { return std::sin(g.t * 37 + x * 13) * g.shake; }

static std::vector<GradStop> toStops(const std::vector<StopStr>& in) {
  std::vector<GradStop> out;
  out.reserve(in.size());
  for (const auto& s : in) out.push_back({s.first, mustColor(s.second)});
  return out;
}

GradPtr gLinear(Canvas& cv, F64 x0, F64 y0, F64 x1, F64 y1,
                const std::vector<StopStr>& stops) {
  return cv.makeLinear(x0, y0, x1, y1, toStops(stops));
}
GradPtr gRadial(Canvas& cv, F64 x0, F64 y0, F64 r0, F64 x1, F64 y1, F64 r1,
                const std::vector<StopStr>& stops) {
  return cv.makeRadial(x0, y0, r0, x1, y1, r1, toStops(stops));
}

GradPtr sg(Canvas& cv, F64 y0, F64 y1, std::string_view c1,
           std::string_view c2) {
  F64 ya = std::isfinite(y0) ? y0 : 0;
  F64 yb = std::isfinite(y1) ? y1 : 100;
  return gLinear(cv, 0, ya, 0, yb, {{0, std::string(c1)}, {1, std::string(c2)}});
}

void shade(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
           F64 rot, std::string_view lc) {
  F64 cx = std::isfinite(x) ? x : 0, cy = std::isfinite(y) ? y : 0;
  F64 ax = std::isfinite(rx) ? std::fabs(rx) : 10;
  F64 ay = std::isfinite(ry) ? std::fabs(ry) : 10;
  F64 rad = std::max(ax, ay) * 1.15;
  std::string c0 = lc.empty() ? std::string(col) : std::string(lc);
  GradPtr g = gRadial(cv, cx, cy, 0, cx, cy, rad > 0 ? rad : 10,
                      {{0, c0}, {1, "rgba(0,0,0,0)"}});
  cv.setFillGrad(g);
  cv.beginPath();
  cv.ellipse(cx, cy, std::max<F64>(1, ax), std::max<F64>(1, ay), rot, 0, TAU);
  cv.fill();
}

static void capsuleBody(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, F64 bob) {
  F64 dx = b.x - a.x, dy = b.y - a.y;
  F64 len = std::hypot(dx, dy);
  if (len < 0.001) return;
  F64 ang = std::atan2(dy, dx);
  cv.beginPath();
  cv.arc(a.x, a.y + bob, std::max(0.1, r1), ang + 3.141592653589793 / 2,
         ang + 3.141592653589793 * 1.5);
  cv.arc(b.x, b.y + bob, std::max(0.1, r2), ang - 3.141592653589793 / 2,
         ang + 3.141592653589793 / 2);
  cv.closePath();
}

void capsule(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, ColorF flat, F64 bob) {
  if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) ||
      !std::isfinite(b.y))
    return;
  F64 dx = b.x - a.x, dy = b.y - a.y;
  if (std::hypot(dx, dy) < 0.001) return;
  capsuleBody(cv, a, b, r1, r2, bob);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.17g)", toU8(flat.r),
                toU8(flat.g), toU8(flat.b), (F64)flat.a);
  std::string s = buf;
  GradPtr g = gLinear(cv, a.x, a.y, b.x, b.y, {{0, s}, {0.5, s}, {1, s}});
  cv.setFillGrad(g);
  cv.fill();
  F64 ang = std::atan2(dy, dx), perp = ang + 3.141592653589793 / 2;
  F64 midX = (a.x + b.x) / 2, midY = (a.y + b.y) / 2 + bob;
  F64 nx = std::cos(perp), ny = std::sin(perp), avgR = (r1 + r2) / 2;
  F64 x0 = midX - nx * avgR, y0 = midY - ny * avgR;
  F64 x1 = midX + nx * avgR, y1 = midY + ny * avgR;
  cv.save();
  cv.setCompStr("soft-light");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(255,140,110,0.42)"},
                          {0.35, "rgba(255,220,190,0.30)"},
                          {0.7, "rgba(180,60,40,0.25)"},
                          {1, "rgba(80,20,15,0.40)"}}));
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(255,245,235,0.32)"},
                          {0.38, "rgba(255,230,210,0.08)"},
                          {0.7, "rgba(0,0,0,0)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("multiply");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(0,0,0,0)"},
                          {0.5, "rgba(0,0,0,0)"},
                          {0.85, "rgba(40,15,18,0.28)"},
                          {1, "rgba(25,8,10,0.48)"}}));
  cv.fill();
  cv.restore();
}

void capsuleGrad(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, GradPtr fill,
                 F64 bob) {
  if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) ||
      !std::isfinite(b.y))
    return;
  F64 dx = b.x - a.x, dy = b.y - a.y;
  if (std::hypot(dx, dy) < 0.001) return;
  capsuleBody(cv, a, b, r1, r2, bob);
  cv.setFillGrad(fill);
  cv.fill();
  F64 ang = std::atan2(dy, dx), perp = ang + 3.141592653589793 / 2;
  F64 midX = (a.x + b.x) / 2, midY = (a.y + b.y) / 2 + bob;
  F64 nx = std::cos(perp), ny = std::sin(perp), avgR = (r1 + r2) / 2;
  F64 x0 = midX - nx * avgR, y0 = midY - ny * avgR;
  F64 x1 = midX + nx * avgR, y1 = midY + ny * avgR;
  cv.save();
  cv.setCompStr("soft-light");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(255,140,110,0.42)"},
                          {0.35, "rgba(255,220,190,0.30)"},
                          {0.7, "rgba(180,60,40,0.25)"},
                          {1, "rgba(80,20,15,0.40)"}}));
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(255,245,235,0.32)"},
                          {0.38, "rgba(255,230,210,0.08)"},
                          {0.7, "rgba(0,0,0,0)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("multiply");
  cv.setFillGrad(gLinear(cv, x0, y0, x1, y1,
                         {{0, "rgba(0,0,0,0)"},
                          {0.5, "rgba(0,0,0,0)"},
                          {0.85, "rgba(40,15,18,0.28)"},
                          {1, "rgba(25,8,10,0.48)"}}));
  cv.fill();
  cv.restore();
}

void capsuleTone(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, F64 bob) {
  if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) ||
      !std::isfinite(b.y))
    return;
  F64 dx = b.x - a.x, dy = b.y - a.y;
  if (std::hypot(dx, dy) < 0.001) return;
  capsuleBody(cv, a, b, r1, r2, bob);
  cv.fill(); // current paint, no SSS/highlight/AO (JS quirk, see header)
}

void heartPath(Canvas& cv, F64 x, F64 y, F64 s) {
  cv.beginPath();
  cv.moveTo(x, y + s * 0.95);
  cv.bezierCurveTo(x - s * 1.35, y + s * 0.12, x - s * 0.85, y - s * 0.95, x,
                   y - s * 0.32);
  cv.bezierCurveTo(x + s * 0.85, y - s * 0.95, x + s * 1.35, y + s * 0.12, x,
                   y + s * 0.95);
  cv.closePath();
}

// ---- js/gfx.js:168-283 drawRoom ----
void drawRoom(Canvas& cv, Game&) {
  cv.setFillGrad(gLinear(cv, 0, 0, 0, 560,
                         {{0, "#120b0e"}, {0.65, "#180e13"}, {1, "#201217"}}));
  cv.fillRect(0, 0, VW, VH);
  cv.setFillColorStr("rgba(0,0,0,0.45)");
  cv.fillRect(0, 0, VW, 72);
  cv.setFillColorStr("rgba(255,200,160,0.02)");
  cv.fillRect(0, 70, VW, 2);
  const F64 winX = 1030, winY = 60, winW = 200, winH = 260;
  cv.setFillColorStr("rgba(135,155,175,0.07)");
  cv.fillRect(winX, winY, winW, winH);
  cv.setFillGrad(gLinear(cv, winX, winY, winX, winY + winH,
                         {{0, "rgba(80,105,130,0.22)"},
                          {1, "rgba(25,35,45,0.10)"}}));
  cv.fillRect(winX, winY, winW, winH);
  const F64 moonX = 1172, moonY = 120;
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gRadial(cv, moonX, moonY, 10, moonX, moonY, 70,
                         {{0, "rgba(225,238,250,0.25)"},
                          {0.5, "rgba(180,210,235,0.06)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.beginPath();
  cv.arc(moonX, moonY, 70, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(235,244,252,0.85)");
  cv.beginPath();
  cv.arc(moonX, moonY, 20, 0, TAU);
  cv.fill();
  cv.restore();
  cv.setStrokeColorStr("#22151a");
  cv.setLineWidth(5);
  cv.beginPath();
  cv.moveTo(winX + winW / 2, winY);
  cv.lineTo(winX + winW / 2, winY + winH);
  cv.moveTo(winX, winY + winH * 0.42);
  cv.lineTo(winX + winW, winY + winH * 0.42);
  cv.stroke();
  // NOTE js strokes the window rect outline via strokeRect before mullions;
  // strokeRect(winX,winY,winW,winH) with same style:
  cv.strokeRect(winX, winY, winW, winH);
  cv.save();
  cv.setCompStr("soft-light");
  cv.setFillColorStr("rgba(200,215,230,0.18)");
  for (int c = 0; c < 5; c++) {
    F64 cx0 = winX - 35 + c * 8;
    cv.beginPath();
    cv.moveTo(cx0, 40);
    cv.bezierCurveTo(cx0 + 20, 150, cx0 - 15, 260, cx0 + 12, 360);
    cv.lineTo(cx0 + 22, 360);
    cv.bezierCurveTo(cx0 - 5, 260, cx0 + 30, 150, cx0 + 10, 40);
    cv.closePath();
    cv.fill();
  }
  cv.restore();
  cv.setFillColorStr("#1c0f13");
  cv.fillRect(48, 465, 158, 120);
  cv.setFillGrad(gLinear(cv, 48, 465, 48, 475,
                         {{0, "rgba(255,190,130,0.16)"}, {1, "rgba(0,0,0,0)"}}));
  cv.fillRect(48, 465, 158, 10);
  cv.setStrokeColorStr("#2e1820");
  cv.setLineWidth(2);
  cv.strokeRect(66, 488, 120, 32);
  cv.setFillColorStr("#bfa068");
  cv.beginPath();
  cv.arc(126, 504, 3.8, 0, TAU);
  cv.fill();
  cv.setFillColorStr("rgba(255,255,230,0.8)");
  cv.beginPath();
  cv.arc(125, 503, 1.2, 0, TAU);
  cv.fill();
  const F64 lx = 127, ly = 405;
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gRadial(cv, lx, ly, 8, lx, ly, 380,
                         {{0, "rgba(255,195,125,0.40)"},
                          {0.28, "rgba(255,170,95,0.18)"},
                          {0.7, "rgba(255,140,80,0.05)"},
                          {1, "rgba(255,140,80,0)"}}));
  cv.beginPath();
  cv.arc(lx, ly, 380, 0, TAU);
  cv.fill();
  cv.restore();
  cv.setStrokeColorStr("#684525");
  cv.setLineWidth(5);
  cv.beginPath();
  cv.moveTo(lx, 465);
  cv.lineTo(lx, 428);
  cv.stroke();
  cv.setStrokeColorStr("#c49e62");
  cv.setLineWidth(2);
  cv.beginPath();
  cv.moveTo(lx - 1, 465);
  cv.lineTo(lx - 1, 428);
  cv.stroke();
  cv.setFillColorStr("#42242b");
  cv.beginPath();
  cv.moveTo(94, 432);
  cv.lineTo(160, 432);
  cv.lineTo(146, 390);
  cv.lineTo(108, 390);
  cv.closePath();
  cv.fill();
  cv.setFillGrad(gLinear(cv, 108, 390, 146, 432,
                         {{0, "rgba(255,225,160,0.85)"},
                          {0.5, "rgba(255,185,115,0.65)"},
                          {1, "rgba(255,140,85,0.45)"}}));
  cv.beginPath();
  cv.moveTo(109, 392);
  cv.lineTo(145, 392);
  cv.lineTo(156, 430);
  cv.lineTo(98, 430);
  cv.closePath();
  cv.fill();
}

// ---- js/gfx.js:285-370 drawBed ----
void drawBed(Canvas& cv, Game& g) {
  cv.setFillColorStr("#1e0e13");
  cv.fillRect(0, 552, VW, VH - 552);
  cv.setFillGrad(gLinear(cv, 0, 532, 0, 585,
                         {{0, "#421d26"}, {0.4, "#31141c"}, {1, "#1b090f"}}));
  cv.fillRect(24, 532, VW - 48, 56);
  cv.setFillGrad(gLinear(cv, 0, 545, 0, 720,
                         {{0, "#532833"},
                          {0.3, "#3d1c24"},
                          {0.7, "#2c1219"},
                          {1, "#1a090e"}}));
  cv.fillRect(0, 550, VW, VH - 550);
  F64 sink = std::sin(g.t * 8) * g.impact * 3;
  cv.save();
  cv.setCompStr("multiply");
  cv.setStrokeColorStr("rgba(20,5,8,0.48)");
  cv.setLineWidth(3.5);
  for (int i = 0; i < 7; i++) {
    F64 y = 562 + i * 22;
    cv.beginPath();
    cv.moveTo(40 + ((i * 73) % 110), y + sink * 0.4);
    cv.bezierCurveTo(460, y - 24 + ((i * 31) % 28) + sink, 820,
                     y + 14 - ((i * 17) % 20), 1240 - ((i * 47) % 120), y);
    cv.stroke();
  }
  cv.restore();
  cv.save();
  cv.setCompStr("soft-light");
  cv.setStrokeColorStr("rgba(255,210,185,0.24)");
  cv.setLineWidth(4);
  for (int i = 0; i < 5; i++) {
    F64 y = 558 + i * 26;
    cv.beginPath();
    cv.moveTo(80 + ((i * 57) % 80), y - 4);
    cv.bezierCurveTo(440, y - 30 + ((i * 27) % 24), 780, y + 8, 1180, y - 6);
    cv.stroke();
  }
  cv.restore();
  const F64 px = 286, py = 542;
  cv.setFillGrad(gLinear(cv, px - 110, py - 35, px + 110, py + 35,
                         {{0, "#c7b096"},
                          {0.4, "#b0977c"},
                          {0.85, "#8a7460"},
                          {1, "#635142"}}));
  cv.beginPath();
  cv.ellipse(px, py, 112, 34, -0.04, 0, TAU);
  cv.fill();
  cv.save();
  cv.setCompStr("multiply");
  cv.setFillGrad(gRadial(cv, px + 45, py + 4, 4, px + 45, py + 4, 52,
                         {{0, "rgba(75,52,38,0.55)"},
                          {0.65, "rgba(85,58,42,0.22)"},
                          {1, "rgba(0,0,0,0)"}}));
  cv.beginPath();
  cv.ellipse(px + 45, py + 4, 48, 22, -0.08, 0, TAU);
  cv.fill();
  cv.restore();
  cv.save();
  cv.setCompStr("multiply");
  shade(cv, 540, 580 + sink * 0.5, 340, 32, "rgba(15,4,8,0.62)", 0);
  shade(cv, 880, 594 + sink * 0.3, 140, 24, "rgba(15,4,8,0.50)", 0);
  shade(cv, 360, 574, 90, 20, "rgba(15,4,8,0.45)", 0);
  cv.restore();
}

// ---- js/gfx.js:375-474 drawFluids ----
void drawFluids(Canvas& cv, Game& g) {
  F64 t = g.t;
  for (auto& d : g.drips) {
    F64 ox = d.ox, oy = 505; // js: d.ox ?? d.x — port stores resolved ox
    F64 p = d.p * 3;
    int i = std::min((int)p, 2);
    F64 f = p - i;
    static const F64 PY[4] = {0, 22, 46, 68};
    static const F64 PX[4] = {0, -7, -12, -16};
    F64 y = oy + lerp(PY[i], PY[i + 1], f);
    F64 x = ox + lerp(PX[i], PX[i + 1], f) + std::sin(d.p * 9.5 + d.j) * 1.8;
    cv.setFillColorStr("rgba(255,248,244,0.78)");
    cv.beginPath();
    cv.ellipse(x, y, 2.6, 4.4, 0, 0, TAU);
    cv.fill();
    cv.setStrokeColorStr("rgba(255,248,244,0.42)");
    cv.setLineWidth(1.4);
    cv.beginPath();
    cv.moveTo(x, y - 4);
    cv.quadraticCurveTo(x + 1.5, y - 10, x - 1, y - 16);
    cv.stroke();
    cv.setFillColorStr("rgba(255,255,255,0.95)");
    cv.beginPath();
    cv.arc(x - 0.7, y - 1.2, 1.1, 0, TAU);
    cv.fill();
  }
  for (auto& j : g.jets) {
    if (j.view != 's') continue;
    F64 spd = std::hypot(j.vx, j.vy);
    F64 ang = std::atan2(j.vy, j.vx);
    F64 alpha = clamp(j.life * 2.4, 0.0, 1.0);
    cv.save();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,250,246,%.17g)", 0.88 * alpha);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(j.x, j.y, 4 + spd * 0.009, 2.4, ang, 0, TAU);
    cv.fill();
    F64 tx = j.x - std::cos(ang) * 16, ty = j.y - std::sin(ang) * 16;
    std::snprintf(buf, sizeof(buf), "rgba(255,245,240,%.17g)", 0.75 * alpha);
    std::string c0 = buf;
    cv.setStrokeGrad(gLinear(cv, j.x, j.y, tx, ty,
                             {{0, c0}, {1, "rgba(255,245,240,0)"}}));
    cv.setLineWidth(2.6);
    cv.beginPath();
    cv.moveTo(j.x, j.y);
    cv.lineTo(tx, ty);
    cv.stroke();
    cv.restore();
  }
  for (auto& s : g.glisten) {
    F64 pulse = 0.5 + 0.5 * std::sin(t * 8 + s.x);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,252,248,%.17g)", s.a * 0.65 * pulse);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(s.x, s.y, 1.6, 0, TAU);
    cv.fill();
  }
  for (auto& s : g.sweat) {
    F64 a = 0.55 * s.life;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(20,5,8,%.17g)", 0.25 * a);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.ellipse(s.x, s.y + 1, 1.6, 1.1, 0, 0, TAU);
    cv.fill();
    std::snprintf(buf, sizeof(buf), "rgba(255,255,255,%.17g)", 0.85 * a);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(s.x - 0.4, s.y - 0.5, 0.9, 0, TAU);
    cv.fill();
  }
  for (auto& h : g.hearts) {
    F64 fade = std::min(1.0, h.life);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,100,135,%.17g)", 0.62 * fade);
    cv.save();
    cv.setFillColorStr(buf);
    cv.setShadowStr("rgba(255,80,120,0.5)", 8);
    heartPath(cv, h.x + std::sin(h.ph) * 7, h.y, 7.5 * h.s);
    cv.fill();
    cv.restore();
  }
}

// ---- js/gfx.js:476-533 drawLight ----
void drawLight(Canvas& cv, Game& g) {
  F64 ar = g.ar, t = g.t;
  cv.save();
  cv.setCompStr("screen");
  cv.setFillGrad(gRadial(cv, 127, 400, 30, 127, 400, 680,
                         {{0, "rgba(255,185,115,0.18)"},
                          {0.45, "rgba(255,155,90,0.06)"},
                          {1, "rgba(255,155,90,0)"}}));
  cv.fillRect(0, 0, VW, VH);
  cv.restore();
  if (ar > 50 && g.state == "play") {
    F64 intensity = ((ar - 50) / 50) * (0.65 + 0.35 * std::sin(t * 3.8));
    char b1[48], b2[48];
    std::snprintf(b1, sizeof(b1), "rgba(255,45,95,%.17g)", 0.08 * intensity);
    std::snprintf(b2, sizeof(b2), "rgba(220,25,80,%.17g)", 0.24 * intensity);
    cv.save();
    cv.setFillGrad(gRadial(cv, 640, 420, 220, 640, 420, 780,
                           {{0, "rgba(255,70,110,0)"}, {0.7, b1}, {1, b2}}));
    cv.fillRect(0, 0, VW, VH);
    cv.restore();
  }
  cv.setFillGrad(gRadial(cv, 640, 380, 340, 640, 420, 880,
                         {{0, "rgba(8,3,8,0)"},
                          {0.7, "rgba(8,3,8,0.30)"},
                          {1, "rgba(8,3,8,0.72)"}}));
  cv.fillRect(0, 0, VW, VH);
  if (g.bloom > 0.005) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,242,236,%.17g)", g.bloom * 0.52);
    cv.save();
    cv.setFillColorStr(buf);
    cv.fillRect(0, 0, VW, VH);
    cv.restore();
  }
  cv.save();
  cv.setCompStr("screen");
  for (int i = 0; i < 16; i++) {
    F64 mt = t * 0.12 + i * 4.3;
    F64 mx = 130 + ((i * 89) % 360) + std::sin(mt + i) * 45;
    F64 my = 360 + std::cos(mt * 0.85 + i * 2.1) * 130 + ((i * 47) % 150);
    F64 alpha = 0.04 + 0.03 * std::sin(mt * 2 + i);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(255,220,175,%.17g)", alpha);
    cv.setFillColorStr(buf);
    cv.beginPath();
    cv.arc(mx, my, 1.2 + (i % 3) * 0.8, 0, TAU);
    cv.fill();
  }
  cv.restore();
}

} // namespace ag
