// Afterglow native — ag_hud.h
// HUD frame builder. The web HUD is DOM/CSS (index.html); here the scene
// Canvas stays pure game art and this module emits a screen-space draw list
// the platform renders on top (bars as rects, labels via cached TTF glyphs).
// Geometry mirrors css/app.css layout proportionally at any window size.
#pragma once
#include "ag_core.h"

namespace ag::hud {

struct HudRect {
  F64 x = 0, y = 0, w = 0, h = 0; // screen px
  ColorF fill{1, 1, 1, 1};
  bool outline = false;
};
struct HudText {
  F64 x = 0, y = 0, size = 14; // screen px, top-left baseline-ish
  std::string text;
  bool title = false; // Cormorant vs Sora
  ColorF color{1, 1, 1, 1};
};
struct HudFrame {
  std::vector<HudRect> rects;
  std::vector<HudText> texts;
};

// Moods: exact word mapping lives in mechanics/charui — TODO audit;
// thresholds below are structural placeholders, flagged, not behavior.
inline std::string moodWord(const Game& g) {
  if (g.pleasure > 85) return "radiant";
  if (g.pleasure > 60) return "aching";
  if (g.pleasure > 35) return "warm";
  return "snug";
}

HudFrame build(const Game& g, int screenW, int screenH);

} // namespace ag::hud
