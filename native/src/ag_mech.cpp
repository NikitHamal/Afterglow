// Afterglow native — ag_mech.cpp
// 1:1 port of js/mechanics.js: thrust physics, pleasure/stamina/combo/
// orgasm/climax progression, particle pools, springs, blink/moan timers.
// Sim-only: JS audio/DOM/speech side effects (play*, say, tierLine,
// document.*) are dropped; RNG draws gating them are still consumed via
// rng.next01()/range() in JS order to keep the call sequence stable.
// Fixed-dt integration; advances g.t / g.sesT; no wall-clock reads.
#include "ag_2dmods.h"

#include <cmath>
#include <cstddef>

namespace ag {
namespace mech {
namespace {

// TODO(core.h): G.lube missing — file-local stand-in (JS seed 0.35, range [0.2,1]).
F64 s_lube = 0.35;
// TODO(core.h): G.engorgement missing — write-only accumulator in JS (seed 0.1).
F64 s_eng = 0.1;
// TODO(core.h): G.stall missing — JS fallback 0.45 used for rate decay.
constexpr F64 kStall = 0.45;
// TODO(core.h): G.spurtHold missing — climax depth driver (seed 0).
F64 s_spurtHold = 0.0;
// TODO(core.h): G.nBreathM missing — tired-panting timer (seed 0).
F64 s_nBreathM = 0.0;
// TODO(core.h): G.speech.until missing — expiry for the speech string.
// Defaults huge so UI speech survives until a say() port sets a real expiry.
F64 s_speechUntil = 1.0e18;
// TODO(core.h): Heart missing vy — spawn sites consume the JS rr() draws;
// rise uses a mean speed instead.
constexpr F64 kHeartRise = -30.0;
// TODO(core.h): Drip missing sp — spawns consume the JS rr() draws;
// progress uses a mean speed instead.
constexpr F64 kDripSp = 0.15;
// TODO(core.h): Sweat missing vy/ph — draws consumed at spawn; fall uses a
// mean speed and sway phase is derived from x.
constexpr F64 kSweatFall = 7.0;
// TODO(core.h): MoanMouth has {x,y,t} but JS filters on {t0,dur} — t is
// treated as t0 with a fixed keep window.
constexpr F64 kMouthKeep = 0.7;
// TODO(core.h/poses): spawnJets()/poseFluidOrigin() live in other modules —
// jets spawn in spurt() is skipped, fluid origin fixed at the JS fallback.
constexpr F64 kFluidX = 640.0;
constexpr F64 kFluidY = 500.0;

struct Band {
  F64 c = 0.0, hw = 0.0;
};

// js sweetBand(): identical numeric order.
inline Band sweetBand(const Game& g) {
  const F64 p = sm(0.15, 0.95, g.pleasure / 100.0);
  const F64 arousalMod = g.ar / 100.0 * 0.22;
  const F64 bioDrift = std::sin(g.t * 0.28) * 0.14 + std::cos(g.t * 0.08) * 0.06;
  const F64 center = lerp(CFG.band0, CFG.band1, p) + arousalMod + bioDrift;
  const F64 halfWidth = lerp(CFG.band2, CFG.band3, p) * (1.0 - p * 0.25);
  return {center, std::fmax(0.12, halfWidth)};
}

struct Zone {
  F64 c = 0.0, w = 0.0;
};

// js hotZone(): identical numeric order.
inline Zone hotZone(const Game& g) {
  const F64 t = g.t;
  const F64 gSpotCycle = std::sin(t * 0.09) * 0.22;
  const F64 fornixCycle = std::sin(t * 0.035 + 1.8) * 0.18;
  const F64 center = clamp(0.56 + gSpotCycle + fornixCycle, 0.32, 0.90);
  const F64 width = CFG.hotW * (1.0 + (g.ar / 100.0) * 0.35);
  return {center, width};
}

// JS falsy-number helper: 0 behaves like undefined (falls back).
inline F64 nz(F64 v, F64 fb) { return v != 0.0 ? v : fb; }

inline void integrateSpring(Spring& s, F64 k, F64 d, F64 dt) {
  s.v += (-s.p * k - s.v * d) * dt;
  s.p += s.v * dt;
  s.p = clamp(s.p, -18.0, 18.0);
}

void beginOrgasm(Game& g, Rng& rng) {
  g.state = "orgasm";
  g.orgT = 0.0;
  g.orgasms += 1.0;
  g.combo = 1.0;
  g.stamina = std::fmin(100.0, g.stamina + CFG.orgBonus);
  g.shake = 6.5;
  g.bloom = 0.65;
  g.shaftPulse = 1.6;
  // say/tierHearts/vocalScore are DOM/audio-only: dropped.
  if (rng.next01() < 0.18) { /* sfxVoice('love') dropped */ }
  for (int i = 0; i < 9; i++) {
    Heart h;
    h.x = 640.0 + rng.range(-40.0, 50.0);
    h.y = 470.0 + rng.range(-30.0, 15.0);
    rng.range(24.0, 48.0); // vy draw consumed; see kHeartRise
    h.ph = rng.next01() * TAU;
    h.life = rng.range(1.6, 2.8);
    h.s = rng.range(0.65, 1.35);
    g.hearts.push_back(h);
  }
}

void registerStroke(Game& g, Rng& rng, F64 peak, F64 valley, F64 dur) {
  g.strokes += 1.0;
  g.lastStrokeT = g.t;
  g.strokeFlash = 0.55;

  const bool inOrg = (g.state == "orgasm");
  const F64 tempo = clamp(1.0 / std::fmax(0.001, dur), 0.35, 4.0);
  const Band band = sweetBand(g);
  const bool inBand = std::fabs(tempo - band.c) <= band.hw;
  if (inBand) g.inBand += 1.0;

  const F64 strokeRange = peak - valley;
  const Zone z = hotZone(g);
  const bool hitHot =
      (peak >= z.c - z.w * 0.5 && peak <= z.c + z.w * 0.5) ||
      (valley <= z.c && peak >= z.c);

  s_lube = clamp(s_lube + 0.025 + (g.ar / 100.0) * 0.04, 0.2, 1.0);
  s_eng = clamp(s_eng + 0.018 * (peak + 0.2), 0.0, 1.0);

  if (!inOrg && g.state == "play") {
    const F64 depthCurve = 0.40 + 0.60 * std::pow(peak, 1.35);
    const F64 displacementFactor = clamp(strokeRange / 0.60, 0.45, 1.25);
    const F64 tempoAccuracy =
        1.0 - clamp(std::fabs(tempo - band.c) / (band.hw * 1.5), 0.0, 1.0);
    const F64 resonanceBonus = 1.0 + tempoAccuracy * 0.65;
    const F64 intimacyMul = (g.kiss > 0.4 ? CFG.kissGain : 1.0);
    const F64 plateauTension = 1.08 - std::pow(g.pleasure / CFG.gainDamp, 1.8);
    F64 gain = CFG.pleaBase * depthCurve * displacementFactor * g.combo *
               g.sens * intimacyMul * plateauTension * resonanceBonus;
    if (hitHot) {
      gain *= 1.85;
      g.shaftPulse = std::fmin(1.4, g.shaftPulse + 0.45);
    }
    if (g.oral > 0.4) gain *= 1.18;
    g.pleasure = clamp(g.pleasure + gain, g.floor, 100.0);

    if (inBand) {
      g.combo = std::fmin(3.8, g.combo + 0.16 + (hitHot ? 0.14 : 0.0));
    } else {
      g.combo = std::fmax(1.0, g.combo - 0.22);
    }
    g.comboBest = std::fmax(g.comboBest, g.combo);
  }

  g.rate = g.rate * 0.45 + tempo * 0.55;
  const F64 strokeMomentum = peak * clamp(tempo / 1.8, 0.6, 2.2);
  g.breast.v += (55.0 + 160.0 * strokeMomentum);
  g.butt.v += (42.0 + 95.0 * strokeMomentum);
  g.impact = std::fmin(1.2, 0.45 + peak * 0.75);
  g.nod = std::fmin(1.2, g.nod + 0.55 * peak);

  if (g.oral > 0.5) {
    // playOralSlurp/playThroatHum dropped (audio-only, no RNG).
    if (peak > 0.72) {
      if (rng.next01() < 0.40) {
        // playOralGag dropped; esophageal clamp state kept.
        g.shaftPulse = std::fmin(1.5, g.shaftPulse + 0.6);
      }
    }
    if (rng.next01() < 0.32) { /* playGrunt dropped */ }
  } else {
    if (peak > 0.68) {
      // playSlap volume dropped (audio-only); impact shake kept.
      g.shake = std::fmin(3.6, g.shake + 1.35 * peak);
    }
    // playSquelch dropped (audio-only, no RNG).
    if (hitHot && g.hotFlash <= 0.0) {
      g.hotFlash = 1.35;
      // playMoan/say/spotFx dropped (audio/DOM).
      for (int i = 0; i < 4; i++) {
        Heart h;
        h.x = 660.0 + rng.range(-12.0, 12.0);
        h.y = 500.0 + rng.range(-8.0, 8.0);
        rng.range(20.0, 38.0); // vy draw consumed; see kHeartRise
        h.ph = rng.next01() * TAU;
        h.life = 1.6;
        h.s = rng.range(0.5, 0.95);
        g.hearts.push_back(h);
      }
    } else if (g.pleasure > 36.0 && rng.next01() < 0.48) {
      /* playMoan dropped */
    }
    if (peak > 0.82 && rng.next01() < 0.26) { /* playGrunt dropped */ }
  }

  if (g.pleasure >= 100.0 && !inOrg && g.state == "play") beginOrgasm(g, rng);
}

void spurt(Game& g, Rng& rng) {
  if (g.state != "climax") return;
  g.spurts += 1.0;
  g.climaxT = 0.0;
  // playSquelch/playCum/playGrunt/playMoan dropped (audio-only, no RNG).
  g.bloom = std::fmax(g.bloom, 0.35);
  g.shake = 4.2;
  g.breast.v += 145.0;
  g.butt.v += 90.0;
  g.nod = 1.2;
  g.shaftPulse = 1.3;
  // spawnJets() skipped: external module (see kFluidX/kFluidY note).
  for (int i = 0; i < 3; i++) {
    if (g.drips.size() < 16) {
      Drip d;
      d.p = 0.0;
      rng.range(0.09, 0.22); // sp draw consumed; see kDripSp
      d.x = kFluidX + rng.range(-6.0, 6.0);
      d.j = rng.range(0.0, 2.0);
      d.ox = kFluidX;
      g.drips.push_back(d);
    }
  }
  for (int i = 0; i < 3; i++) {
    Heart h;
    h.x = 655.0 + rng.range(-10.0, 10.0);
    h.y = 500.0 + rng.range(-6.0, 8.0);
    rng.range(22.0, 40.0); // vy draw consumed; see kHeartRise
    h.ph = rng.next01() * TAU;
    h.life = 1.4;
    h.s = rng.range(0.55, 1.05);
    g.hearts.push_back(h);
  }

  if (g.spurts >= CFG.spurtNeed) {
    g.state = "finish";
    g.finishT = 0.0;
    g.endedShown = false;
    // mashTip DOM dropped.
    g.shake = 7.5;
    g.bloom = 0.55;
    // playGrunt x2 / say dropped (audio/speech).
    if (rng.next01() < 0.15) { /* sfxVoice('love') dropped */ }
  }
}

void nextRound(Game& g) {
  g.state = "play";
  g.climax = false;
  g.spurts = 0.0;
  g.climaxT = 0.0;
  g.endedShown = false;
  g.finishT = 0.0;
  g.stamina = 100.0;
  g.tired = false;
  g.pleasure = std::fmax(g.pleasure, 54.0);
  g.sync = false;
  g.round += 1.0;
  g.floor = std::fmax(g.floor, CFG.orgFloor);
  g.drips.clear();
  g.jets.clear();
  if (g.glisten.size() > 8) g.glisten.resize(8);
  // mashTip/say dropped (DOM/speech).
}

void scheduleVoice(Game& g, Rng& rng, F64 dt) {
  g.nextMoan = nz(g.nextMoan, 2.0) - dt;
  if (g.rub > 0.5) g.nextMoan = std::fmax(g.nextMoan, 0.65);

  if (g.nextMoan <= 0.0) {
    const F64 a = g.pleasure / 100.0;
    // playMoan dropped (audio-only, no RNG).
    g.nextMoan = lerp(6.2, 1.4, a) * (0.65 + rng.next01() * 0.75) *
                 (g.state == "orgasm" ? 0.42 : 1.0);
    if (g.after > 0.0 && rng.next01() < 0.55) {
      /* say(LINES.after) dropped */
    } else if (rng.next01() < 0.52 && g.sayCd <= 0.0) {
      /* tierLine() dropped */
    }
  }

  g.nextBreath = nz(g.nextBreath, 1.5) - dt;
  if (g.nextBreath <= 0.0) {
    // playBreath(false) dropped (audio-only, no RNG).
    g.nextBreath = 1.0 / lerp(0.18, 0.55, g.pleasure / 100.0);
  }

  if (g.tired) {
    g.tiredT += dt;
    s_nBreathM -= dt;
    if (s_nBreathM < 0.0) {
      // playBreath(true) dropped (audio-only, no RNG).
      s_nBreathM = 0.52;
    }
    if (g.tiredT > 5.2) {
      g.tired = false;
      g.stamina = 65.0;
    }
  }

  if (g.pleasure > 70.0) {
    g.nextBeat = nz(g.nextBeat, 0.6) - dt;
    if (g.nextBeat <= 0.0) {
      // playHeart() dropped (audio-only, no RNG).
      const F64 bpm = 74.0 + ((g.pleasure - 70.0) * 1.35) + (g.orgasms * 8.5);
      g.nextBeat = 60.0 / std::fmin(185.0, bpm);
    }
  }
}

} // namespace

F64 sweetBandLo(const Game& g) {
  const Band b = sweetBand(g);
  return b.c - b.hw;
}

F64 sweetBandHi(const Game& g) {
  const Band b = sweetBand(g);
  return b.c + b.hw;
}

F64 hotLo(const Game& g) {
  const Zone z = hotZone(g);
  return z.c - z.w * 0.5;
}

F64 hotHi(const Game& g) {
  const Zone z = hotZone(g);
  return z.c + z.w * 0.5;
}

void update(Game& g, Rng& rng, F64 dt) {
  const F64 t = (g.t += dt);
  g.ar = g.pleasure;

  // Linear decay timers.
  g.sayCd = std::fmax(0.0, g.sayCd - dt);
  g.shake *= std::pow(0.015, dt);
  g.bloom *= std::pow(0.04, dt);
  g.viewFade = std::fmax(0.0, g.viewFade - dt * 3.4);
  g.impact *= std::pow(0.04, dt);
  g.nod *= std::pow(0.003, dt);
  g.strokeFlash = std::fmax(0.0, g.strokeFlash - dt);
  g.hotFlash = std::fmax(0.0, g.hotFlash - dt);
  g.after = std::fmax(0.0, g.after - dt);
  g.shaftPulse = std::fmax(0.0, g.shaftPulse - dt * 1.6);
  s_lube = clamp(s_lube - dt * 0.012 + (g.ar / 100.0) * dt * 0.018, 0.2, 1.0);

  integrateSpring(g.breast, 250.0, 8.8, dt);
  integrateSpring(g.butt, 310.0, 9.8, dt);

  if (!g.mouths.empty()) {
    size_t w = 0;
    for (size_t r = 0; r < g.mouths.size(); r++) {
      // TODO(core.h): MoanMouth lacks t0/dur — m.t treated as t0, fixed window.
      if (t - g.mouths[r].t < kMouthKeep) {
        if (w != r) g.mouths[w] = g.mouths[r];
        w++;
      }
    }
    g.mouths.resize(w);
  }
  if (!g.speech.empty() && t > s_speechUntil) g.speech.clear();

  g.blink -= dt;
  if (g.blink < -0.14) g.blink = rng.range(2.0, 5.8);
  g.blinkPh = g.blink < 0.0 ? 1.0 - std::fabs(g.blink / 0.14) : 0.0;

  if (g.state == "intro" || g.state == "play" || g.state == "orgasm") {
    if (g.state != "intro") g.sesT += dt;

    g.kiss = lerp(g.kiss, (g.kissT != 0.0 && g.state == "play") ? 1.0 : 0.0,
                  1.0 - std::pow(0.002, dt));
    g.rub = lerp(g.rub, (g.rubT != 0.0 && g.state == "play") ? 1.0 : 0.0,
                 1.0 - std::pow(0.003, dt));
    g.oral = lerp(g.oral, (g.oralT != 0.0 && g.state == "play") ? 1.0 : 0.0,
                  1.0 - std::pow(0.004, dt));

    if (g.tired) g.spaceHeld = 0.0;
    if (g.spaceHeld != 0.0 && !g.dragOn &&
        (g.state == "play" || g.state == "orgasm")) {
      if (g.state != "orgasm") {
        const Band band = sweetBand(g);
        g.autoPh += TAU * band.c * dt;
        const F64 rawWave = std::sin(g.autoPh);
        const F64 wave = rawWave > 0.0 ? std::pow(rawWave, 0.85)
                                       : -std::pow(-rawWave, 1.15);
        g.target = 0.54 + 0.40 * wave;
      } else {
        g.spaceHeld = 0.0;
      }
    }

    if (g.state != "orgasm") {
      const F64 elasticResistance =
          g.target > 0.82 ? 1.0 - (g.target - 0.82) * 0.45 : 1.0;
      const F64 springRate = std::fmin(1.0, dt * 17.5 * elasticResistance);
      g.depth += (g.target - g.depth) * springRate;
    } else {
      const F64 orgSpasm = (std::sin(t * 12.0) * 0.5 + 0.5) * 0.15 *
                           std::sin(3.141592653589793 *
                                    clamp(g.orgT / 5.2, 0.0, 1.0));
      const F64 orgDrift = (std::sin(t * 1.1) * 0.5 + 0.5) * 0.45 + orgSpasm;
      g.depth += (orgDrift - g.depth) * std::fmin(1.0, dt * 6.5);
    }

    const F64 v = (g.depth - nz(g.pDepth, g.depth)) / std::fmax(dt, 0.001);
    g.pDepth = g.depth;
    g.vel = g.vel * 0.72 + v * 0.28;
    const F64 sv = g.vel;

    if (sv > 0.0 && g.prevV <= 0.0) g.valley = g.depth;
    if (sv > 0.0) g.maxD = std::fmax(nz(g.maxD, 0.0), g.depth);
    if (sv < 0.0 && g.prevV > 0.0) {
      const F64 peak = nz(g.maxD, g.depth);
      g.maxD = g.depth;
      const F64 dur =
          clamp(t - nz(g.lastStrokeT, t - 1.0), 0.22, 4.0);
      if (peak - nz(g.valley, 0.0) > CFG.strokeMinRange &&
          (g.state == "play" || g.state == "orgasm")) {
        registerStroke(g, rng, peak, nz(g.valley, 0.0), dur);
        g.strokeDur = dur;
      }
    }

    // Retraction audio block (playLipPop/playPullout/playSlide): audio-only,
    // no RNG, no sim state — dropped.
    g.prevV = sv;

    g.rate *= std::pow(kStall, dt);
    if (t - nz(g.lastStrokeT, t) > 2.2) {
      g.combo = std::fmax(1.0, g.combo - 0.16 * dt);
    }

    if (g.state == "play") {
      if (t - nz(g.lastStrokeT, t) > 1.8) {
        g.pleasure = std::fmax(g.floor, g.pleasure - CFG.pleaDecay * dt);
      }

      // rubZoneName()/RUBGAIN map to the native zone table; only the
      // sim-state parts (gain + breast wobble) are kept, audio dropped.
      const F64 zm = rubGain(g.rubZone);
      const bool isBoth = (g.rubZone == 0);

      if (g.rub > 0.45) {
        const F64 rubStep =
            (CFG.rubGain0 * zm + CFG.rubGain1 * g.pleasure) * dt;
        g.pleasure = std::fmin(100.0, g.pleasure + rubStep);
      }
      if (g.rub > 0.12) {
        g.breast.v +=
            g.rub * (isBoth ? 68.0 : 50.0) * dt * 10.0 * std::sin(t * 9.2);
        // playRubStroke half-cycle scheduling + rub moans: audio-only — dropped.
      }

      if (g.oral > 0.04) {
        const F64 oralStep =
            (CFG.rubGain0 * 1.20 + CFG.rubGain1 * g.pleasure) * dt;
        g.pleasure = std::fmin(100.0, g.pleasure + oralStep);
        g.breast.v += g.oral * 24.0 * dt * 10.0 * std::sin(t * 6.2);
      }

      F64 drain = 0.0;
      if (g.rate > 0.25) {
        const Band band = sweetBand(g);
        drain = 0.65 + g.rate * CFG.stamDrill * (0.3 + 0.7 * g.depth);
        if (g.rate > band.c + band.hw) drain += 0.95;
      } else {
        drain = -CFG.stamRegen;
      }
      if (g.kiss > 0.5) drain += CFG.kissCost;
      if (g.rub > 0.5) drain += CFG.rubCost;
      if (g.oral > 0.5) drain += 1.1;

      if (drain > 0.0) {
        g.stamina = std::fmax(0.0, g.stamina - drain * dt);
      } else if (!g.tired) {
        g.stamina = std::fmin(100.0, g.stamina - drain * dt);
      }

      if (g.stamina <= 0.0 && !g.tired) {
        g.tired = true;
        g.tiredT = 0.0;
        g.kissT = 0.0;
        g.rubT = 0.0;
        g.oralT = 0.0;
        g.spaceHeld = 0.0;
        // say(LINES.tired) dropped (speech).
      }

      scheduleVoice(g, rng, dt);
    }

    if (g.state == "orgasm") {
      g.orgT += dt;
      g.shaftPulse = 0.8 + 0.6 * std::sin(g.orgT * 8.5);
      if (g.orgT > 1.4) {
        g.pleasure = std::fmax(
            CFG.orgFloor, 100.0 - ((g.orgT - 1.4) / 4.0) * (100.0 - CFG.orgFloor));
      }
      const F64 pelvicBump = std::sin(std::fmin(1.0, g.orgT / 0.6) * 3.141592653589793);
      g.breast.v += pelvicBump * 90.0;
      if (rng.next01() < dt * 9.0) g.shake = std::fmin(4.2, g.shake + 1.6);

      if (g.orgT > 5.4) {
        g.state = "play";
        g.after = 8.5;
        g.floor = std::fmax(g.floor, CFG.orgFloor);
        g.sens += CFG.sensStep;
        // say(LINES.after) dropped (speech).
      }
    }
  } else if (g.state == "climax") {
    g.sesT += dt;
    g.climaxT += dt;
    s_spurtHold = std::fmax(0.0, s_spurtHold - dt * 3.2);
    g.depth = 0.80 + s_spurtHold * 0.16;
    g.breast.v += 35.0 * dt * 10.0 * std::sin(t * 42.0);
    if (g.climaxT > 5.2) spurt(g, rng);
    scheduleVoice(g, rng, dt);
  } else if (g.state == "finish") {
    g.sesT += dt;
    g.finishT += dt;
    g.depth =
        0.97 + std::sin(t * 28.0) * 0.010 * std::fmax(0.0, 1.0 - g.finishT / 2.4);
    if (g.finishT < 2.4) {
      g.breast.v += 140.0 * dt * std::sin(t * 32.0);
    }
    if (g.finishT > 1.0 && g.drips.size() < 12 && rng.next01() < dt * 2.4) {
      Drip d;
      d.p = 0.0;
      rng.range(0.08, 0.18); // sp draw consumed; see kDripSp
      d.x = kFluidX + rng.range(-6.0, 6.0);
      d.j = rng.range(0.0, 2.0);
      d.ox = kFluidX;
      g.drips.push_back(d);
    }
    if (g.finishT > 3.4 && !g.endedShown) {
      g.endedShown = true;
      nextRound(g);
    }
  }

  if (!g.jets.empty()) {
    for (size_t i = 0; i < g.jets.size(); i++) {
      Jet& j = g.jets[i];
      j.vy += 980.0 * dt;
      j.x += j.vx * dt;
      j.y += j.vy * dt;
      j.life -= dt;
    }
    size_t w = 0;
    for (size_t r = 0; r < g.jets.size(); r++) {
      if (g.jets[r].life > 0.0) {
        if (w != r) g.jets[w] = g.jets[r];
        w++;
      }
    }
    g.jets.resize(w);
  }

  if (!g.sweat.empty() || g.state == "play" || g.state == "orgasm") {
    for (size_t i = 0; i < g.sweat.size(); i++) {
      Sweat& s = g.sweat[i];
      s.y += kSweatFall * dt;
      s.life -= dt * 0.32;
      s.x += std::sin(t * 2.2 + s.x) * 5.5 * dt;
    }
    size_t w = 0;
    for (size_t r = 0; r < g.sweat.size(); r++) {
      if (g.sweat[r].life > 0.0) {
        if (w != r) g.sweat[w] = g.sweat[r];
        w++;
      }
    }
    g.sweat.resize(w);
    const bool hotBody = ((g.pleasure > 52.0 && g.pleasure < 100.0 &&
                           g.state == "play") ||
                          g.state == "orgasm");
    if (hotBody && g.sweat.size() < 18 && rng.next01() < dt * 2.4) {
      Sweat s;
      s.x = 318.0 + rng.range(-14.0, 18.0);
      s.y = 462.0 + rng.range(-6.0, 3.0);
      rng.range(4.5, 9.5);   // vy draw consumed; see kSweatFall
      rng.next01();          // ph draw consumed; phase derived from x
      s.life = 1.0;
      g.sweat.push_back(s);
    }
    if (g.pleasure > 72.0 && g.sweat.size() < 18 && rng.next01() < dt * 2.8) {
      Sweat s;
      s.x = 330.0 + rng.range(-8.0, 8.0);
      s.y = 478.0 + rng.range(0.0, 5.0);
      rng.range(4.5, 8.5);   // vy draw consumed; see kSweatFall
      rng.next01();          // ph draw consumed; phase derived from x
      s.life = 1.0;
      g.sweat.push_back(s);
    }
  }

  if (!g.hearts.empty()) {
    for (size_t i = 0; i < g.hearts.size(); i++) {
      Heart& h = g.hearts[i];
      h.y += kHeartRise * dt;
      h.life -= dt;
      h.ph += dt * 3.2;
    }
    size_t w = 0;
    for (size_t r = 0; r < g.hearts.size(); r++) {
      if (g.hearts[r].life > 0.0) {
        if (w != r) g.hearts[w] = g.hearts[r];
        w++;
      }
    }
    g.hearts.resize(w);
  }

  if (!g.drips.empty()) {
    for (size_t i = 0; i < g.drips.size(); i++) g.drips[i].p += kDripSp * dt;
    size_t w = 0;
    for (size_t r = 0; r < g.drips.size(); r++) {
      Drip& d = g.drips[r];
      if (d.p > 1.0) {
        if (g.glisten.size() < 45) {
          Speck s;
          s.x = d.x + rng.range(-3.0, 3.0);
          s.y = 556.0 + rng.range(-2.0, 4.0);
          s.a = 0.55;
          g.glisten.push_back(s);
        }
        const F64 roll = rng.next01();
        if (roll < 0.32 && g.glisten.size() < 55) {
          Speck s;
          s.x = 640.0 + rng.range(-8.0, 8.0);
          s.y = 546.0 + rng.range(-4.0, 4.0);
          s.a = 0.45;
          g.glisten.push_back(s);
        }
      } else {
        if (w != r) g.drips[w] = g.drips[r];
        w++;
      }
    }
    g.drips.resize(w);
  }
  for (size_t i = 0; i < g.glisten.size(); i++) {
    if (g.glisten[i].a > 0.55) g.glisten[i].a = 0.55;
  }
}

} // namespace mech
} // namespace ag
