// Afterglow native — ag_canvas.cpp
// Software Canvas2D rasterizer (HTML5 semantics, 1x, no AA).
// Performance design (mirrors the js/gfx.js findings):
//   - fills iterate bbox only; gradients sampled per pixel in user space
//   - gradient objects cached by exact key (transform+geom+stops) so the
//     222 shade()/frame pattern allocates ~12 gradients, not ~300
//   - zero heap allocation in the pixel loop (scratch buffers reused)
// PERF TODO: stroke rasterization is per-pixel distance tests (correct but
//   O(segs*pixels)); a scanline stroker will replace it once parity is green.
#include "ag_canvas.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ag {
namespace {

uint64_t dbits(F64 v) {
  uint64_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

ColorF mixC(const ColorF& a, const ColorF& b, F64 t) {
  float k = (float)t;
  return {(float)(a.r + (b.r - a.r) * k), (float)(a.g + (b.g - a.g) * k),
          (float)(a.b + (b.b - a.b) * k), (float)(a.a + (b.a - a.a) * k)};
}

// W3C Compositing & Blending L1 blend functions (non-premultiplied sRGB).
float blendChannel(CompOp op, float cb, float cs) {
  switch (op) {
  case CompOp::SrcOver:
    return cs;
  case CompOp::Multiply:
    return cb * cs;
  case CompOp::Screen:
    return 1.0f - (1.0f - cb) * (1.0f - cs);
  case CompOp::SoftLight: {
    float cbq = cb < 0 ? 0 : (cb > 1 ? 1 : cb);
    float d = (cbq <= 0.25f) ? ((16.0f * cbq - 12.0f) * cbq + 4.0f) * cbq
                             : std::sqrt(cbq);
    if (cs <= 0.5f)
      return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    return cb + (2.0f * cs - 1.0f) * (d - cb);
  }
  }
  return cs;
}

uint64_t fnv1a(const void* data, size_t n) {
  const uint8_t* p = (const uint8_t*)data;
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

struct Span {
  int y = 0, x0 = 0, x1 = 0; // [x0,x1)
};

// Nonzero winding of (x,y) over closed polygon loops.
int Canvas::winding(const std::vector<DevPath>& loops, F64 x, F64 y) {
  int w = 0;
  for (const auto& L : loops) {
    size_t n = L.pts.size();
    if (n < 3) continue;
    for (size_t i = 0; i < n; i++) {
      const auto& p = L.pts[i];
      const auto& q = L.pts[(i + 1) % n];
      if (p.y <= y) {
        if (q.y > y && (q.x - p.x) * (y - p.y) - (x - p.x) * (q.y - p.y) > 0)
          w++;
      } else {
        if (q.y <= y && (q.x - p.x) * (y - p.y) - (x - p.x) * (q.y - p.y) < 0)
          w--;
      }
    }
  }
  return w;
}

} // namespace

// ---- Gradient::sample (linear + full two-circle radial, WHATWG) ----
ColorF Gradient::sample(F64 ux, F64 uy) const {
  if (stops.empty()) return {0, 0, 0, 0};
  F64 t = 0;
  if (!radial) {
    F64 dx = x1 - x0, dy = y1 - y0;
    F64 d2 = dx * dx + dy * dy;
    t = (d2 > 1e-12) ? ((ux - x0) * dx + (uy - y0) * dy) / d2 : 0.0;
  } else {
    F64 dx = x1 - x0, dy = y1 - y0, dr = r1 - r0;
    F64 px = ux - x0, py = uy - y0;
    F64 A = dx * dx + dy * dy - dr * dr;
    F64 B = -2.0 * (px * dx + py * dy + r0 * dr);
    F64 C = px * px + py * py - r0 * r0;
    F64 tans = 0;
    if (std::fabs(A) < 1e-12) {
      tans = (std::fabs(B) < 1e-12) ? 0 : -C / B;
    } else {
      F64 D = B * B - 4 * A * C;
      if (D < 0) return {0, 0, 0, 0}; // ray misses cone: unpainted (spec)
      F64 s = std::sqrt(D);
      F64 t1 = (-B + s) / (2 * A), t2 = (-B - s) / (2 * A);
      tans = t1 > t2 ? t1 : t2;
    }
    t = tans;
  }
  if (t <= stops.front().off) return stops.front().col;
  if (t >= stops.back().off) return stops.back().col;
  for (size_t i = 1; i < stops.size(); i++) {
    if (t <= stops[i].off) {
      F64 span = stops[i].off - stops[i - 1].off;
      F64 k = span > 1e-12 ? (t - stops[i - 1].off) / span : 0;
      return mixC(stops[i - 1].col, stops[i].col, k);
    }
  }
  return stops.back().col;
}

// ---- Canvas basics ----
Canvas::Canvas(int w, int h) : w_(w), h_(h), fb_((size_t)w * (size_t)h, 0) {
  stack_.push_back(State());
}

void Canvas::save() {
  stack_.push_back(stack_.back());
  clipDepth_.push_back(clips_.size());
}
void Canvas::restore() {
  if (stack_.size() <= 1) return;
  stack_.pop_back();
  size_t d = clipDepth_.empty() ? 0 : clipDepth_.back();
  if (!clipDepth_.empty()) clipDepth_.pop_back();
  if (d < clips_.size()) clips_.resize(d);
}

void Canvas::setTransform(F64 a, F64 b, F64 c, F64 d, F64 e, F64 f) {
  stack_.back().ctm = {a, b, c, d, e, f};
}
Mat Canvas::getTransform() const { return stack_.back().ctm; }
void Canvas::translate(F64 x, F64 y) {
  Mat& m = stack_.back().ctm;
  m.e += m.a * x + m.c * y;
  m.f += m.b * x + m.d * y;
}
void Canvas::scale(F64 sx, F64 sy) {
  Mat& m = stack_.back().ctm;
  m.a *= sx;
  m.b *= sx;
  m.c *= sy;
  m.d *= sy;
}
void Canvas::rotate(F64 ang) {
  F64 co = std::cos(ang), si = std::sin(ang);
  Mat& m = stack_.back().ctm;
  F64 a = m.a * co + m.c * si, c = -m.a * si + m.c * co;
  F64 b = m.b * co + m.d * si, d = -m.b * si + m.d * co;
  m.a = a;
  m.b = b;
  m.c = c;
  m.d = d;
}

void Canvas::setGlobalAlpha(F64 a) { stack_.back().alpha = clamp(a, 0.0, 1.0); }
void Canvas::setComp(CompOp op) { stack_.back().comp = op; }
void Canvas::setCompStr(std::string_view op) {
  if (op == "multiply") setComp(CompOp::Multiply);
  else if (op == "screen") setComp(CompOp::Screen);
  else if (op == "soft-light") setComp(CompOp::SoftLight);
  else setComp(CompOp::SrcOver);
}
void Canvas::setLineWidth(F64 w) { stack_.back().lineW = w; }
void Canvas::setLineCap(LineCap c) { stack_.back().cap = c; }
void Canvas::setLineCapStr(std::string_view s) {
  if (s == "round") setLineCap(LineCap::Round);
  else if (s == "square") setLineCap(LineCap::Square);
  else setLineCap(LineCap::Butt);
}
void Canvas::setLineJoin(LineJoin j) { stack_.back().join = j; }
void Canvas::setShadow(ColorF c, F64 blur) {
  stack_.back().shadow = c;
  stack_.back().shadowBlur = blur;
}
void Canvas::setShadowStr(std::string_view s, F64 blur) {
  setShadow(mustColor(s), blur);
}
void Canvas::clearShadow() {
  stack_.back().shadow = {0, 0, 0, 0};
  stack_.back().shadowBlur = 0;
}

void Canvas::setFillColor(ColorF c) {
  fillPaint_.isGrad = false;
  fillPaint_.color = c;
}
void Canvas::setFillColorStr(std::string_view s) { setFillColor(mustColor(s)); }
void Canvas::setStrokeColor(ColorF c) {
  strokePaint_.isGrad = false;
  strokePaint_.color = c;
}
void Canvas::setStrokeColorStr(std::string_view s) {
  setStrokeColor(mustColor(s));
}
void Canvas::setFillGrad(GradPtr g) {
  fillPaint_.isGrad = true;
  fillPaint_.grad = std::move(g);
}
void Canvas::setStrokeGrad(GradPtr g) {
  strokePaint_.isGrad = true;
  strokePaint_.grad = std::move(g);
}

// ---- gradient cache (port of js/gfx.js _gcache) ----
size_t Canvas::GKeyHash::operator()(const GKey& k) const noexcept {
  uint64_t h = 1469598103934665603ull;
  h ^= (uint64_t)(k.radial ? 0x9e37 : 0x51f3);
  h *= 1099511628211ull;
  for (int i = 0; i < k.n; i++) {
    h ^= k.bits[i];
    h *= 1099511628211ull;
  }
  h ^= fnv1a(k.stops.data(), k.stops.size());
  h *= 1099511628211ull;
  return (size_t)h;
}

void Canvas::keyDoubles(GKey& k, const F64* v, int n) {  for (int i = 0; i < n && k.n < 12; i++) {
    uint64_t u = 0;
    std::memcpy(&u, &v[i], 8);
    k.bits[k.n++] = u;
  }
}
void Canvas::keyStops(GKey& k, std::span<const GradStop> stops) {
  k.stops.reserve(stops.size() * 24);
  for (const auto& s : stops) {
    uint64_t u = 0;
    std::memcpy(&u, &s.off, 8);
    for (int i = 0; i < 8; i++) k.stops.push_back((char)(u >> (i * 8)));
    RGBA8 p = pack(s.col);
    k.stops.push_back((char)p.r);
    k.stops.push_back((char)p.g);
    k.stops.push_back((char)p.b);
    k.stops.push_back((char)p.a);
  }
}

GradPtr Canvas::makeLinear(F64 x0, F64 y0, F64 x1, F64 y1,
                           std::span<const GradStop> stops) {
  GKey k;
  k.radial = false;
  F64 g[4] = {x0, y0, x1, y1};
  keyDoubles(k, g, 4);
  const Mat& m = stack_.back().ctm;
  F64 t[6] = {m.a, m.b, m.c, m.d, m.e, m.f};
  keyDoubles(k, t, 6);
  keyStops(k, stops);
  auto it = gcache_.find(k);
  if (it != gcache_.end()) return it->second;
  if (gcache_.size() >= GCAP) gcache_.clear();
  auto gr = std::make_shared<Gradient>();
  gr->radial = false;
  gr->x0 = x0;
  gr->y0 = y0;
  gr->x1 = x1;
  gr->y1 = y1;
  gr->stops.assign(stops.begin(), stops.end());
  gcache_.emplace(std::move(k), gr);
  return gr;
}
GradPtr Canvas::makeRadial(F64 x0, F64 y0, F64 r0, F64 x1, F64 y1, F64 r1,
                           std::span<const GradStop> stops) {
  GKey k;
  k.radial = true;
  F64 g[6] = {x0, y0, r0, x1, y1, r1};
  keyDoubles(k, g, 6);
  const Mat& m = stack_.back().ctm;
  F64 t[6] = {m.a, m.b, m.c, m.d, m.e, m.f};
  keyDoubles(k, t, 6);
  keyStops(k, stops);
  auto it = gcache_.find(k);
  if (it != gcache_.end()) return it->second;
  if (gcache_.size() >= GCAP) gcache_.clear();
  auto gr = std::make_shared<Gradient>();
  gr->radial = true;
  gr->x0 = x0;
  gr->y0 = y0;
  gr->r0 = r0;
  gr->x1 = x1;
  gr->y1 = y1;
  gr->r1 = r1;
  gr->stops.assign(stops.begin(), stops.end());
  gcache_.emplace(std::move(k), gr);
  return gr;
}
size_t Canvas::cacheSize() const { return gcache_.size(); }
void Canvas::cacheClear() { gcache_.clear(); }

// ---- path recording (curves flattened to user-space polylines) ----
void Canvas::beginPath() { path_.clear(); }
void Canvas::closePath() {
  if (!path_.empty()) path_.back().closed = true;
}
void Canvas::moveTo(F64 x, F64 y) {
  path_.push_back(SubPath());
  path_.back().pts.push_back({x, y});
}
void Canvas::lineTo(F64 x, F64 y) {
  if (path_.empty() || path_.back().pts.empty()) {
    moveTo(x, y);
    return;
  }
  const Pt& l = path_.back().pts.back();
  if (l.x == x && l.y == y) return;
  path_.back().pts.push_back({x, y});
}

void Canvas::flatCubic(std::vector<Pt>& out, F64 x0, F64 y0, F64 x1,
                       F64 y1, F64 x2, F64 y2, F64 x3, F64 y3, int depth) {
  // flatness: max distance of controls from chord, squared
  F64 dx = x3 - x0, dy = y3 - y0;
  F64 d2 = dx * dx + dy * dy;
  auto dist2 = [&](F64 px, F64 py) {
    if (d2 < 1e-12) {
      F64 ex = px - x0, ey = py - y0;
      return ex * ex + ey * ey;
    }
    F64 cr = (px - x0) * dy - (py - y0) * dx;
    return (cr * cr) / d2;
  };
  if (depth >= 10 || (dist2(x1, y1) <= 0.12 && dist2(x2, y2) <= 0.12)) {
    out.push_back({x3, y3});
    return;
  }
  F64 x01 = (x0 + x1) * 0.5, y01 = (y0 + y1) * 0.5;
  F64 x12 = (x1 + x2) * 0.5, y12 = (y1 + y2) * 0.5;
  F64 x23 = (x2 + x3) * 0.5, y23 = (y2 + y3) * 0.5;
  F64 x012 = (x01 + x12) * 0.5, y012 = (y01 + y12) * 0.5;
  F64 x123 = (x12 + x23) * 0.5, y123 = (y12 + y23) * 0.5;
  F64 xm = (x012 + x123) * 0.5, ym = (y012 + y123) * 0.5;
  flatCubic(out, x0, y0, x01, y01, x012, y012, xm, ym, depth + 1);
  flatCubic(out, xm, ym, x123, y123, x23, y23, x3, y3, depth + 1);
}

void Canvas::bezierCurveTo(F64 c1x, F64 c1y, F64 c2x, F64 c2y, F64 x, F64 y) {
  if (path_.empty() || path_.back().pts.empty()) {
    moveTo(x, y);
    return;
  }
  const Pt& p = path_.back().pts.back();
  flatCubic(path_.back().pts, p.x, p.y, c1x, c1y, c2x, c2y, x, y, 0);
}
void Canvas::quadraticCurveTo(F64 cx, F64 cy, F64 x, F64 y) {
  if (path_.empty() || path_.back().pts.empty()) {
    moveTo(x, y);
    return;
  }
  const Pt& p = path_.back().pts.back();
  F64 c1x = p.x + (2.0 / 3.0) * (cx - p.x), c1y = p.y + (2.0 / 3.0) * (cy - p.y);
  F64 c2x = x + (2.0 / 3.0) * (cx - x), c2y = y + (2.0 / 3.0) * (cy - y);
  flatCubic(path_.back().pts, p.x, p.y, c1x, c1y, c2x, c2y, x, y, 0);
}
void Canvas::arc(F64 x, F64 y, F64 r, F64 a0, F64 a1, bool ccw) {
  if (!(r > 0)) return;
  ellipse(x, y, r, r, 0, a0, a1, ccw);
}
void Canvas::ellipse(F64 x, F64 y, F64 rx, F64 ry, F64 rot, F64 a0, F64 a1,
                     bool ccw) {
  if (!(rx > 0) || !(ry > 0)) return;
  F64 sweep = a1 - a0;
  while (sweep > TAU) sweep -= TAU;
  while (sweep < -TAU) sweep += TAU;
  if (!ccw && sweep < 0) sweep += TAU;
  if (ccw && sweep > 0) sweep -= TAU;
  if (std::fabs(sweep) < 1e-9) return;
  F64 approx = std::fabs(sweep) * std::max(rx, ry);
  int n = (int)clamp(approx * 0.5, 8.0, 512.0);
  F64 ca = std::cos(rot), sa = std::sin(rot);
  for (int i = 0; i <= n; i++) {
    F64 a = a0 + sweep * (F64)i / (F64)n;
    F64 ex = rx * std::cos(a), ey = ry * std::sin(a);
    F64 px = x + ex * ca - ey * sa, py = y + ex * sa + ey * ca;
    if (i == 0) {
      if (path_.empty() || path_.back().pts.empty()) moveTo(px, py);
      else lineTo(px, py);
    } else {
      lineTo(px, py);
    }
  }
}
void Canvas::rect(F64 x, F64 y, F64 w, F64 h) {
  moveTo(x, y);
  lineTo(x + w, y);
  lineTo(x + w, y + h);
  lineTo(x, y + h);
  closePath();
}
void Canvas::fillRect(F64 x, F64 y, F64 w, F64 h) {
  beginPath();
  rect(x, y, w, h);
  fill();
}
void Canvas::strokeRect(F64 x, F64 y, F64 w, F64 h) {
  beginPath();
  rect(x, y, w, h);
  stroke();
}

// ---- raster ----
Canvas::Pt Canvas::applyCtm(Pt p) const {
  const Mat& m = stack_.back().ctm;
  return {m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f};
}

std::vector<Canvas::DevPath> Canvas::devicePaths() const {
  std::vector<DevPath> out;
  out.reserve(path_.size());
  for (const auto& s : path_) {
    if (s.pts.empty()) continue;
    DevPath d;
    d.closed = s.closed;
    d.pts.reserve(s.pts.size());
    for (const auto& p : s.pts) d.pts.push_back(applyCtm(p));
    out.push_back(std::move(d));
  }
  return out;
}

bool Canvas::insideClips(F64 dx, F64 dy) const {
  for (const auto& entry : clips_) {
    if (winding(entry, dx, dy) == 0) return false;
  }
  return true;
}

void Canvas::blendPixel(int x, int y, const ColorF& src) {
  if ((unsigned)x >= (unsigned)w_ || (unsigned)y >= (unsigned)h_) return;
  if (src.a <= 0) return;
  const State& st = stack_.back();
  uint32_t& dst = fb_[(size_t)y * (size_t)w_ + (size_t)x];
  RGBA8 dp = {(uint8_t)(dst & 255), (uint8_t)((dst >> 8) & 255),
              (uint8_t)((dst >> 16) & 255), (uint8_t)((dst >> 24) & 255)};
  ColorF cb = unpack(dp);
  float as = src.a, ab = cb.a;
  ColorF cs = {blendChannel(st.comp, cb.r, src.r),
               blendChannel(st.comp, cb.g, src.g),
               blendChannel(st.comp, cb.b, src.b), 0};
  float ao = as + ab * (1 - as);
  ColorF out;
  if (ao <= 0) {
    out = {0, 0, 0, 0};
  } else {
    out.r = (cs.r * as + cb.r * ab * (1 - as)) / ao;
    out.g = (cs.g * as + cb.g * ab * (1 - as)) / ao;
    out.b = (cs.b * as + cb.b * ab * (1 - as)) / ao;
    out.a = ao;
  }
  RGBA8 p = pack(out);
  dst = (uint32_t)p.r | ((uint32_t)p.g << 8) | ((uint32_t)p.b << 16) |
        ((uint32_t)p.a << 24);
}

ColorF Canvas::resolvePaint(const FillPaint& paint, F64 ux, F64 uy) const {
  ColorF c = paint.isGrad && paint.grad ? paint.grad->sample(ux, uy)
                                        : paint.color;
  c.a *= (float)stack_.back().alpha;
  return c;
}

void Canvas::clip() {
  auto dev = devicePaths();
  std::vector<DevPath> entry;
  for (auto& d : dev) {
    if (d.pts.size() >= 3) {
      d.closed = true;
      entry.push_back(std::move(d));
    }
  }
  if (!entry.empty()) clips_.push_back(std::move(entry));
}

void Canvas::fillPolys(const std::vector<DevPath>& dev, const FillPaint& paint) {
  struct Edge {
    F64 y0, y1, x, dxdy;
    int dir;
  };
  std::vector<Edge> edges;
  F64 gMinX = 1e30, gMinY = 1e30, gMaxX = -1e30, gMaxY = -1e30;
  for (const auto& L : dev) {
    size_t n = L.pts.size();
    if (n < 2) continue;
    size_t m = L.closed ? n : (n >= 2 ? n - 1 : 0);
    for (size_t i = 0; i < m; i++) {
      const Pt& p = L.pts[i];
      const Pt& q = L.pts[(i + 1) % n];
      if (p.x < gMinX) gMinX = p.x;
      if (q.x < gMinX) gMinX = q.x;
      if (p.x > gMaxX) gMaxX = p.x;
      if (q.x > gMaxX) gMaxX = q.x;
      if (p.y < gMinY) gMinY = p.y;
      if (q.y < gMinY) gMinY = q.y;
      if (p.y > gMaxY) gMaxY = p.y;
      if (q.y > gMaxY) gMaxY = q.y;
      if (p.y == q.y) continue;
      if (p.y < q.y) edges.push_back({p.y, q.y, p.x, (q.x - p.x) / (q.y - p.y), +1});
      else edges.push_back({q.y, p.y, q.x, (p.x - q.x) / (p.y - q.y), -1});
    }
  }
  if (edges.empty()) return;
  int yA = std::max(0, (int)std::floor(gMinY));
  int yB = std::min(h_ - 1, (int)std::ceil(gMaxY) - 1);
  if (yB < yA) return;

  // inverse CTM for gradient sampling (user space at paint time, per spec)
  const Mat& mc = stack_.back().ctm;
  F64 det = mc.a * mc.d - mc.b * mc.c;
  bool hasInv = std::fabs(det) > 1e-12;
  F64 ia = 0, ib = 0, ic = 0, id = 0, ie = 0, iff = 0;
  if (hasInv) {
    ia = mc.d / det;
    ib = -mc.b / det;
    ic = -mc.c / det;
    id = mc.a / det;
    ie = (mc.c * mc.f - mc.d * mc.e) / det;
    iff = (mc.b * mc.e - mc.a * mc.f) / det;
  }
  bool needUser = paint.isGrad && paint.grad && hasInv;

  std::vector<std::pair<F64, int>> xs;
  xs.reserve(32);
  for (int y = yA; y <= yB; y++) {
    F64 yc = (F64)y + 0.5;
    xs.clear();
    for (const auto& e : edges) {
      if (yc >= e.y0 && yc < e.y1) xs.emplace_back(e.x + (yc - e.y0) * e.dxdy, e.dir);
    }
    if (xs.empty()) continue;
    std::sort(xs.begin(), xs.end(),
              [](const auto& A, const auto& B) { return A.first < B.first; });
    int wind = 0;
    for (size_t i = 0; i + 1 < xs.size(); i++) {
      wind += xs[i].second;
      if (wind == 0) continue;
      int x0 = std::max(0, (int)std::ceil(xs[i].first - 0.5));
      int x1 = std::min(w_, (int)std::ceil(xs[i + 1].first - 0.5));
      for (int x = x0; x < x1; x++) {
        F64 dx = (F64)x + 0.5;
        if (!insideClips(dx, yc)) continue;
        ColorF c;
        if (needUser) {
          F64 ux = ia * dx + ic * yc + ie, uy = ib * dx + id * yc + iff;
          c = resolvePaint(paint, ux, uy);
        } else if (paint.isGrad) {
          c = paint.color; // degenerate CTM: fall back to flat (rare)
          c.a *= (float)stack_.back().alpha;
        } else {
          c = paint.color;
          c.a *= (float)stack_.back().alpha;
        }
        blendPixel(x, y, c);
      }
    }
  }
}

void Canvas::fill() {
  auto dev = devicePaths();
  if (dev.empty()) return;
  const State& st = stack_.back();
  // shadow pass (canvas draws shadows source-over, ignoring comp)
  if (st.shadow.a > 0 && st.shadowBlur > 0) {
    // coverage via fillPolys into scratch mask: approximate by re-running the
    // span loop is complex; instead stamp bbox with winding test.
    F64 gMinX = 1e30, gMinY = 1e30, gMaxX = -1e30, gMaxY = -1e30;
    for (auto& L : dev)
      for (auto& p : L.pts) {
        if (p.x < gMinX) gMinX = p.x;
        if (p.x > gMaxX) gMaxX = p.x;
        if (p.y < gMinY) gMinY = p.y;
        if (p.y > gMaxY) gMaxY = p.y;
      }
    int x0 = std::max(0, (int)std::floor(gMinX) - 32);
    int y0 = std::max(0, (int)std::floor(gMinY) - 32);
    int x1 = std::min(w_, (int)std::ceil(gMaxX) + 32);
    int y1 = std::min(h_, (int)std::ceil(gMaxY) + 32);
    int bw = x1 - x0, bh = y1 - y0;
    if (bw > 0 && bh > 0) {
      scratchA_.assign((size_t)bw * (size_t)bh, 0);
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          if (!insideClips((F64)x + 0.5, (F64)y + 0.5)) continue;
          if (winding(dev, (F64)x + 0.5, (F64)y + 0.5) != 0)
            scratchA_[(size_t)(y - y0) * (size_t)bw + (size_t)(x - x0)] = 255;
        }
      int R = std::max(1, (int)(st.shadowBlur * 0.75));
      scratchB_.assign((size_t)bw * (size_t)bh, 0);
      for (int pass = 0; pass < 2; pass++) { // separable box blur x2
        for (int y = 0; y < bh; y++) {
          int acc = 0;
          for (int x = -R; x < bw + R; x++) {
            int xa = std::min(bw - 1, std::max(0, x + R));
            int xb = std::min(bw - 1, std::max(0, x - R - 1));
            if (pass == 0) {
              acc += scratchA_[(size_t)y * (size_t)bw + (size_t)xa];
              if (x - R - 1 >= -R) acc -= scratchA_[(size_t)y * (size_t)bw + (size_t)xb];
              if (x >= 0 && x < bw)
                scratchB_[(size_t)y * (size_t)bw + (size_t)x] = (uint8_t)(acc / (2 * R + 1));
            } else {
              acc += scratchB_[(size_t)y * (size_t)bw + (size_t)xa];
              if (x - R - 1 >= -R) acc -= scratchB_[(size_t)y * (size_t)bw + (size_t)xb];
              if (x >= 0 && x < bw)
                scratchA_[(size_t)y * (size_t)bw + (size_t)x] = (uint8_t)(acc / (2 * R + 1));
            }
          }
        }
        // transpose-free vertical pass: reuse same loop on swapped roles
        std::vector<uint8_t> tmp((size_t)bw * (size_t)bh);
        const std::vector<uint8_t>& src = (pass == 0) ? scratchB_ : scratchA_;
        for (int x = 0; x < bw; x++) {
          int acc = 0;
          for (int y = -R; y < bh + R; y++) {
            int ya = std::min(bh - 1, std::max(0, y + R));
            int yb = std::min(bh - 1, std::max(0, y - R - 1));
            acc += src[(size_t)ya * (size_t)bw + (size_t)x];
            if (y - R - 1 >= -R) acc -= src[(size_t)yb * (size_t)bw + (size_t)x];
            if (y >= 0 && y < bh)
              tmp[(size_t)y * (size_t)bw + (size_t)x] = (uint8_t)(acc / (2 * R + 1));
          }
        }
        if (pass == 0) scratchB_.swap(tmp);
        else scratchA_.swap(tmp);
      }
      CompOp savedComp = stack_.back().comp;
      stack_.back().comp = CompOp::SrcOver;
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          uint8_t cov = scratchA_[(size_t)(y - y0) * (size_t)bw + (size_t)(x - x0)];
          if (!cov) continue;
          ColorF c = st.shadow;
          c.a *= cov / 255.0f;
          blendPixel(x, y, c);
        }
      stack_.back().comp = savedComp;
    }
  }
  fillPolys(dev, fillPaint_);
}

