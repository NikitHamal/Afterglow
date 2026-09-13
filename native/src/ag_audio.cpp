// Afterglow native — ag_audio.cpp
// Bank decode (WAV hand-parsed + OGG via stb_vorbis) + tiny polyphonic mixer.
// Backend-agnostic: the platform pulls PCM via mixInto() (SDL audio stream on
// PC+Android, direct call in headless parity). Full js/audio.js cue scheduling
// (moan/stroke/climax timing) lands in update(); mixer + decode are real now.
#include "ag_2dmods.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>

// stb_vorbis.c leaks single-letter macros (C, L, R) that collide with
// input::Key enumerators, so it must come after project headers and its
// macros must not escape this TU.
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"
#ifdef C
#undef C
#endif
#ifdef L
#undef L
#endif
#ifdef R
#undef R
#endif

namespace ag::audio {
namespace {

struct Clip {
  std::vector<float> pcm; // interleaved stereo f32
  int rate = 44100;
};
struct Voice {
  const Clip* clip = nullptr;
  size_t pos = 0;
  float gain = 1;
};

std::unordered_map<std::string, Clip> bank_;
std::vector<Voice> voices_;
std::mutex mtx_;
bool muted_ = false;
std::string assetDir_;

// minimal PCM WAV parser (16-bit / 32-bit float, any channels -> stereo)
bool loadWav(const char* path, Clip& out) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  auto rd32 = [&]() {
    uint8_t b[4];
    if (std::fread(b, 1, 4, f) != 4) return (uint32_t)0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
  };
  auto rd16 = [&]() {
    uint8_t b[2];
    if (std::fread(b, 1, 2, f) != 2) return (uint16_t)0;
    return (uint16_t)(b[0] | (b[1] << 8));
  };
  char riff[4], wave[4];
  if (std::fread(riff, 1, 4, f) != 4 || std::fread(wave, 1, 4, f) != 4) {
    std::fclose(f);
    return false;
  }
  (void)wave;
  rd32(); // chunk size
  if (std::memcmp(riff, "RIFF", 4) != 0) {
    std::fclose(f);
    return false;
  }
  int fmt = 0, ch = 0, rate = 0, bits = 0;
  std::vector<float> samples;
  for (;;) {
    char id[4];
    if (std::fread(id, 1, 4, f) != 4) break;
    uint32_t sz = rd32();
    long next = std::ftell(f) + (long)sz;
    if (std::memcmp(id, "fmt ", 4) == 0) {
      fmt = rd16();
      ch = rd16();
      rate = (int)rd32();
      rd32();
      rd16();
      bits = rd16();
    } else if (std::memcmp(id, "data", 4) == 0) {
      size_t frames = sz / (size_t)(ch > 0 ? ch : 1) / (size_t)(bits / 8);
      samples.reserve(frames * 2);
      for (size_t i = 0; i < frames * (size_t)(ch > 0 ? ch : 1); i++) {
        float v = 0;
        if (bits == 16) {
          int16_t s = (int16_t)rd16();
          v = s / 32768.0f;
        } else if (bits == 32 && fmt == 3) {
          uint32_t u = rd32();
          std::memcpy(&v, &u, 4);
        }
        samples.push_back(v);
      }
      // mono->stereo duplicate; >2ch take first two
      if (ch == 1) {
        std::vector<float> st;
        st.reserve(frames * 2);
        for (float v : samples) {
          st.push_back(v);
          st.push_back(v);
        }
        samples.swap(st);
      } else if (ch > 2) {
        std::vector<float> st;
        st.reserve(frames * 2);
        for (size_t i = 0; i < frames; i++) {
          st.push_back(samples[i * (size_t)ch]);
          st.push_back(samples[i * (size_t)ch + 1]);
        }
        samples.swap(st);
      }
      out.rate = rate > 0 ? rate : 44100;
    }
    std::fseek(f, next, SEEK_SET);
  }
  std::fclose(f);
  if (samples.empty()) return false;
  out.pcm = std::move(samples);
  return true;
}

bool loadOgg(const char* path, Clip& out) {
  int ch = 0, rate = 0;
  short* data = nullptr;
  int n = stb_vorbis_decode_filename(path, &ch, &rate, &data);
  if (n <= 0 || !data) return false;
  out.pcm.reserve((size_t)n * 2);
  for (int i = 0; i < n; i++) {
    float l = data[i * ch] / 32768.0f;
    float r = (ch > 1 ? data[i * ch + 1] : data[i * ch]) / 32768.0f;
    out.pcm.push_back(l);
    out.pcm.push_back(r);
  }
  out.rate = rate;
  std::free(data);
  return true;
}

} // namespace

