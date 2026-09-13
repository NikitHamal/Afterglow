// Afterglow native — ag_gfx.h
// 1:1 port surface of js/gfx.js: shear helpers, cached gradients, shade,
// volumetric capsule, heart path, room/bed/fluids/light passes.
// Every function documents the js/gfx.js line range it mirrors so the
// incremental port stays auditable.
#pragma once
#include "ag_canvas.h"
#include "ag_core.h"

namespace ag {

struct V2 {
  F64 x = 0, y = 0;
};

// js/gfx.js:8-20 — thrust shear helpers (depend on G.depth / G.impact).
V2 hp(const Game& g, F64 x, F64 y, F64 f);
V2 sp(const Game& g, F64 x, F64 y, F64 f);
F64 chaos(const Game& g, F64 x);

// js/gfx.js:48-61 — cached gradient constructors (exact-key, CTM-aware).
// Stops as (offset, css-color) pairs; colors parsed once per unique key.
using StopStr = std::pair<F64, std::string>;
GradPtr gLinear(Canvas& cv, F64 x0, F64 y0, F64 x1, F64 y1,
                const std::vector<StopStr>& stops);
GradPtr gRadial(Canvas& cv, F64 x0, F64 y0, F64 r0, F64 x1, F64 y1, F64 r1,
                const std::vector<StopStr>& stops);

// js/gfx.js:66-83 — vertical 2-stop helper + soft radial blob.
GradPtr sg(Canvas& cv, F64 y0, F64 y1, std::string_view c1, std::string_view c2);
void shade(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
           F64 rot = 0, std::string_view lc = {});

// js/gfx.js:89-155 — volumetric capsule (currently 0 calls in side/fpv views,
// kept for parity; routes its 3 internal gradients through the cache).
void capsule(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, ColorF flat, F64 bob = 0);
void capsuleGrad(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, GradPtr fill,
                 F64 bob = 0);

// js/gfx.js:157-163
void heartPath(Canvas& cv, F64 x, F64 y, F64 s);

// js/gfx.js:168-283 / 285-370 / 375-474 / 476-533 — environment passes.
void drawRoom(Canvas& cv, Game& g);
void drawBed(Canvas& cv, Game& g);
void drawFluids(Canvas& cv, Game& g);
void drawLight(Canvas& cv, Game& g);

} // namespace ag
