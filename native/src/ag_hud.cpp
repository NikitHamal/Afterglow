// Afterglow native — ag_hud.cpp (geometry mirrors css/app.css).
#include "ag_hud.h"
#include "ag_2dmods.h"

#include <cstdio>

#include <cstdio>

namespace ag::hud {

HudFrame build(const Game& g, int sw, int sh) {
  HudFrame f;
  F64 k = sh / 720.0; // css px scale
  auto rect = [&](F64 x, F64 y, F64 w, F64 h, ColorF c, bool o = false) {
    f.rects.push_back({x * k, y * k, w * k, h * k, c, o});
  };
  auto text = [&](F64 x, F64 y, F64 s, std::string t, bool title,
                  ColorF c = {1, 1, 1, 1}) {
    f.texts.push_back({x * k, y * k, s * k, std::move(t), title, c});
  };
  ColorF rose = mustColor("#f6dfe6"), dim = mustColor("#c9a0ac");

  // pleasure bar (left, vertical) — plFill height = pleasure%
  rect(36, 170, 14, 380, {1, 1, 1, 0.10});
  F64 ph = 380 * clamp(g.pleasure / 100.0, 0.0, 1.0);
  rect(36, 170 + 380 - ph, 14, ph, mustColor("#ff5f86"));
  text(20, 150, 15, "YUKI", false, rose);
  text(20, 560, 13, moodWord(g), true, rose);

  // depth gauge (right) + hot zone + marker
  rect(1228, 170, 14, 380, {1, 1, 1, 0.10});
  F64 dh = 380 * clamp(g.depth, 0.0, 1.0);
  rect(1228, 170 + 380 - dh, 14, dh, mustColor("#7fd4ff"));
  F64 hz0 = 170 + 380 * (1 - mech::hotHi(g)), hz1 = 170 + 380 * (1 - mech::hotLo(g));
  rect(1226, hz0, 18, hz1 - hz0, mustColor("#ff4f8a"));
  text(1200, 150, 12, "depth", false, dim);

  // stamina + rhythm (bottom-left cluster)
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%d", (int)g.stamina);
  text(36, 600, 12, "stamina", false, dim);
  rect(36, 616, 180, 8, {1, 1, 1, 0.12});
  rect(36, 616, 180 * clamp(g.stamina / 100.0, 0.0, 1.0), 8,
       mustColor("#9fe870"));
  text(220, 616, 12, buf, false, rose);
  std::snprintf(buf, sizeof(buf), "x%.1f", g.combo);
  text(36, 636, 12, std::string("rhythm ") + buf, false, dim);

  // top bar: clock + strokes
  int secs = (int)g.sesT;
  std::snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
  text(600, 30, 16, buf, false, rose);
  std::snprintf(buf, sizeof(buf), "%d strokes", (int)g.strokes);
  text(660, 30, 13, buf, false, dim);

  // action buttons (bottom center) — labels from index.html#actWrap
  std::string posName = poses::name(g);
  const char* acts[6] = {"EMBRACE [E]", "RUB HER [C]", posName.c_str(),
                         "ORAL [O]",    "SOLO [S]",   "CLIMAX [X]"};
  for (int i = 0; i < 6; i++)
    text(430 + i * 90, 690, 12, acts[i], false, rose);

  (void)sw;
  return f;
}

} // namespace ag::hud
