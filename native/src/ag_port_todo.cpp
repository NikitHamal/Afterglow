// Afterglow native — ag_port_todo.cpp
// Stub bodies for not-yet-ported modules + the master port-status table.
// Every stub is side-effect-free and clearly marked; the game boots and the
// parity harness runs on (core+gfx+skin+room/bed/light) while the remaining
// scene modules land one by one. Rule: port in this order —
//   mech -> input -> poses -> side -> fpv -> oral -> chars -> charui -> audio
// because each later module only calls earlier ones (+gfx/skin).
#include "ag_2dmods.h"

namespace ag {

// PORT_STATUS: js symbol -> C++ home -> state.
//   DONE: core.js, gfx.js (hp/sp/chaos/sg/shade/capsule/heart/room/bed/
//         fluids/light), skin.js (tones, skFill*, fSh/fHi/fSSS/fAO, skLine,
//         limbS), chars.js hexToRgb/lerpHex.
//   TODO: side.js draw(375L), fpv.js draw(988L), poses.js(497L),
//         oral.js(636L), mechanics.js(641L), input.js(179L), chars.js presets,
//         charui.js(396L), audio.js timing(471L), skin.js handS/footS/
//         breastS/vulvaS/hair (need js/skin.js:221-340), 3D: engine3d/
//         chars3d/poses3d/anim3d/fx3d/glbModel/app3d.

namespace chars {
SkinPair getSkin(const Game& g) {
  (void)g;
  return {"#f2c9b3", "#d99a86", "#a86a5c", "#e8b89a", "#b08068"};
}
void applyPreset(Game& g, const std::string& name) {
  (void)g;
  (void)name;
}
} // namespace chars

namespace input {
void keyDown(Game& g, Key k) {
  (void)g;
  (void)k;
}
void keyUp(Game& g, Key k) {
  (void)g;
  (void)k;
}
void pointerDown(Game& g, const Pointer& p) {
  (void)g;
  (void)p;
}
void pointerMove(Game& g, const Pointer& p) {
  (void)g;
  (void)p;
}
void pointerUp(Game& g, const Pointer& p) {
  (void)g;
  (void)p;
}
void update(Game& g, F64 dt) {
  (void)g;
  (void)dt;
}
} // namespace input

namespace mech {
void update(Game& g, Rng& rng, F64 dt) {
  // Minimal time advance so headless frames differ deterministically;
  // full sim in the mechanics port.
  g.t += dt;
  g.sesT += dt;
  (void)rng;
}
F64 sweetBandLo(const Game& g) {
  (void)g;
  return CFG.band0;
}
F64 sweetBandHi(const Game& g) {
  (void)g;
  return CFG.band1;
}
F64 hotLo(const Game& g) { return g.hotC - CFG.hotW; }
F64 hotHi(const Game& g) { return g.hotC + CFG.hotW; }
} // namespace mech

namespace side {
void draw(Canvas& cv, Game& g) {
  // Placeholder until side.js port: room+bed so framing/parity is testable.
  drawRoom(cv, g);
  drawBed(cv, g);
}
void herExpression(Canvas& cv, Game& g) {
  (void)cv;
  (void)g;
}
} // namespace side

namespace fpv {
void draw(Canvas& cv, Game& g) {
  drawRoom(cv, g);
  drawBed(cv, g);
}
} // namespace fpv

namespace poses {
void cycle(Game& g) {
  g.pos = (g.pos + 1) % COUNT;
  (void)g.pos;
}
std::string name(const Game& g) {
  static const char* N[7] = {"MISSIONARY", "LEGS-UP",   "DOGGY",  "PRONE",
                             "COWGIRL",    "REV-COWG", "SPOON"};
  return N[g.pos < 0 ? 0 : (g.pos > 6 ? 6 : g.pos)];
}
void drawBodies(Canvas& cv, Game& g) {
  (void)cv;
  (void)g;
}
} // namespace poses

namespace oral {
void toggle(Game& g) { g.oral = g.oral > 0 ? 0 : 1; }
void draw(Canvas& cv, Game& g) {
  drawRoom(cv, g);
  drawBed(cv, g);
}
} // namespace oral

namespace charui {
bool visible(const Game&) { return false; }
void toggle(Game&) {}
void draw(Canvas&, Game&) {}
bool tap(Game&, F64, F64) { return false; }
} // namespace charui

// handS/footS/breastS/vulvaS declared in ag_skin.h; need js/skin.js:221-340.
void handS(Canvas& cv, Game& g, F64 x, F64 y, F64 ang, F64 s, const Tone& T,
           const HandOpt& opt) {
  (void)cv;
  (void)g;
  (void)x;
  (void)y;
  (void)ang;
  (void)s;
  (void)T;
  (void)opt;
}
void footS(Canvas& cv, F64 x, F64 y, F64 ang, F64 s, const Tone& T) {
  (void)cv;
  (void)x;
  (void)y;
  (void)ang;
  (void)s;
  (void)T;
}
void breastS(Canvas& cv, Game& g, F64 cx, F64 cy, F64 r, F64 ang,
             const Tone& T, const BreastOpt& opt) {
  (void)cv;
  (void)g;
  (void)cx;
  (void)cy;
  (void)r;
  (void)ang;
  (void)T;
  (void)opt;
}
void vulvaS(Canvas& cv, F64 cx, F64 cy, F64 open, F64 eng, const Tone& T,
            const VulvaOpt& opt) {
  (void)cv;
  (void)cx;
  (void)cy;
  (void)open;
  (void)eng;
  (void)T;
  (void)opt;
}

namespace app {
void draw(Canvas& cv, Game& g) {
  // js/app.js: setTransform fit + route + drawLight.
  cv.setTransform((F64)cv.w() / VW, 0, 0, (F64)cv.h() / VH, 0, 0);
  if (g.view == "fpv") fpv::draw(cv, g);
  else if (g.oral > 0) oral::draw(cv, g);
  else side::draw(cv, g);
  if (!charui::visible(g)) drawLight(cv, g);
  drawFluids(cv, g);
}
void step(Game& g, Rng& rng, F64 dt) {
  input::update(g, dt);
  mech::update(g, rng, dt);
  audio::update(g, dt);
}
} // namespace app

} // namespace ag