void init(const char* assetDir) {
  shutdown();
  if (!assetDir || !assetDir[0]) return; // silent mode (parity harness)
  assetDir_ = assetDir;
  namespace fs = std::filesystem;
  std::error_code ec;
  size_t loaded = 0;
  for (auto it = fs::recursive_directory_iterator(assetDir_, ec);
       it != fs::recursive_directory_iterator() && loaded < 256;
       it.increment(ec)) {
    if (!it->is_regular_file()) continue;
    std::string ext = it->path().extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext != ".ogg" && ext != ".wav") continue;
    Clip c;
    std::string p = it->path().string();
    bool ok = (ext == ".ogg") ? loadOgg(p.c_str(), c) : loadWav(p.c_str(), c);
    if (ok && !c.pcm.empty() && c.pcm.size() < 64 * 1024 * 1024) {
      bank_[it->path().stem().string()] = std::move(c);
      loaded++;
    }
  }
}
void shutdown() {
  std::lock_guard<std::mutex> l(mtx_);
  bank_.clear();
  voices_.clear();
}
void setMuted(bool m) { muted_ = m; }

void playOnce(const std::string& id, F64 gain) {
  if (muted_) return;
  std::lock_guard<std::mutex> l(mtx_);
  auto it = bank_.find(id);
  if (it == bank_.end()) return;
  if (voices_.size() >= 16) voices_.erase(voices_.begin());
  voices_.push_back({&it->second, 0, (float)gain});
}

