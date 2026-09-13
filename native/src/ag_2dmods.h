// Afterglow native — ag_2dmods.h
// API surface of the remaining 2D modules (1:1 with js/*.js).
// Fully ported: core, gfx, skin(tones+limbS). The rest expose their JS entry
// points here; bodies live in ag_port_todo.cpp until ported module by module
// (see PORT_STATUS table there). No behavioral guesses: signatures mirror JS.
#pragma once
#include "ag_canvas.h"
#include "ag_core.h"
#include "ag_skin.h"

namespace ag {

// ---- js/chars.js ----
namespace chars {
// presets + getSkin() (SkinPair declared in ag_skin.h)
SkinPair getSkin(const Game& g);
void applyPreset(Game& g, const std::string& name); // js/chars.js preset table
} // namespace chars

// ---- js/input.js ----
namespace input {
enum class Key : int {
  Space = 0, E, C, Tab, O, S, X, V, Z, P, M, F, T, Q, L, R, Digit1, Digit2,
  Digit3, Digit4, Unknown
};
struct Pointer {
  F64 x = 0, y = 0; // virtual 1280x720 coords
  bool down = false;
};
void keyDown(Game& g, Key k);
void keyUp(Game& g, Key k);
void pointerDown(Game& g, const Pointer& p);
void pointerMove(Game& g, const Pointer& p);
void pointerUp(Game& g, const Pointer& p);
void update(Game& g, F64 dt); // drag dynamics, hold states
} // namespace input

// ---- js/mechanics.js ----
namespace mech {
// Per-frame sim: thrust physics, pleasure/stamina/combo/orgasm/climax,
// particles (jets/drips/sweat/hearts/glisten), springs, moans scheduling.
void update(Game& g, Rng& rng, F64 dt);
F64 sweetBandLo(const Game& g); // her tempo sweet band (poses/band cfg)
F64 sweetBandHi(const Game& g);
F64 hotLo(const Game& g); // her spot zone on depth gauge
F64 hotHi(const Game& g);
} // namespace mech

// ---- js/side.js ----
namespace side {
void draw(Canvas& cv, Game& g); // full side-view scene pass
void herExpression(Canvas& cv, Game& g); // shared with 3D mode
} // namespace side

// ---- js/fpv.js ----
namespace fpv {
void visualReset(); // reseed draw-time visual RNG (parity harness)
void draw(Canvas& cv, Game& g); // full first-person pass
} // namespace fpv

// ---- js/poses.js (7 poses) ----
namespace poses {
inline constexpr int COUNT = 7;
void cycle(Game& g); // Tab
std::string name(const Game& g); // 'MISSIONARY' etc for HUD button
void drawBodies(Canvas& cv, Game& g); // pose-driven her/him rendering
} // namespace poses

// ---- js/oral.js ----
namespace oral {
void toggle(Game& g); // O key
void visualReset();    // reseed draw-time visual RNG (parity harness)
void draw(Canvas& cv, Game& g); // oral-mode scene pass
} // namespace oral

// ---- js/charui.js (customize overlay P) ----
namespace charui {
bool visible(const Game& g);
void toggle(Game& g);
void draw(Canvas& cv, Game& g);
bool tap(Game& g, F64 x, F64 y);
} // namespace charui

// ---- js/audio.js ----
namespace audio {
void init(const char* assetDir); // decode banks (ogg/wav), null-safe w/o dir
void shutdown();
void setMuted(bool m);
void update(Game& g, F64 dt); // schedules moans/strokes/climax per sim
void playOnce(const std::string& id, F64 gain = 1.0);
// Platform pull (audio thread): additive stereo f32 @rate.
void mixInto(float* out, int frames, int ch, int rate);
} // namespace audio

// ---- js/app.js ----
namespace app {
// Mirrors draw(): setTransform fit + route side/fpv + drawLight.
void draw(Canvas& cv, Game& g);
// One fixed step: input.update + mech.update + audio.update.
void step(Game& g, Rng& rng, F64 dt);
} // namespace app

} // namespace ag