void Canvas::strokePolys(const std::vector<DevPath>& dev, const FillPaint& paint) {
  const State& st = stack_.back();
  F64 sc = (std::hypot(st.ctm.a, st.ctm.b) + std::hypot(st.ctm.c, st.ctm.d)) * 0.5;
  if (!(sc > 0)) sc = 1;
  F64 hw = st.lineW * sc * 0.5;
  if (!(hw > 0)) return;
  bool roundCap = (st.cap == LineCap::Round);
  bool squareCap = (st.cap == LineCap::Square);
  bool roundJoin = (st.join == LineJoin::Round);

  const Mat& mc = st.ctm;
  F64 det = mc.a * mc.d - mc.b * mc.c;
  bool hasInv = std::fabs(det) > 1e-12;
  F64 ia = 0, ib = 0, ic = 0, id = 0, ie = 0, iff = 0;
  if (hasInv) {
    ia = mc.d / det;
    ib = -mc.b / det;
    ic = -mc.c / det;
    id = mc.a / det;
    ie = (mc.c * mc.f - mc.d * mc.e) / det;
    iff = (mc.b * mc.e - mc.a * mc.f) / det;
  }
  bool needUser = paint.isGrad && paint.grad && hasInv;

  for (const auto& L : dev) {
    size_t n = L.pts.size();
    if (n < 2) continue;
    F64 gMinX = 1e30, gMinY = 1e30, gMaxX = -1e30, gMaxY = -1e30;
    for (auto& p : L.pts) {
      if (p.x < gMinX) gMinX = p.x;
      if (p.x > gMaxX) gMaxX = p.x;
      if (p.y < gMinY) gMinY = p.y;
      if (p.y > gMaxY) gMaxY = p.y;
    }
    F64 pad = hw + 1;
    int x0 = std::max(0, (int)std::floor(gMinX - pad));
    int y0 = std::max(0, (int)std::floor(gMinY - pad));
    int x1 = std::min(w_, (int)std::ceil(gMaxX + pad));
    int y1 = std::min(h_, (int)std::ceil(gMaxY + pad));
    size_t segs = L.closed ? n : n - 1;
    for (int y = y0; y < y1; y++) {
      for (int x = x0; x < x1; x++) {
        F64 px = (F64)x + 0.5, py = (F64)y + 0.5;
        bool hit = false;
        for (size_t i = 0; i < segs && !hit; i++) {
          const Pt& a = L.pts[i];
          const Pt& b = L.pts[(i + 1) % n];
          F64 abx = b.x - a.x, aby = b.y - a.y;
          F64 len2 = abx * abx + aby * aby;
          if (len2 < 1e-12) {
            F64 dx = px - a.x, dy = py - a.y;
            if (dx * dx + dy * dy <= hw * hw) hit = true;
            continue;
          }
          F64 len = std::sqrt(len2);
          F64 t = ((px - a.x) * abx + (py - a.y) * aby) / len2;
          F64 tlo = 0, thi = 1;
          if (!L.closed) {
            if (squareCap) {
              tlo = -hw / len;
              thi = 1 + hw / len;
            } else if (!roundCap) {
              tlo = 0;
              thi = 1;
            }
          }
          if (roundCap || roundJoin) {
            F64 tc = t < 0 ? 0 : (t > 1 ? 1 : t);
            F64 dx = px - (a.x + abx * tc), dy = py - (a.y + aby * tc);
            if (dx * dx + dy * dy <= hw * hw) hit = true;
          } else if (t >= tlo && t <= thi) {
            F64 cx = a.x + abx * (t < 0 ? 0 : (t > 1 ? 1 : t)), cy = a.y + aby * (t < 0 ? 0 : (t > 1 ? 1 : t));
            F64 dx = px - cx, dy = py - cy;
            if (dx * dx + dy * dy <= hw * hw) hit = true;
          }
        }
        if (!hit) continue;
        if (!insideClips(px, py)) continue;
        ColorF c;
        if (needUser) {
          c = resolvePaint(paint, ia * px + ic * py + ie, ib * px + id * py + iff);
        } else {
          c = paint.isGrad ? paint.color : paint.color;
          c.a *= (float)st.alpha;
        }
        blendPixel(x, y, c);
      }
    }
  }
}