// ---- JS cue scheduling port ------------------------------------------------
// Covers the js/audio.js cue entry points as called from js/mechanics.js
// (registerStroke foley, scheduleVoice timers, spurt climax layer, retraction
// slide/pullout, rub half-cycle strokes), js/input.js toggle one-shots
// (kiss/rub/solo), js/oral.js mode one-shots, and js3d/anim3d.js solo ambient
// moans. The mixer above has no synth and no playbackRate, so every cue maps
// to playOnce(stem, gain):
//   * sampled families pick a random stem variant per hit (mirrors sfxHit's
//     random-file-per-hit in audio.js; counts mirror audio/sfx.json);
//   * synth-only keys (grunt/lip/hum/gag/heart/breath) call playOnce with
//     the JS sfx key as a best-effort id — a silent no-op until a matching
//     stem or a synth lands (see TODOs at the bottom of update()).
// Purely observational: only the voice timers (nextMoan/nextBreath/nextBeat),
// lip-sync mouth markers, and file-local edge state are mutated — never
// pleasure, stamina, combo, or particle pools (those belong to the mech port).
namespace {

// Deterministic cue-variety RNG (audio::update takes no Rng; keeps headless
// parity bit-stable regardless of bank contents). Only touched on the sim
// thread inside update().
Rng cueRng_{1234567ull};

// Sampled bank families (flat assets/ dir, keyed by stem; see audio/sfx.json).
constexpr int kSlapWetN = 16; // plapwet01..16 <- sfx 'slap'
constexpr int kSlapDryN = 11; // plapdry01..11 <- sfx 'slapdry'
constexpr int kWetN = 12;     // wet01..12      <- sfx 'wet' (squelch)
constexpr int kStrokeN = 10;  // stroke01..10   <- sfx 'stroke' (slide)
constexpr int kPulloutN = 3;  // pullout01..03  <- sfx 'pullout'
constexpr int kCumInN = 3;    // cum_in01..03   <- sfx 'cumin'
constexpr int kCumOutN = 6;   // cum_out01..06  <- sfx 'cumout'

int pickVariant(int n) { return 1 + (int)(cueRng_.next01() * (F64)n); }
F64 pickChance() { return cueRng_.next01(); }

std::string stemNum(const char* pre, int n) {
  char b[40];
  std::snprintf(b, sizeof b, "%s%02d", pre, n);
  return std::string(b);
}

// Gain laws — verbatim JS clamps from each play* function.
inline F64 moanGain(F64 i) { return clamp(0.44 + i * 0.50, 0.36, 0.98); }
inline F64 slapGain(F64 v) { return clamp(0.38 + v * 0.55, 0.32, 0.98); }
inline F64 squelchGain(F64 v) { return clamp(0.38 + v * 0.52, 0.32, 0.95); }
inline F64 slideGain(F64 v) { return clamp(0.32 + v * 0.48, 0.28, 0.85); }
inline F64 slurpGain(F64 v) { return clamp(0.36 + v * 0.54, 0.30, 0.96); }

void auMouth(Game& g) {
  // Lip-sync marker (js pushes {t0,dur,i}; native MoanMouth only carries a
  // timestamp — dur/i restore is a TODO for the renderer port).
  if (g.mouths.size() >= 8) g.mouths.erase(g.mouths.begin());
  g.mouths.push_back(MoanMouth{0, 0, g.t});
}

// js playMoan(i, {vol}): stem moan02, gain = clamp(.44+i*.5,.36,.98)*vol.
void auMoan(Game& g, F64 intensity, F64 volMul) {
  if (muted_) return; // js: every play* returns early when muted (no mouths)
  playOnce("moan02", moanGain(intensity) * volMul);
  auMouth(g);
}

// js playSquelch(v): stem wetXX.
void auSquelch(Game&, F64 v) {
  playOnce(stemNum("wet", pickVariant(kWetN)), squelchGain(v));
}

// js playSlide(v): stem strokeXX.
void auSlide(Game&, F64 v) {
  playOnce(stemNum("stroke", pickVariant(kStrokeN)), slideGain(v));
}

// js playOralSlurp(v): stem suck05.
void auSlurp(Game&, F64 v) { playOnce("suck05", slurpGain(v)); }

// js playSlap(v): dry clap when lube<0.45 else wet plap. Game has no lube
// field yet (mech port owns it), so assume the JS 0.8 default -> wet.
void auSlap(Game&, F64 v, bool dry) {
  if (dry) playOnce(stemNum("plapdry", pickVariant(kSlapDryN)), slapGain(v));
  else playOnce(stemNum("plapwet", pickVariant(kSlapWetN)), slapGain(v));
}

// Synth-only JS keys: no sample on disk (see bank table above). Best-effort
// id so they go live automatically if a stem is ever added; silent until
// then. Covers grunt/lip/hum/gag/heart/breath.
void auSynth(const std::string& key, F64 gain) { playOnce(key, gain); }

struct AuDelay {
  F64 due = 0;
  std::string id;
  F64 gain = 0;
  F64 mouthI = -1; // >=0: push a lip-sync marker when emitted (orgasm cascade)
};

struct AuSched {
  bool init = false;
  F64 prevT = 0;
  F64 prevStrokes = 0, prevSpurts = 0;
  std::string prevState = "intro";
  F64 prevVel = 0, prevKiss = 0, prevRub = 0, prevHotFlash = 0, prevOralT = 0;
  bool prevSolo = false;
  bool rubArmed = false; // rub half-cycle tracker (js G._rubHalf/_rubN)
  long long rubHalf = 0;
  int rubCount = 0;
  F64 maleBreathT = 0; // js G.nBreathM (no C++ Game field yet)
  F64 soloT = 2.5;     // js G.soloMoanT tick lives in updateSolo3 (see TODO)
  std::vector<AuDelay> delay; // swallow lag + orgasm vocal cascade
};
AuSched sched_;

// js/mechanics.js registerStroke audio block (115-166), keyed off the strokes
// counter edge. Depth is the stroke-peak proxy, rate scales impact; lube
// branches use the JS 0.8 fallback default (no C++ field yet).
void auStroke(Game& g, bool hotNow) {
  const F64 depth = g.depth, plea = g.pleasure;
  if (g.oral > 0.5) {
    auSlurp(g, clamp(depth * 1.35 * 0.8, 0.4, 1.4));
    if (depth > 0.72) {
      auSynth("hum", 0.55);                        // playThroatHum
      if (pickChance() < 0.40) auSynth("gag", 0.6); // playOralGag
    }
    if (pickChance() < 0.32) auSynth("grunt", 0.55); // playGrunt
    return;
  }
  if (depth > 0.68) {
    const F64 r = g.rate > 0 ? g.rate : 1.0; // js: (G.rate||1)
    auSlap(g, clamp((depth - 0.45) * 1.5 * (r * 0.55), 0.35, 1.0), false);
  }
  if (depth > 0.28)
    auSquelch(g, 0.42 + 0.75 * (plea / 100.0) * 0.8 * depth);
  if (hotNow) return; // js if/else: hot-spot cry replaces the ambient moan
  if (plea > 36 && pickChance() < 0.48)
    auMoan(g, 0.44 + (plea / 100.0) * 0.4, 1.0);
  if (depth > 0.82 && pickChance() < 0.26) auSynth("grunt", 0.55);
}

// js/mechanics.js retraction block (468-479): velocity crossing -0.40.
void auRetract(Game& g) {
  if (g.oral > 0.5) {
    auSynth("lip", 0.6); // playLipPop
    return;
  }
  if (g.depth <= 0.15) {
    playOnce(stemNum("pullout", pickVariant(kPulloutN)), 0.7); // playPullout
  } else if (g.depth > 0.30) {
    auSlide(g, clamp(-g.vel * 0.35 * 0.8, 0.32, 1.15)); // playSlide
  }
}

// js/mechanics.js spurt audio block (253-258) + playCum routing.
void auSpurt(Game& g, AuSched& s) {
  auSquelch(g, 1.35);
  if (g.oral > 0.5) {
    // playCum swallow path (js setTimeout 550ms -> delay queue).
    if (s.delay.size() < 24) s.delay.push_back({g.t + 0.55, "swallow04", 0.75});
  } else if (g.depth > 0.5) {
    playOnce(stemNum("cum_in", pickVariant(kCumInN)), 0.8); // playCum inside
  } else {
    playOnce(stemNum("cum_out", pickVariant(kCumOutN)), 0.8); // outside
  }
  auSynth("grunt", 0.55);
  // pmul pitch (1.20+spurts*0.03) ignored — mixer has no playbackRate.
  auMoan(g, 0.92, 1.0);
}

// js/mechanics.js rub foley (504-521): phase-locked to the hand cycle
// floor(t*9.2/PI) with alternating push/pull; low zone squelches, else slides.
void auRub(Game& g, AuSched& s) {
  const long long half = (long long)std::floor(g.t * 9.2 / (TAU / 2.0));
  if (!s.rubArmed) {
    s.rubArmed = true;
    s.rubHalf = half;
    return;
  }
  if (half <= s.rubHalf) return;
  s.rubHalf = half;
  const F64 dir = (half % 2 == 0) ? 1.0 : -1.0;
  const F64 r = g.rub;
  const bool low = g.rubZone == 3; // RUBZONES[3] == "low"
  if (low) {
    auSquelch(g, 0.5 * r + 0.3);
    if (dir < 0) auSlide(g, 0.45 * r);
  } else {
    auSlide(g, 0.4 * r);
    if (dir > 0) auSquelch(g, 0.35 * r + 0.2);
  }
  s.rubCount++;
  // pmul pitch ignored (mixer has no playbackRate); vol mul kept.
  if (s.rubCount % (low ? 3 : 4) == 0)
    auMoan(g, low ? 0.48 : 0.32,
           (low ? 0.92 : 0.78) * (g.kiss > 0.5 ? 0.7 : 1.0));
}

// js/mechanics.js scheduleVoice (671-723). Speech text (say/tierLine) is
// skipped — no native speech UI yet — except the takeme/loveyou VOX stems,
// which exist on disk.
void auVoice(Game& g, AuSched& s, F64 dt) {
  g.nextMoan -= dt;
  if (g.rub > 0.5) g.nextMoan = std::max(g.nextMoan, 0.65);
  if (g.nextMoan <= 0) {
    const F64 a = g.pleasure / 100.0;
    auMoan(g, 0.22 + a * 0.78 + (g.state == "orgasm" ? 0.25 : 0.0),
           g.kiss > 0.5 ? 0.78 : 1.0);
    g.nextMoan = lerp(6.2, 1.4, a) * (0.65 + pickChance() * 0.75) *
                 (g.state == "orgasm" ? 0.42 : 1.0);
    if (a * 100.0 >= 72 && pickChance() < 0.12)
      playOnce("takeme", 0.8); // tierLine rare spoken line
  }

  g.nextBreath -= dt;
  if (g.nextBreath <= 0) {
    auSynth("breath", 0.24); // playBreath(false)
    g.nextBreath = 1.0 / lerp(0.18, 0.55, g.pleasure / 100.0);
  }

  if (g.tired) { // panting recovery (tiredT recovery stays in mech)
    s.maleBreathT -= dt;
    if (s.maleBreathT < 0) {
      auSynth("breath", 0.28); // playBreath(true)
      s.maleBreathT = 0.52;
    }
  }

  if (g.pleasure > 70) { // cardiac tachycardia (playHeart)
    g.nextBeat -= dt;
    if (g.nextBeat <= 0) {
      auSynth("heart", 0.5);
      const F64 bpm = 74 + (g.pleasure - 70) * 1.35 + g.orgasms * 8.5;
      g.nextBeat = 60.0 / std::min(185.0, bpm);
    }
  }
}

// js3d/anim3d.js updateSolo3 ambient moans. Timer owned here (file-local) so
// the future 3D port must not also tick G.soloMoanT — see TODO.
void auSolo(Game& g, AuSched& s, F64 dt) {
  if (!g.solo || g.state != "play") return;
  s.soloT -= dt;
  if (s.soloT <= 0) {
    auMoan(g, 0.35 + pickChance() * 0.25, 0.85);
    s.soloT = 3.5 + pickChance() * 3.5;
  }
}

void auEmitDue(Game& g, AuSched& s) {
  for (size_t i = 0; i < s.delay.size();) {
    const AuDelay& d = s.delay[i];
    if (d.due <= g.t) {
      playOnce(d.id, d.gain);
      if (d.mouthI >= 0 && !muted_) auMouth(g);
      s.delay.erase(s.delay.begin() + (long long)i);
    } else {
      i++;
    }
  }
}

} // namespace

