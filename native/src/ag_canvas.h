// Afterglow native — ag_canvas.h
// Software Canvas2D rasterizer with HTML5 semantics, fixed 1280x720.
// Covers exactly the subset js/* uses (audited 2026-09):
//   state: save/restore, setTransform/getTransform, translate/scale/rotate,
//          globalAlpha, globalCompositeOperation(source-over|multiply|screen|
//          soft-light), lineWidth/lineCap/lineJoin, shadowColor/shadowBlur
//   paths: beginPath/closePath, moveTo/lineTo, bezierCurveTo,
//          quadraticCurveTo, arc, ellipse, rect, fill, stroke, clip,
//          fillRect/strokeRect
//   paint: CSS color strings, createLinearGradient/createRadialGradient +
//          addColorStop (exact-key, transform-aware cache — port of the
//          js/gfx.js _gcache that killed the GC spikes)
// Deliberately NOT implemented (unused by the game): drawImage, fillText,
// createPattern, setLineDash, filters. They are stubbed to no-op.
#pragma once
#include "ag_prelude.h"

#include <span>

namespace ag {

enum class CompOp { SrcOver, Multiply, Screen, SoftLight };
enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };

struct Mat {
  F64 a = 1, b = 0, c = 0, d = 1, e = 0, f = 0; // [a c e; b d f; 0 0 1]
};

struct GradStop {
  F64 off = 0;
  ColorF col{};
};

struct Gradient {
  bool radial = false;
  // linear: (x0,y0)->(x1,y1). radial: (x0,y0,r0)->(x1,y1,r1). User space.
  F64 x0 = 0, y0 = 0, r0 = 0, x1 = 0, y1 = 0, r1 = 0;
  std::vector<GradStop> stops;
  ColorF sample(F64 ux, F64 uy) const; // sRGB interpolation like canvas
};
using GradPtr = std::shared_ptr<const Gradient>;

class Canvas {
public:
  Canvas(int w = VW, int h = VH);
  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  int w() const { return w_; }
  int h() const { return h_; }

  // ---- state ----
  void save();
  void restore();
  void setTransform(F64 a, F64 b, F64 c, F64 d, F64 e, F64 f);
  Mat getTransform() const;
  void translate(F64 x, F64 y);
  void scale(F64 sx, F64 sy);
  void rotate(F64 ang);
  void setGlobalAlpha(F64 a);
  void setComp(CompOp op);
  void setCompStr(std::string_view op); // 'multiply' | 'screen' | 'soft-light' | ...
  void setLineWidth(F64 w);
  void setLineCap(LineCap c);
  void setLineCapStr(std::string_view s); // 'butt' | 'round' | 'square'
  void setLineJoin(LineJoin j);
  void setShadow(ColorF c, F64 blur);
  void setShadowStr(std::string_view cssColor, F64 blur);
  void clearShadow();

  // ---- styles ----
  void setFillColor(ColorF c);
  void setFillColorStr(std::string_view s);
  void setStrokeColor(ColorF c);
  void setStrokeColorStr(std::string_view s);
  void setFillGrad(GradPtr g);
  void setStrokeGrad(GradPtr g);

  // ---- gradients (cached; key = transform + geometry + stops, exact bits) ----
  GradPtr makeLinear(F64 x0, F64 y0, F64 x1, F64 y1,
                     std::span<const GradStop> stops);
  GradPtr makeRadial(F64 x0, F64 y0, F64 r0, F64 x1, F64 y1, F64 r1,
                     std::span<const GradStop> stops);
  size_t cacheSize() const;
  void cacheClear();

  // ---- paths ----
  void beginPath();
  void closePath();
  void moveTo(F64 x, F64 y);
  void lineTo(F64 x, F64 y);
  void bezierCurveTo(F64 c1x, F64 c1y, F64 c2x, F64 c2y, F64 x, F64 y);
  void quadraticCurveTo(F64 cx, F64 cy, F64 x, F64 y);
  void arc(F64 x, F64 y, F64 r, F64 a0, F64 a1, bool ccw = false);
  void ellipse(F64 x, F64 y, F64 rx, F64 ry, F64 rot, F64 a0, F64 a1,
               bool ccw = false);
  void rect(F64 x, F64 y, F64 w, F64 h);
  void fill();
  void stroke();
  void clip();
  void fillRect(F64 x, F64 y, F64 w, F64 h);
  void strokeRect(F64 x, F64 y, F64 w, F64 h);

  // ---- frame ----
  void clearAll(); // transparent black, reset clip stack depth kept
  const uint32_t* pixels() const { return fb_.data(); } // packed RGBA8
  uint64_t hash() const;                                // FNV-1a over pixels
  bool writePPM(const char* path) const;                // P6, for CI review

private:
  struct Pt {
    F64 x = 0, y = 0;
  };
  struct SubPath {
    std::vector<Pt> pts; // user space, flattened
    bool closed = false;
  };
  struct DevPath {
    std::vector<Pt> pts; // device space
    bool closed = false;
  };
  struct State {
    Mat ctm;
    F64 alpha = 1;
    CompOp comp = CompOp::SrcOver;
    F64 lineW = 1;
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    ColorF shadow{0, 0, 0, 0};
    F64 shadowBlur = 0;
  };

  int w_, h_;
  std::vector<uint32_t> fb_;
  std::vector<State> stack_;
  std::vector<SubPath> path_;
  std::vector<std::vector<DevPath>> clips_; // one entry per clip()

  struct FillPaint {
    bool isGrad = false;
    ColorF color{0, 0, 0, 1};
    GradPtr grad;
  };
  FillPaint fillPaint_, strokePaint_;

  // gradient cache: exact-bit key incl. CTM (port of js/gfx.js _gcache)
  struct GKey {
    bool radial = false;
    int n = 0; // used doubles in bits (geom + 6 CTM)
    uint64_t bits[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::string stops; // off + rgba bytes
    bool operator==(const GKey& o) const {
      if (radial != o.radial || n != o.n || stops != o.stops) return false;
      for (int i = 0; i < n; i++)
        if (bits[i] != o.bits[i]) return false;
      return true;
    }
  };
  struct GKeyHash {
    size_t operator()(const GKey& k) const noexcept;
  };
  std::unordered_map<GKey, GradPtr, GKeyHash> gcache_;
  static constexpr size_t GCAP = 4096;

  // raster helpers
  void blendPixel(int x, int y, const ColorF& src);
  Pt applyCtm(Pt p) const;
  static void flatCubic(std::vector<Pt>& out, F64 x0, F64 y0, F64 x1, F64 y1,
                        F64 x2, F64 y2, F64 x3, F64 y3, int depth);
  static int winding(const std::vector<DevPath>& loops, F64 x, F64 y);
  static void keyDoubles(GKey& k, const F64* v, int n);
  static void keyStops(GKey& k, std::span<const GradStop> stops);
  std::vector<DevPath> devicePaths() const;
  bool insideClips(F64 dx, F64 dy) const;
  void fillPolys(const std::vector<DevPath>& dev, const FillPaint& paint);
  void strokePolys(const std::vector<DevPath>& dev, const FillPaint& paint);
  ColorF resolvePaint(const FillPaint& paint, F64 ux, F64 uy) const;

  std::vector<size_t> clipDepth_; // clips_.size() snapshot per save() level
  std::vector<uint8_t> scratchA_, scratchB_; // shadow/blur reuse buffers
};

} // namespace ag
