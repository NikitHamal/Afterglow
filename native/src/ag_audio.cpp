// Afterglow native — ag_audio.cpp
// Bank decode (WAV hand-parsed + OGG via stb_vorbis) + tiny polyphonic mixer.
// Backend-agnostic: the platform pulls PCM via mixInto() (SDL audio stream on
// PC+Android, direct call in headless parity). Full js/audio.js cue scheduling
// (moan/stroke/climax timing) lands in update(); mixer + decode are real now.
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include "ag_2dmods.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>

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

void update(Game&, F64) {
  // TODO js/audio.js: breath/moan/beat scheduling, stroke foley by rate,
  // climax layering, kiss/rub one-shots. Mixer above is real.
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