void update(Game& g, F64 dt) {
  AuSched& s = sched_;
  if (!(dt >= 0)) dt = 0; // NaN-proof
  if (dt > 0.5) dt = 0.5;
  // Fresh-run / restart rewind: session counters only grow monotonically.
  if (!s.init || g.t < s.prevT || g.strokes < s.prevStrokes ||
      g.spurts < s.prevSpurts) {
    s = AuSched{};
    s.init = true;
    cueRng_ = Rng(1234567ull); // keep headless frames bit-stable
  }

  const std::string& st = g.state;
  const bool inPlay = (st == "play" || st == "orgasm");

  // --- counter / level edges (sampled before prev is overwritten) ---
  const bool strokeEdge = g.strokes > s.prevStrokes && inPlay;
  const bool retractEdge = inPlay && s.prevVel >= -0.40 && g.vel < -0.40;
  const bool hotNow = g.hotFlash > 0 && s.prevHotFlash <= 0 && inPlay;
  const bool spurtEdge = g.spurts > s.prevSpurts;
  const bool enteredOrgasm = s.prevState != "orgasm" && st == "orgasm";
  const bool enteredFinish = s.prevState == "climax" && st == "finish";
  const bool kissEdge = s.prevKiss < 0.4 && g.kiss >= 0.4 && st == "play";
  const bool rubEdge = s.prevRub < 0.12 && g.rub >= 0.12 && st == "play";
  const bool soloEdge = !s.prevSolo && g.solo;
  // oralT mode steps: 0->1 lick, 1->2 blow, any->0 off (js/oral.js toggleOral)
  const int oralNow = g.oralT >= 1.5 ? 2 : (g.oralT >= 0.5 ? 1 : 0);
  const int oralPrev = s.prevOralT >= 1.5 ? 2 : (s.prevOralT >= 0.5 ? 1 : 0);

  if (strokeEdge) auStroke(g, hotNow);
  if (hotNow) auMoan(g, 0.96, 1.0); // hot-spot cry (LINES.hot text: TODO say)
  if (retractEdge) auRetract(g);
  if (spurtEdge) auSpurt(g, s);

  if (enteredOrgasm) {
    // js beginOrgasm vocalScore: 9 syncopated moans + rare loveyou vox.
    // at=0 emits below in auEmitDue; pitch (pmul) ignored — no playbackRate.
    static const F64 kAt[9] = {0.00, 0.30, 0.58, 0.82,
                               1.12, 1.55, 2.15, 2.90, 3.90};
    for (F64 at : kAt) {
      if (s.delay.size() >= 24) break;
      s.delay.push_back({g.t + at, "moan02", moanGain(1.0), 1.0});
    }
    if (pickChance() < 0.18) playOnce("loveyou", 0.8);
  }
  if (enteredFinish) {
    auSynth("grunt", 0.55);
    auSynth("grunt", 0.55); // js spurt(): double grunt at finish
    if (pickChance() < 0.15) playOnce("loveyou", 0.8);
  }
  if (kissEdge) auMoan(g, 0.35, 0.85); // js input.js toggleKiss
  if (rubEdge) auMoan(g, g.rubZone == 3 ? 0.48 : 0.35, 0.85); // rubFeedback
  if (st == "play") {
    if (oralPrev == 0 && oralNow == 1) auMoan(g, 0.42, 0.90);
    else if (oralPrev == 1 && oralNow == 2) auMoan(g, 0.38, 0.85);
    else if (oralPrev > 0 && oralNow == 0) auSynth("lip", 0.6);
  }
  if (soloEdge) {
    auMoan(g, 0.4, 0.9); // js input.js toggleSolo
    s.soloT = 2.5;
  }

  // --- continuous schedulers (js calls scheduleVoice in play + climax) ---
  if (st == "play" && g.rub > 0.12 && g.oral < 0.5) auRub(g, s);
  else s.rubArmed = false;
  if (st == "play" || st == "climax") auVoice(g, s, dt);
  auSolo(g, s, dt);
  auEmitDue(g, s);

  // --- store edges ---
  s.prevT = g.t;
  s.prevStrokes = g.strokes;
  s.prevSpurts = g.spurts;
  s.prevState = st;
  s.prevVel = g.vel;
  s.prevKiss = g.kiss;
  s.prevRub = g.rub;
  s.prevHotFlash = g.hotFlash;
  s.prevOralT = g.oralT;
  s.prevSolo = g.solo;

  // TODOs for follow-up ports (out of scope: ONLY ag_audio.cpp is touched):
  // - Lube/engorgement (js G.lube): no C++ Game field yet. auSlap always
  //   takes the wet branch and 0.8 stands in for (G.lube||0.8); the mech port
  //   should add Game::lube and thread it through.
  // - Speech text (say/tierLine/LINES, G.sayCd/speech.until): skipped; needs
  //   the HUD/bubble port. Only the sampled takeme/loveyou VOX lines play.
  // - Synth-only cues (grunt/lip/hum/gag/heart/breath) are silent no-ops
  //   until a procedural synth or matching stems land in assets/.
  // - pmul/playbackRate pitch is dropped everywhere — mixer has no resample
  //   rate control on playOnce (mixInto resamples only for output rate).
  // - G.soloMoanT is deliberately NOT ticked here (file-local soloT instead)
  //   so js3d's updateSolo3 decrement has one owner when the 3D port lands.
}

// Platform pull: additive stereo f32 @rate. Nearest-neighbor resample.
void mixInto(float* out, int frames, int ch, int rate) {
  std::lock_guard<std::mutex> l(mtx_);
  for (auto it = voices_.begin(); it != voices_.end();) {
    const Clip& c = *it->clip;
    F64 step = (F64)c.rate / (rate > 0 ? rate : 44100);
    F64 pos = (F64)it->pos;
    size_t total = c.pcm.size() / 2;
    for (int i = 0; i < frames; i++) {
      size_t idx = (size_t)pos;
      if (idx >= total) break;
      float g = it->gain;
      out[i * ch] += c.pcm[idx * 2] * g;
      if (ch > 1) out[i * ch + 1] += c.pcm[idx * 2 + 1] * g;
      pos += step;
    }
    it->pos = (size_t)pos;
    if (it->pos >= total) it = voices_.erase(it);
    else ++it;
  }
}

} // namespace ag::audio
