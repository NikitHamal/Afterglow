// Afterglow native — ag_input.cpp
// Port of js/input.js (179L): keyboard + pointer -> Game state.
//
// Pointer input arrives in virtual 1280x720 coords (platform layer scales
// OS pixels; ag::VW/ag::VH). Drag gain mirrors js/app.js resize():
//   dragK = 1/(w*CFG.dragSens)   ("fraction of screen-width = full depth")
// with w = VW in virtual space, applied to the vertical drag delta exactly
// like js/input.js pmove():  target = clamp(target + dy*dragK, 0, 1).
// CFG.strokeMinRange is consumed by the stroke detector in mechanics.js,
// not here (see summary).
//
// Delegated (declared in ag_2dmods.h): poses::cycle (Tab), oral::toggle (O),
// charui::toggle/tap (P / pointer routing), audio::setMuted (M).
// Everything else is direct Game field writes. Speech/moans/audio one-shots
// (say/pick/playMoan/...) have no C++ API yet -> field effects only + TODOs.
//
// NOTE: ag_port_todo.cpp still holds side-effect-free input:: stubs; delete
// that stub block and list this file in CMakeLists add_library(afterglow_core)
// or the link will see duplicate definitions.
#include "ag_2dmods.h"

#include <cmath>

namespace ag::input {
namespace {

// ---- file-local module state (JS module lets / dynamic props) ----
F64 s_dragY = 0.0;   // JS: let dragY (clientY anchor while dragging)
bool s_hasDrag = false; // JS: dragY===null test
// TODO(core.h): persist last-rub-tap on Game (JS: G._lastRubTap, wall clock).
// Uses sim clock g.t so headless parity frames stay deterministic.
F64 s_lastRubTap = -10.0; // JS: G._lastRubTap (performance.now, 350ms window)
bool s_spaceDown = false; // JS: e.repeat guard for Space
// TODO(core.h/audio): persist mute on Game/audio (JS: global muted+lsSet).
// No isMuted() query exists yet; platform should seed via setMuted hook.
bool s_muted = false; // JS: global muted toggled by M key / mute button

bool isPlayLike(const Game& g) {
  return g.state == "play" || g.state == "orgasm";
}

// JS input.js spurt(): climax mash. Field effects only; particles/audio are
// TODO(mech/fx): spawnJets, drip spawn (poseFluidOrigin; native Drip shape
// differs), playSquelch/playCum/playGrunt/playMoan.
void doSpurt(Game& g) {
  if (g.state != "climax") return;
  g.spurts += 1.0;
  g.climaxT = 0.0;
  g.bloom = g.bloom > 0.35 ? g.bloom : 0.35;
  g.shake = 4.2;
  g.breast.v += 145.0;
  g.butt.v += 90.0;
  g.nod = 1.2;
  g.shaftPulse = 1.3;
}

// JS mechanics.js startClimax(). sync quirk preserved literally: JS sets
// G.state='climax' BEFORE reading G.state==='orgasm', so the first disjunct
// is always false; sync then rests on pleasure/orgT/orgasms.
void startClimax(Game& g) {
  g.state = "climax";
  g.climax = true;
  g.spurts = 0.0;
  g.climaxT = 0.0;
  g.kissT = 0.0;
  g.rubT = 0.0;
  g.oralT = 0.0;
  g.sync = (g.state == "orgasm" || g.pleasure >= 86.0 ||
            (g.orgT < 6.0 && g.orgasms > 0.0));
  // TODO(platform): mashTip DOM overlay has no native hook yet.
}

// JS input.js toggleKiss(). TODO(audio): playMoan+say on engage.
void toggleKiss(Game& g) {
  if (g.state != "play" || g.tired) return;
  g.kissT = (g.kissT != 0.0) ? 0.0 : 1.0;
}

// JS input.js setRubZone(z): direct pick 1-4, starts rubbing immediately.
// TODO(audio): rubFeedback moan/say (nod=1 kept; speech has no C++ API yet).
void setRubZone(Game& g, int z) {
  if (g.state != "play") return;
  if (g.tired) return; // JS says a tired line; TODO(audio): speech hook
  g.rubZone = ((z % 4) + 4) % 4;
  g.rubT = 1.0;
  g.nod = 1.0;
}

// JS input.js cycleRubZone() (Q or repeated C-tap).
void cycleRubZone(Game& g) {
  if (g.state != "play" || g.tired) return;
  if (g.rubT == 0.0) {
    g.rubT = 1.0;
    g.nod = 1.0;
    return;
  }
  setRubZone(g, g.rubZone + 1);
}

// JS input.js toggleRub() (tap) / stopRub() (double-tap or Digit0).
void toggleRub(Game& g) {
  if (g.state != "play" || g.tired) return;
  if (g.rubT == 0.0) {
    g.rubT = 1.0;
    g.nod = 1.0;
  } else {
    cycleRubZone(g);
  }
}

void stopRub(Game& g) { g.rubT = 0.0; }

// JS input.js C-tap / rub-button double-tap (<350ms) stops, else starts.
void tapRub(Game& g) {
  if (g.rubT != 0.0 && (g.t - s_lastRubTap) < 0.35) {
    s_lastRubTap = -10.0;
    stopRub(g);
  } else {
    s_lastRubTap = g.t;
    toggleRub(g);
  }
}

// JS input.js toggleSolo(). TODO(audio): playMoan+say on engage.
void toggleSolo(Game& g) {
  if (g.state != "play" && g.state != "orgasm") return;
  g.solo = !g.solo;
  g.soloPh = 0.0;
  g.soloMoanT = 2.5;
  if (g.solo) {
    g.oralT = 0.0;
    g.oral = 0.0;
    g.kissT = 0.0;
    g.rubT = 0.0;
    // 2D closeups are couple-framed; drop to full-body side view.
    // (Native 2D has no CAM3, so the typeof-CAM3 guard is always true.)
    if (g.view != "side") {
      g.view = "side";
      g.viewFade = 1.0;
    }
  }
}

// JS input.js toggleView(). TODO(platform): lsSet('ag_view') persistence.
void toggleView(Game& g) {
  g.view = (g.view == "fpv") ? "side" : "fpv";
  g.viewFade = 1.0;
}

// JS input.js toggleFaceSide(). TODO(audio): say hook.
void toggleFaceSide(Game& g) {
  g.faceSide = (g.faceSide == "profile") ? "camera" : "profile";
}

// JS input.js cycleFPVFocus() (Z / zoom button). TODO(audio): say hook.
void cycleFPVFocus(Game& g) {
  if (g.view != "fpv") {
    g.view = "fpv";
    g.viewFade = 1.0;
    g.fpvFocus = "hips";
    return;
  }
  static const char* kModes[4] = {"full", "hips", "breasts", "face"};
  int idx = 0;
  for (int i = 0; i < 4; ++i) {
    if (g.fpvFocus == kModes[i]) {
      idx = i;
      break;
    }
  }
  g.fpvFocus = kModes[(idx + 1) % 4];
}

// JS input.js keyFX() (X / climax button): unlocks after CFG.climaxAt.
void keyFX(Game& g) {
  if ((g.state == "play" || g.state == "orgasm") && g.sesT > CFG.climaxAt)
    startClimax(g);
}

// JS input.js restart() (restart/again buttons; R key in native).
void doRestart(Game& g) {
  g.state = "play";
  g.sesT = 0.0;
  g.target = 0.28;
  g.depth = 0.28;
  g.pDepth = 0.28;
  g.maxD = 0.0;
  g.valley = 0.28;
  g.strokes = 0.0;
  g.inBand = 0.0;
  g.rate = 0.0;
  g.combo = 1.0;
  g.comboBest = 1.0;
  g.pleasure = 12.0;
  g.floor = 2.0;
  g.sens = 1.0;
  g.stamina = 100.0;
  g.tired = false;
  g.kissT = 0.0;
  g.rubT = 0.0;
  g.oralT = 0.0;
  g.orgasms = 0.0;
  g.orgT = 0.0;
  g.after = 0.0;
  g.climax = false;
  g.spurts = 0.0;
  g.climaxT = 0.0;
  g.finishT = 0.0;
  g.sync = false;
  g.endedShown = false;
  g.round = 1.0;
  g.speech.clear();
  g.sayCd = 0.0;
  g.spaceHeld = 0.0;
  g.shaftPulse = 0.0;
  g.solo = false;
  g.soloPh = 0.0;
  g.drips.clear();
  g.glisten.clear();
  g.hearts.clear();
  g.sweat.clear();
  g.jets.clear();
  s_hasDrag = false;
  g.dragOn = false;
  // JS also clears orgHearts DOM + G.slisten typo; no native equivalents.
}

} // namespace

void keyDown(Game& g, Key k) {
  switch (k) {
  case Key::Space: {
    if (s_spaceDown) return; // JS: e.repeat guard
    s_spaceDown = true;
    if (g.state == "climax") {
      doSpurt(g);
      return;
    }
    if (g.state == "intro") return;
    if (g.state == "play" || g.state == "orgasm") {
      g.spaceHeld = 1.0;
      if (g.tired) {
        g.spaceHeld = 0.0; // JS says a tired line; TODO(audio): speech hook
      } else {
        // Phase-align the auto-thrust wave (mechanics.js SPACE drive).
        g.autoPh = std::asin(clamp((g.target - 0.55) / 0.38, -1.0, 1.0));
        // Solo space = rub engage (zone LOW). TODO(audio): rubFeedback moan.
        if (g.solo && g.rubT == 0.0) {
          g.rubZone = 3;
          g.rubT = 1.0;
          g.nod = 1.0;
        }
      }
    }
    return;
  }
  case Key::E:
    toggleKiss(g);
    return;
  case Key::C:
    tapRub(g);
    return;
  case Key::Q:
    cycleRubZone(g);
    return;
  case Key::Digit1:
    setRubZone(g, 0);
    return;
  case Key::Digit2:
    setRubZone(g, 1);
    return;
  case Key::Digit3:
    setRubZone(g, 2);
    return;
  case Key::Digit4:
    setRubZone(g, 3);
    return;
  case Key::T:
    toggleFaceSide(g);
    return;
  case Key::Z:
    cycleFPVFocus(g);
    return;
  case Key::Tab:
    poses::cycle(g); // JS: cyclePose(), unconditional
    return;
  case Key::O:
    // JS toggleOral() guards play/non-tired internally; enforce at call site
    // since oral::toggle is a bare flip until oral.js lands.
    if (g.state != "play" || g.tired) return;
    oral::toggle(g);
    return;
  case Key::S:
    toggleSolo(g);
    return;
  case Key::X:
    keyFX(g);
    return;
  case Key::V:
    toggleView(g);
    return;
  case Key::P:
    charui::toggle(g); // JS: toggleCustomPanel()
    return;
  case Key::M:
    // JS: ai(); setMute(!muted). ai() (audio resume) is platform-owned.
    s_muted = !s_muted;
    audio::setMuted(s_muted);
    return;
  case Key::R:
    doRestart(g);
    return;
  case Key::F:
    // Platform-owned fullscreen (JS: toggleFull); ignore.
    return;
  case Key::L:
    // L/R drive the 3D orbit camera (resetCam3 / CAM3.look); no 2D binding.
    return;
  case Key::Unknown:
    return;
  }
}

void keyUp(Game& g, Key k) {
  if (k == Key::Space) {
    s_spaceDown = false;
    g.spaceHeld = 0.0;
    if (g.solo && g.rubT != 0.0) stopRub(g); // JS keyup: solo rub release
  }
}

void pointerDown(Game& g, const Pointer& p) {
  // JS pdown() ignores presses on buttons/hud/panels; native equivalent:
  // the customize overlay consumes its own taps.
  if (charui::visible(g)) {
    charui::tap(g, p.x, p.y);
    return;
  }
  if (g.state == "intro") return;
  if (g.state == "climax") {
    doSpurt(g);
    return;
  }
  if (!isPlayLike(g)) return;
  s_dragY = p.y;
  s_hasDrag = true;
  g.dragOn = true;
}

void pointerMove(Game& g, const Pointer& p) {
  if (!g.dragOn || !s_hasDrag) return;
  if (!(CFG.dragSens > 0.0)) return;
  const F64 gain = 1.0 / (F64)VW * (1.0 / CFG.dragSens); // 1/(VW*dragSens)
  const F64 dy = p.y - s_dragY;
  s_dragY = p.y;
  g.target = clamp(g.target + dy * gain, 0.0, 1.0);
}

void pointerUp(Game& g, const Pointer& p) {
  (void)p;
  g.dragOn = false;
  s_hasDrag = false;
}

void update(Game& g, F64 dt) {
  (void)dt;
  // Hold dynamics owned here; the SPACE wave drive itself lives in mech.
  // Mirrors mechanics.js: tired kills a held thrust.
  if (g.tired) g.spaceHeld = 0.0;
  g.target = clamp(g.target, 0.0, 1.0);
}

} // namespace ag::input
