// Afterglow native — ag_skin.h
// 1:1 port surface of js/skin.js: tone derivation + shared primitives
// (limbs, hands, feet, breasts, vulva, hair) used by side.js & fpv.js.
#pragma once
#include "ag_canvas.h"
#include "ag_core.h"
#include "ag_gfx.h"

#include <functional>

namespace ag {

// js/skin.js:13-19 — 4-stop tone ramp + her/him derivation.
struct Tone {
  std::string hi, b, s, d, dk;
  std::string base, sh;
};
Tone skTone(std::string_view base);
// getSkin() lives in the chars module (js/chars.js); declared here for herT.
struct SkinPair {
  std::string her, herSh, herDk, him, himSh;
};
Tone herT(const Game& g, const SkinPair& sk);
Tone himT(const Game& g, const SkinPair& sk);

// js/skin.js:29-55 — shape fill + form shading primitives.
using PathFn = std::function<void(Canvas&)>;
void skFillShape(Canvas& cv, PathFn fn, const Tone& T, const F64 lg[4]);
void skFillRad(Canvas& cv, PathFn fn, const Tone& T, F64 cx, F64 cy, F64 r0,
               F64 r1);
void skClipIn(Canvas& cv, PathFn fn, std::function<void()> inner);
void fSh(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
         F64 rot = 0);
void fHi(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, std::string_view col,
         F64 rot = 0);
void fSSS(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, F64 rot = 0);
void fAO(Canvas& cv, F64 x, F64 y, F64 rx, F64 ry, F64 a, F64 rot = 0);
void skLine(Canvas& cv, PathFn fn, const Tone& T, F64 w = 1.4,
            F64 a = 0.4);

// js/skin.js:61-96 — organic tapered limb.
struct LimbOpt {
  F64 bow = 0, belly = 1.06;
  bool hasLit = false;
  F64 litX = -0.62, litY = -0.78; // SK_LIT default
  F64 aoA = 0, aoB = 0;
  bool line = true;
};
void limbS(Canvas& cv, V2 a, V2 b, F64 r1, F64 r2, const Tone& T,
           const LimbOpt& opt = LimbOpt{});

// js/skin.js:101+ — ported incrementally (signatures mirror JS exactly).
struct HandOpt {
  F64 curl = 0.35, spread = 0.30;
};
void handS(Canvas& cv, Game& g, F64 x, F64 y, F64 ang, F64 s, const Tone& T,
           const HandOpt& opt = HandOpt{}); // js/skin.js:101
void footS(Canvas& cv, F64 x, F64 y, F64 ang, F64 s,
           const Tone& T); // js/skin.js:143
struct BreastOpt {
  F64 jig = 0;
};
void breastS(Canvas& cv, Game& g, F64 cx, F64 cy, F64 r, F64 ang,
             const Tone& T, const BreastOpt& opt = BreastOpt{}); // :165
struct VulvaOpt {
  std::string view = "side"; // 'side' | 'front'
  // Game-carried state (js reads G.ar / G.char inside vulvaS). The function
  // signature is unchanged; callers that own a Game forward g.ar / g.ch here.
  // Defaults match the JS no-G fallbacks (no wet specular, trim, dark hair).
  F64 ar = 0;                        // G.ar — wet specular sheen when > 22
  std::string pubicHair = "trim";    // G.char.pubicHair
  std::string hairColor = "#231318"; // G.char.hairColor
};
void vulvaS(Canvas& cv, F64 cx, F64 cy, F64 open, F64 eng, const Tone& T,
            const VulvaOpt& opt = VulvaOpt{}); // js/skin.js:206

// js/skin.js:279 — neck + trapezius column (skull to shoulder line).
struct NeckOpt {
  bool line = true;
};
void neckS(Canvas& cv, V2 top, V2 base, F64 w, const Tone& T,
           const NeckOpt& opt = NeckOpt{}); // js/skin.js:279

// js/skin.js:308 — silky tapered ribbon tress; empty sheen = no sheen pass.
void tressS(Canvas& cv, F64 x0, F64 y0, F64 c1x, F64 c1y, F64 c2x, F64 c2y,
            F64 x1, F64 y1, F64 w0, F64 w1, std::string_view col,
            std::string_view sheen = {}); // js/skin.js:308

// js/skin.js:335 — rounded scalp / hair mass with soft top light.
void hairMassS(Canvas& cv, F64 cx, F64 cy, F64 rx, F64 ry, F64 rot,
               std::string_view col); // js/skin.js:335

} // namespace ag
