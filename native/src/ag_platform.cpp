// Afterglow native — ag_platform.cpp
// SDL3 platform (window/input/audio/HUD overlay) + headless runner.
// Scene Canvas (1280x720) uploads as ONE streaming texture per frame —
// the software rasterizer owns all pixels, so PC and Android match exactly.
#include "ag_platform.h"
#include "ag_2dmods.h"
#include "ag_3d.h"
#include "ag_hud.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace ag {

int headlessRun(const HeadlessOpts& o) {
  Game g;
  g.reset();
  g.state = "play";
  g.view = o.view;
  g.pleasure = o.pleasure;
  g.depth = o.depth;
  if (o.view == "oral") {
    g.view = "side";
    g.oral = 1;
  }
  Rng rng(1234);
  Canvas cv;
  if (!o.outDir.empty()) std::filesystem::create_directories(o.outDir);
  uint64_t first = 0;
  for (int f = 0; f < o.frames; f++) {
    g.t = o.t0 + f / 60.0;
    app::step(g, rng, 1.0 / 60.0);
    g.t = o.t0 + f / 60.0; // step() must not advance wall-clock art time
    app::draw(cv, g);
    uint64_t h = cv.hash();
    if (f == 0) first = h;
    std::printf("frame %d hash %llu cache %llu\n", f,
                (unsigned long long)h, (unsigned long long)cv.cacheSize());
    if (!o.outDir.empty()) {
      char p[512];
      std::snprintf(p, sizeof(p), "%s/f%03d.ppm", o.outDir.c_str(), f);
      cv.writePPM(p);
    }
  }
  // identity: cleared-cache redraw of the final state must match exactly
  uint64_t before = cv.hash();
  cv.cacheClear();
  app::draw(cv, g);
  uint64_t after = cv.hash();
  std::printf("identity %s cache %llu\n", before == after ? "OK" : "MISMATCH",
              (unsigned long long)cv.cacheSize());
  (void)first;
  return before == after ? 0 : 1;
}

#ifndef AG_WITH_SDL

int runGame(int argc, char** argv) {
  HeadlessOpts o;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) o.frames = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) o.outDir = argv[++i];
    else if (!std::strcmp(argv[i], "--view") && i + 1 < argc) o.view = argv[++i];
  }
  return headlessRun(o);
}

