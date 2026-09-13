// Afterglow native — ag_prelude.h
// Shared foundation: JS-number semantics (double), math helpers mirroring
// js/core.js, sRGB color + CSS color-string parsing, deterministic RNG.
// Zero dependencies beyond the C++20 standard library.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ag {

using F64 = double;
inline constexpr F64 TAU = 6.283185307179586476925286766559;
inline constexpr int VW = AG_VW; // 1280 virtual pixels (js/core.js W)
inline constexpr int VH = AG_VH; // 720  virtual pixels (js/core.js H)

// ---- math (1:1 with js/core.js) ----
inline F64 clamp(F64 v, F64 a, F64 b) { return v < a ? a : (v > b ? b : v); }
inline F64 lerp(F64 a, F64 b, F64 t) { return a + (b - a) * t; }
// smoothstep helper `sm(a,b,v)` from core.js
inline F64 sm(F64 a, F64 b, F64 v) {
  F64 t = clamp((v - a) / (b - a), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

// ---- color ----
// Float sRGB, non-premultiplied, components in [0,1]. Canvas gradients in the
// web build interpolate in sRGB, so we keep stops in sRGB too.
struct ColorF {
  float r = 0, g = 0, b = 0, a = 1;
};

struct RGBA8 {
  uint8_t r = 0, g = 0, b = 0, a = 255;
};

inline uint8_t toU8(float v) {
  long q = std::lround(v * 255.0f);
  return (uint8_t)(q < 0 ? 0 : (q > 255 ? 255 : q));
}
inline RGBA8 pack(ColorF c) { return {toU8(c.r), toU8(c.g), toU8(c.b), toU8(c.a)}; }
inline ColorF unpack(RGBA8 p) {
  const float k = 1.0f / 255.0f;
  return {(float)p.r * k, (float)p.g * k, (float)p.b * k, (float)p.a * k};
}

// CSS color strings used across js/*: #rgb #rrggbb #rrggbbaa rgb() rgba()
// plus 'transparent'. Returns nullopt when unparseable (caller falls back).
inline std::optional<ColorF> parseColor(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  if (s.empty()) return std::nullopt;
  if (s == "transparent") return ColorF{0, 0, 0, 0};
  auto hex1 = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  if (s.front() == '#') {
    s.remove_prefix(1);
    auto byte = [&](size_t i) -> float {
      int h = hex1(s[i * 2]), l = hex1(s[i * 2 + 1]);
      if (h < 0 || l < 0) return -1.0f;
      return (float)(h * 16 + l) / 255.0f;
    };
    if (s.size() == 3) {
      int r = hex1(s[0]), g = hex1(s[1]), b = hex1(s[2]);
      if (r < 0 || g < 0 || b < 0) return std::nullopt;
      float k = 1.0f / 15.0f;
      return ColorF{(float)r * k, (float)g * k, (float)b * k, 1};
    }
    if (s.size() == 6 || s.size() == 8) {
      float r = byte(0), g = byte(1), b = byte(2);
      if (r < 0 || g < 0 || b < 0) return std::nullopt;
      float a = 1.0f;
      if (s.size() == 8) {
        a = byte(3);
        if (a < 0) return std::nullopt;
      }
      return ColorF{r, g, b, a};
    }
    return std::nullopt;
  }
  // rgb()/rgba() — comma separated, alpha may be float.
  if ((s.size() > 4 && s.substr(0, 4) == "rgb(" && s.back() == ')') ||
      (s.size() > 5 && s.substr(0, 5) == "rgba(" && s.back() == ')')) {
    size_t lp = s.find('(');
    std::string body(s.substr(lp + 1, s.size() - lp - 2));
    for (char& c : body)
      if (c == ',') c = ' ';
    float v[4] = {0, 0, 0, 1};
    int n = std::sscanf(body.c_str(), "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
    if (n < 3) return std::nullopt;
    auto ch = [](float x) { return clamp((F64)x / 255.0, 0.0, 1.0); };
    return ColorF{(float)ch(v[0]), (float)ch(v[1]), (float)ch(v[2]),
                  (float)clamp(v[3], 0.0, 1.0)};
  }
  if (s == "black") return ColorF{0, 0, 0, 1};
  if (s == "white") return ColorF{1, 1, 1, 1};
  return std::nullopt;
}

inline ColorF mustColor(std::string_view s) {
  if (auto c = parseColor(s)) return *c;
  return ColorF{1, 0, 1, 1}; // magenta = unported color literal, easy to spot
}

// js/chars.js hexToRgb / lerpHex (tone derivation in skin).
inline std::array<float, 3> hexToRgb(std::string_view h) {
  ColorF c = mustColor(h);
  return {c.r * 255.0f, c.g * 255.0f, c.b * 255.0f};
}
inline std::string lerpHex(std::string_view a, std::string_view b, F64 t);

// ---- deterministic RNG ----
// Web uses Math.random (nondeterministic). The port uses a seeded xorshift64*
// so headless parity frames are bit-stable. Same call sequence => same frame.
struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
  uint64_t next() {
    uint64_t x = s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s = x;
    return x * 0x2545F4914F6CDD1Dull;
  }
  F64 next01() { return (F64)(next() >> 11) * (1.0 / 9007199254740992.0); }
  F64 range(F64 a, F64 b) { return a + next01() * (b - a); }
};

} // namespace ag