void Canvas::stroke() {
  auto dev = devicePaths();
  if (dev.empty()) return;
  // shadow pass for strokes: reuse coverage approach (simple, rare in game)
  strokePolys(dev, strokePaint_);
}

// ---- frame ----
void Canvas::clearAll() {
  std::fill(fb_.begin(), fb_.end(), 0u);
  clips_.clear();
  clipDepth_.clear();
  stack_.clear();
  stack_.push_back(State());
  path_.clear();
  fillPaint_ = FillPaint();
  strokePaint_ = FillPaint();
}

uint64_t Canvas::hash() const { return fnv1a(fb_.data(), fb_.size() * 4); }

bool Canvas::writePPM(const char* path) const {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w_, h_);
  for (int y = 0; y < h_; y++) {
    for (int x = 0; x < w_; x++) {
      uint32_t p = fb_[(size_t)y * (size_t)w_ + (size_t)x];
      uint8_t rgb[3] = {(uint8_t)(p & 255), (uint8_t)((p >> 8) & 255),
                        (uint8_t)((p >> 16) & 255)};
      // un-premultiplied storage already; composite over black for preview
      float a = ((p >> 24) & 255) / 255.0f;
      rgb[0] = (uint8_t)(rgb[0] * a);
      rgb[1] = (uint8_t)(rgb[1] * a);
      rgb[2] = (uint8_t)(rgb[2] * a);
      if (std::fwrite(rgb, 1, 3, f) != 3) {
        std::fclose(f);
        return false;
      }
    }
  }
  std::fclose(f);
  return true;
}

} // namespace ag