#else // AG_WITH_SDL

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace {
input::Key mapKey(SDL_Keycode k) {
  using K = input::Key;
  switch (k) {
  case SDLK_SPACE: return K::Space;
  case SDLK_E: return K::E;
  case SDLK_C: return K::C;
  case SDLK_TAB: return K::Tab;
  case SDLK_O: return K::O;
  case SDLK_S: return K::S;
  case SDLK_X: return K::X;
  case SDLK_V: return K::V;
  case SDLK_Z: return K::Z;
  case SDLK_P: return K::P;
  case SDLK_M: return K::M;
  case SDLK_F: return K::F;
  case SDLK_T: return K::T;
  case SDLK_Q: return K::Q;
  case SDLK_L: return K::L;
  case SDLK_R: return K::R;
  case SDLK_1: return K::Digit1;
  case SDLK_2: return K::Digit2;
  case SDLK_3: return K::Digit3;
  case SDLK_4: return K::Digit4;
  default: return K::Unknown;
  }
}

struct Font {
  stbtt_fontinfo info{};
  std::vector<unsigned char> data;
  bool ok = false;
  bool load(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    data.resize((size_t)n);
    bool good = std::fread(data.data(), 1, (size_t)n, f) == (size_t)n;
    std::fclose(f);
    ok = good && stbtt_InitFont(&info, data.data(), 0);
    return ok;
  }
};

struct TextCache {
  SDL_Renderer* r = nullptr;
  Font *sora = nullptr, *corm = nullptr;
  struct Item {
    SDL_Texture* t = nullptr;
    int w = 0, h = 0;
  };
  std::unordered_map<std::string, Item> m;
  void clear() {
    for (auto& kv : m) SDL_DestroyTexture(kv.second.t);
    m.clear();
  }
  Item* get(const std::string& text, bool title, int px, ColorF col) {
    char key[32];
    std::snprintf(key, sizeof(key), "|%d|%d|%02x%02x%02x", px, (int)title,
                  toU8(col.r), toU8(col.g), toU8(col.b));
    std::string k = text + key;
    auto it = m.find(k);
    if (it != m.end()) return &it->second;
    Font* f = title ? corm : sora;
    if (!f || !f->ok) return nullptr;
    float scale = stbtt_ScaleForPixelHeight(&f->info, (float)px);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&f->info, &ascent, &descent, &gap);
    int capH = (int)((ascent - descent) * scale);
    int tw = 0;
    for (unsigned char c : text) {
      int adv = 0, lsb = 0;
      stbtt_GetCodepointHMetrics(&f->info, c, &adv, &lsb);
      tw += (int)(adv * scale);
      int kern = stbtt_GetCodepointKernAdvance(&f->info, c, c);
      (void)kern;
    }
    if (tw <= 0) tw = 1;
    int th = capH > 0 ? capH + 4 : px;
    std::vector<uint8_t> buf((size_t)tw * (size_t)th * 4, 0);
    int x = 0;
    for (unsigned char c : text) {
      int w = 0, h = 0, xo = 0, yo = 0;
      unsigned char* bm = stbtt_GetCodepointBitmap(
          &f->info, scale, scale, c, &w, &h, &xo, &yo);
      int yoff = (int)(ascent * scale) + yo;
      for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
          int dx = x + xo + xx, dy = yoff + yy;
          if ((unsigned)dx < (unsigned)tw && (unsigned)dy < (unsigned)th) {
            uint8_t a = bm[yy * w + xx];
            size_t o = ((size_t)dy * (size_t)tw + (size_t)dx) * 4;
            buf[o] = 255;
            buf[o + 1] = 255;
            buf[o + 2] = 255;
            buf[o + 3] = a;
          }
        }
      stbtt_FreeBitmap(bm, nullptr);
      int adv = 0, lsb = 0;
      stbtt_GetCodepointHMetrics(&f->info, c, &adv, &lsb);
      x += (int)(adv * scale);
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(tw, th, SDL_PIXELFORMAT_RGBA32,
                                           buf.data(), tw * 4);
    if (!s) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_DestroySurface(s);
    if (!t) return nullptr;
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(t, toU8(col.r), toU8(col.g), toU8(col.b));
    Item it2{t, tw, th};
    auto inserted = m.emplace(std::move(k), it2);
    return &inserted.first->second;
  }
};

void SDLCALL audioCb(void*, SDL_AudioStream* s, int addl, int) {
  static thread_local std::vector<float> buf;
  int frames = addl / (int)sizeof(float) / 2;
  if (frames <= 0) return;
  buf.assign((size_t)frames * 2, 0.0f);
  audio::mixInto(buf.data(), frames, 2, 44100);
  SDL_PutAudioStreamData(s, buf.data(), frames * 2 * (int)sizeof(float));
}

const char* findAssets() {
  static const char* cands[] = {"../assets", "../../assets", "assets",
                                "../..//assets"};
  for (const char* c : cands) {
    std::error_code ec;
    if (std::filesystem::is_directory(c, ec)) return c;
  }
  return "";
}
const char* findFonts() {
  static const char* cands[] = {"fonts", "build/win64/fonts", "../fonts",
                                "../../fonts"};
  static char joined[512];
  for (const char* c : cands) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(std::string(c) + "/Sora-Regular.ttf",
                                         ec)) {
      std::snprintf(joined, sizeof(joined), "%s", c);
      return joined;
    }
  }
  return "fonts";
}
} // namespace

