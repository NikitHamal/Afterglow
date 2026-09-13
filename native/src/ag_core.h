// Afterglow native — ag_core.h
// 1:1 port of js/core.js: tuning knobs (CFG) + full game state (G).
// All sim math stays in double (= JS number). RNG is seeded (see prelude).
#pragma once
#include "ag_prelude.h"

namespace ag {

struct Cfg {
  F64 dragSens = 0.30;
  F64 strokeMinRange = 0.22;
  F64 pleaBase = 1.85;
  F64 pleaDecay = 1.35;
  F64 gainDamp = 260.0;
  F64 stamDrill = 1.9;
  F64 stamRegen = 8.0;
  F64 kissGain = 1.28, kissCost = 1.8;
  F64 rubGain0 = 1.5, rubGain1 = 0.012;
  F64 rubCost = 2.2;
  F64 band0 = 1.05, band1 = 1.85, band2 = 0.34, band3 = 0.22;
  F64 hotW = 0.15;
  F64 orgBonus = 38.0;
  F64 orgFloor = 44.0;
  F64 sensStep = 0.22;
  F64 climaxAt = 10.0;
  F64 spurtNeed = 7.0;
};
inline const Cfg CFG{};

struct CharPreset {
  std::string preset = "yuki", name = "Yuki";
  std::string quote =
      "\"Mm... finally we're alone. You remember how I like it... right?\"";
  F64 skinTone = 0.18;
  std::string hairColor = "#231318", hairStyle = "long";
  F64 bodyScale = 0.45, breastSize = 0.45;
  std::string nippleColor = "#c25f63", blushColor = "#e86070";
  std::string lipColor = "#b3555f", eyeColor = "#4a2c33";
  std::string pubicHair = "trim";
};

// Particle / transient pools (js/core.js G arrays). SoA-light: tiny structs,
// std::vector with reserve() — zero per-frame allocation in the hot loop.
struct Jet {
  F64 x = 0, y = 0, vx = 0, vy = 0, life = 0;
  char view = 's'; // 's' side | 'f' fpv  (js uses view!=='side' skip)
};
struct Drip {
  F64 x = 0, ox = 0, p = 0, j = 0;
};
struct Speck {
  F64 x = 0, y = 0, a = 0;
};
struct Sweat {
  F64 x = 0, y = 0, life = 0;
};
struct Heart {
  F64 x = 0, y = 0, life = 0, ph = 0, s = 1;
};
struct MoanMouth {
  F64 x = 0, y = 0, t = 0;
};

struct Spring {
  F64 p = 0, v = 0;
};

struct Game {
  std::string state = "intro";
  F64 t = 0, sesT = 0;
  F64 target = 0.28, depth = 0.28, pDepth = 0.28, vel = 0, prevV = 0;
  F64 maxD = 0, valley = 0.28;
  F64 lastStrokeT = -9, strokeDur = 0, strokes = 0, inBand = 0;
  F64 rate = 0, combo = 1, comboBest = 1;
  F64 pleasure = 12, floor = 2, sens = 1;
  F64 stamina = 100;
  bool tired = false;
  F64 tiredT = 0;
  F64 kiss = 0, rub = 0, kissT = 0, rubT = 0;
  int rubZone = 0;
  F64 oral = 0, oralT = 0, oralDepth = 0, oralGag = 0;
  int pos = 0;
  bool solo = false;
  F64 soloPh = 0, soloMoanT = 3;
  std::vector<Jet> jets;
  F64 shaftPulse = 0;
  F64 orgasms = 0, orgT = 0, after = 0;
  bool climax = false;
  F64 spurts = 0, climaxT = 0, finishT = 0;
  bool sync = false, endedShown = false;
  F64 ar = 0, round = 1;
  F64 hotC = 0.55, shake = 0, bloom = 0, impact = 0;
  Spring breast, butt;
  F64 blink = 2, blinkPh = -1;
  std::vector<MoanMouth> mouths;
  F64 nextMoan = 2, nextBreath = 1, nextBeat = 0;
  F64 sayCd = 0;
  std::string speech;
  std::vector<Sweat> sweat;
  std::vector<Heart> hearts;
  std::vector<Drip> drips;
  std::vector<Speck> glisten;
  F64 strokeFlash = 0, hotFlash = 0;
  bool dragOn = false;
  F64 nod = 0, spaceHeld = 0, autoPh = -3.141592653589793 / 2.0;
  std::string view = "side";
  F64 viewFade = 0;
  std::string faceSide = "profile";
  F64 faceBlend = 0;
  std::string fpvFocus = "full";
  F64 fpvZoom = 1, fpvPanX = 640, fpvPanY = 360;
  CharPreset ch;

  void reset(); // defaults above (mirrors core.js literal)
};

inline const char* RUBZONES[4] = {"both", "left", "right", "low"};
inline const char* RUBLBL[4] = {"BOTH", "LEFT", "RIGHT", "LOW"};
inline F64 rubGain(int zone) {
  switch (zone) {
  case 1:
  case 2:
    return 0.85;
  case 3:
    return 1.4;
  default:
    return 1.0;
  }
}

} // namespace ag