int runGame(int argc, char** argv) {
  (void)argc;
  (void)argv;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* win =
      SDL_CreateWindow("Afterglow", VW, VH, SDL_WINDOW_RESIZABLE);
  if (!win) {
    std::fprintf(stderr, "window: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(win, NULL);
  SDL_SetRenderVSync(ren, 1);
  SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STREAMING, VW, VH);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);

  audio::init(findAssets());
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  spec.freq = 44100;
  SDL_AudioStream* audio = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCb, nullptr);
  if (audio) SDL_ResumeAudioStreamDevice(audio);

  Font sora, corm;
  std::string fd = findFonts();
  if (!sora.load((fd + "/Sora-Regular.ttf").c_str())) {
    // Android: fonts ship inside the APK — read via SDL's asset IO.
    auto loadAsset = [&](Font& f, const char* name) {
      SDL_IOStream* io = SDL_IOFromFile(name, "rb");
      if (!io) return;
      Sint64 n = SDL_GetIOSize(io);
      if (n > 0 && n < 16 * 1024 * 1024) {
        f.data.resize((size_t)n);
        if (SDL_ReadIO(io, f.data.data(), (size_t)n) == (size_t)n)
          f.ok = stbtt_InitFont(&f.info, f.data.data(), 0);
      }
      SDL_CloseIO(io);
    };
    loadAsset(sora, "fonts/Sora-Regular.ttf");
    loadAsset(corm, "fonts/CormorantGaramond-Medium.ttf");
  } else {
    corm.load((fd + "/CormorantGaramond-Medium.ttf").c_str());
  }
  TextCache tc{ren, &sora, &corm};

  Game g;
  g.reset();
  Rng rng(1234);
  Canvas cv;
  bool mute = false;
  bool running = true;
  uint64_t prev = SDL_GetTicksNS();
  double acc = 0;
  const double STEP = 1.0 / 120.0;

  while (running) {
    int sw = 0, sh = 0;
    SDL_GetWindowSize(win, &sw, &sh);
    double sc = std::min(sw / (double)VW, sh / (double)VH);
    double ox = (sw - VW * sc) * 0.5, oy = (sh - VH * sc) * 0.5;
    auto toVirtual = [&](float sx, float sy) {
      return input::Pointer{(sx - ox) / sc, (sy - oy) / sc, true};
    };

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_EVENT_QUIT:
        running = false;
        break;
      case SDL_EVENT_KEY_DOWN:
        if (!e.key.repeat) {
          if (e.key.key == SDLK_M) {
            mute = !mute;
            audio::setMuted(mute);
          } else if (e.key.key == SDLK_F) {
            bool fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(win, fs ? false : true);
          } else {
            input::keyDown(g, mapKey(e.key.key));
          }
        }
        break;
      case SDL_EVENT_KEY_UP:
        input::keyUp(g, mapKey(e.key.key));
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          auto p = toVirtual(e.button.x, e.button.y);
          input::pointerDown(g, p);
        }
        break;
      case SDL_EVENT_MOUSE_MOTION:
        if (e.motion.state & SDL_BUTTON_LMASK) {
          auto p = toVirtual(e.motion.x, e.motion.y);
          input::pointerMove(g, p);
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          auto p = toVirtual(e.button.x, e.button.y);
          p.down = false;
          input::pointerUp(g, p);
        }
        break;
      default:
        break;
      }
    }

    uint64_t now = SDL_GetTicksNS();
    double dt = (now - prev) / 1e9;
    prev = now;
    if (dt > 0.25) dt = 0.25;
    acc += dt;
    int n = 0;
    while (acc >= STEP && n < 8) {
      app::step(g, rng, STEP);
      acc -= STEP;
      n++;
    }

    app::draw(cv, g);
    SDL_UpdateTexture(tex, NULL, cv.pixels(), VW * 4);
    SDL_SetRenderDrawColor(ren, 5, 3, 5, 255);
    SDL_RenderClear(ren);
    SDL_FRect dst{(float)ox, (float)oy, (float)(VW * sc), (float)(VH * sc)};
    SDL_RenderTexture(ren, tex, NULL, &dst);

    // HUD overlay (screen space, mirrors DOM HUD)
    hud::HudFrame hf = hud::build(g, sw, sh);
    for (auto& rc : hf.rects) {
      SDL_SetRenderDrawColor(ren, toU8(rc.fill.r), toU8(rc.fill.g),
                             toU8(rc.fill.b), toU8(rc.fill.a));
      SDL_FRect r{(float)rc.x, (float)rc.y, (float)rc.w, (float)rc.h};
      if (rc.outline) SDL_RenderRect(ren, &r);
      else SDL_RenderFillRect(ren, &r);
    }
    for (auto& tx : hf.texts) {
      int px = std::max(8, (int)tx.size);
      if (auto* it = tc.get(tx.text, tx.title, px, tx.color)) {
        SDL_FRect r{(float)tx.x, (float)tx.y, (float)it->w, (float)it->h};
        SDL_RenderTexture(ren, it->t, NULL, &r);
      }
    }
    SDL_RenderPresent(ren);
  }

  tc.clear();
  if (audio) SDL_DestroyAudioStream(audio);
  audio::shutdown();
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}

#endif // AG_WITH_SDL
} // namespace ag
