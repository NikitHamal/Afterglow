// Afterglow native — ag_platform.cpp
// SDL3 platform (window/input/audio/HUD overlay) + headless runner.
// Scene Canvas (1280x720) uploads as ONE streaming texture per frame —
// the software rasterizer owns all pixels, so PC and Android match exactly.
#include "ag_platform.h"
#include "ag_2dmods.h"
#include "ag_3d.h"
#include "ag_d3_gl.h"
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
  g.t = o.t0; // sim owns the clock from here (mech advances g.t/g.sesT)
  if (!o.outDir.empty()) std::filesystem::create_directories(o.outDir);
  uint64_t first = 0;
  for (int f = 0; f < o.frames; f++) {
    app::step(g, rng, 1.0 / 60.0);
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
// NOTE: SDL_main.h is intentionally included only in main.cpp (the TU that
// defines main). Including it here too emitted a second WinMain -> LNK2005.

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

// ---- --3d GL HUD (file-local ortho overlays; scene GL owned by d3::App3d)
// GLSL ES 1.00 with GL_ES precision guards: compiles on real ES (Android)
// and desktop GL (Windows SDL_GL context). Positions are baked to NDC on
// the CPU so the shaders need no screen-size uniforms (the d3gl ABI has no
// Uniform2f); the text program needs only sampler + color.
const char* kGlHudVertC =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "attribute vec2 aPos;\n"
    "attribute vec4 aCol;\n"
    "varying vec4 vCol;\n"
    "void main() {\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "  vCol = aCol;\n"
    "}\n";
const char* kGlHudFragC =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "varying vec4 vCol;\n"
    "void main() { gl_FragColor = vCol; }\n";
const char* kGlHudVertT =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "varying vec2 vUV;\n"
    "void main() {\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "  vUV = aUV;\n"
    "}\n";
const char* kGlHudFragT =
    "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec4 uColor;\n"
    "void main() {\n"
    "  vec4 t = texture2D(uTex, vUV);\n"
    "  gl_FragColor = vec4(uColor.rgb, uColor.a * t.a);\n"
    "}\n";

d3gl::GLuint glHudCompile(d3gl::GLenum type, const char* src) {
  d3gl::GLuint s = d3gl::CreateShader(type);
  if (!s) return 0;
  d3gl::ShaderSource(s, 1, &src, nullptr);
  d3gl::CompileShader(s);
  d3gl::GLint st = 0;
  d3gl::GetShaderiv(s, d3gl::COMPILE_STATUS, &st);
  if (!st) {
    char log[512] = {0};
    d3gl::GetShaderInfoLog(s, (d3gl::GLsizei)sizeof(log) - 1, nullptr, log);
    std::fprintf(stderr, "afterglow --3d: hud shader compile: %s\n", log);
    d3gl::DeleteShader(s);
    return 0;
  }
  return s;
}

d3gl::GLuint glHudLink(d3gl::GLuint vs, d3gl::GLuint fs) {
  d3gl::GLuint p = d3gl::CreateProgram();
  if (!p) return 0;
  d3gl::AttachShader(p, vs);
  d3gl::AttachShader(p, fs);
  d3gl::LinkProgram(p);
  d3gl::GLint st = 0;
  d3gl::GetProgramiv(p, d3gl::LINK_STATUS, &st);
  if (!st) {
    char log[512] = {0};
    d3gl::GetProgramInfoLog(p, (d3gl::GLsizei)sizeof(log) - 1, nullptr, log);
    std::fprintf(stderr, "afterglow --3d: hud program link: %s\n", log);
    d3gl::DeleteProgram(p);
    return 0;
  }
  return p;
}

struct GlHud {
  d3gl::GLuint progC = 0, progT = 0, vbo = 0;
  d3gl::GLint aPosC = -1, aColC = -1;
  d3gl::GLint aPosT = -1, aUvT = -1, uTexT = -1, uColorT = -1;
  bool ok = false;
  bool init() {
    d3gl::GLuint vs = glHudCompile(d3gl::VERTEX_SHADER, kGlHudVertC);
    d3gl::GLuint fs = glHudCompile(d3gl::FRAGMENT_SHADER, kGlHudFragC);
    d3gl::GLuint vs2 = glHudCompile(d3gl::VERTEX_SHADER, kGlHudVertT);
    d3gl::GLuint fs2 = glHudCompile(d3gl::FRAGMENT_SHADER, kGlHudFragT);
    if (vs && fs) progC = glHudLink(vs, fs);
    if (vs2 && fs2) progT = glHudLink(vs2, fs2);
    if (vs) d3gl::DeleteShader(vs);
    if (fs) d3gl::DeleteShader(fs);
    if (vs2) d3gl::DeleteShader(vs2);
    if (fs2) d3gl::DeleteShader(fs2);
    if (!progC || !progT) {
      shutdown();
      return false;
    }
    aPosC = d3gl::GetAttribLocation(progC, "aPos");
    aColC = d3gl::GetAttribLocation(progC, "aCol");
    aPosT = d3gl::GetAttribLocation(progT, "aPos");
    aUvT = d3gl::GetAttribLocation(progT, "aUV");
    uTexT = d3gl::GetUniformLocation(progT, "uTex");
    uColorT = d3gl::GetUniformLocation(progT, "uColor");
    if (aPosC < 0 || aColC < 0 || aPosT < 0 || aUvT < 0 || uTexT < 0 ||
        uColorT < 0) {
      std::fprintf(stderr, "afterglow --3d: hud shader location missing\n");
      shutdown();
      return false;
    }
    d3gl::GenBuffers(1, &vbo);
    if (!vbo) {
      shutdown();
      return false;
    }
    ok = true;
    return true;
  }
  void shutdown() {
    if (vbo) {
      d3gl::DeleteBuffers(1, &vbo);
      vbo = 0;
    }
    if (progC) {
      d3gl::DeleteProgram(progC);
      progC = 0;
    }
    if (progT) {
      d3gl::DeleteProgram(progT);
      progT = 0;
    }
    ok = false;
  }
  void beginHud(int sw, int sh) {
    d3gl::Viewport(0, 0, (d3gl::GLsizei)sw, (d3gl::GLsizei)sh);
    d3gl::Disable(d3gl::DEPTH_TEST);
    d3gl::Enable(d3gl::BLEND);
    d3gl::BlendFunc(d3gl::SRC_ALPHA, d3gl::ONE_MINUS_SRC_ALPHA);
  }
  static const void* byteOff(std::uintptr_t o) {
    return reinterpret_cast<const void*>(o);
  }
  // Interleaved NDC x,y + rgba, stride 6 floats.
  void drawColor(d3gl::GLenum mode, const std::vector<float>& v, int sw,
                 int sh) {
    if (v.empty()) return;
    beginHud(sw, sh);
    d3gl::UseProgram(progC);
    d3gl::BindBuffer(d3gl::ARRAY_BUFFER, vbo);
    d3gl::BufferData(d3gl::ARRAY_BUFFER,
                     (d3gl::GLsizeiptr)(v.size() * sizeof(float)), v.data(),
                     d3gl::DYNAMIC_DRAW);
    d3gl::GLuint p = (d3gl::GLuint)aPosC, c = (d3gl::GLuint)aColC;
    d3gl::EnableVertexAttribArray(p);
    d3gl::VertexAttribPointer(p, 2, d3gl::FLOAT_, 0, 24, nullptr);
    d3gl::EnableVertexAttribArray(c);
    d3gl::VertexAttribPointer(c, 4, d3gl::FLOAT_, 0, 24, byteOff(8));
    d3gl::DrawArrays(mode, 0, (d3gl::GLsizei)(v.size() / 6));
    d3gl::DisableVertexAttribArray(p);
    d3gl::DisableVertexAttribArray(c);
    d3gl::BindBuffer(d3gl::ARRAY_BUFFER, 0);
  }
  // One cached glyph quad: 6 verts of NDC x,y + uv (24 floats).
  void drawText(d3gl::GLuint tex, const float* v, const ColorF& col, int sw,
                int sh) {
    beginHud(sw, sh);
    d3gl::UseProgram(progT);
    d3gl::ActiveTexture(d3gl::TEXTURE0);
    d3gl::BindTexture(d3gl::TEXTURE_2D, tex);
    d3gl::Uniform1i(uTexT, 0);
    const float c[4] = {col.r, col.g, col.b, col.a};
    d3gl::Uniform4fv(uColorT, 1, c);
    d3gl::BindBuffer(d3gl::ARRAY_BUFFER, vbo);
    d3gl::BufferData(d3gl::ARRAY_BUFFER,
                     (d3gl::GLsizeiptr)(24 * sizeof(float)), v,
                     d3gl::DYNAMIC_DRAW);
    d3gl::GLuint p = (d3gl::GLuint)aPosT, t = (d3gl::GLuint)aUvT;
    d3gl::EnableVertexAttribArray(p);
    d3gl::VertexAttribPointer(p, 2, d3gl::FLOAT_, 0, 16, nullptr);
    d3gl::EnableVertexAttribArray(t);
    d3gl::VertexAttribPointer(t, 2, d3gl::FLOAT_, 0, 16, byteOff(8));
    d3gl::DrawArrays(d3gl::TRIANGLES, 0, 6);
    d3gl::DisableVertexAttribArray(p);
    d3gl::DisableVertexAttribArray(t);
    d3gl::BindTexture(d3gl::TEXTURE_2D, 0);
    d3gl::BindBuffer(d3gl::ARRAY_BUFFER, 0);
  }
};

// GL twin of TextCache above: identical stb_truetype raster (white glyph +
// coverage alpha), uploaded with d3gl::TexImage2D instead of SDL textures.
// Raster block is intentionally mirrored so the 2D path stays untouched.
struct GlTextCache {
  Font *sora = nullptr, *corm = nullptr;
  struct Item {
    d3gl::GLuint tex = 0;
    int w = 0, h = 0;
  };
  std::unordered_map<std::string, Item> m;
  void clear() {
    for (auto& kv : m) d3gl::DeleteTextures(1, &kv.second.tex);
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
    d3gl::GLuint tex = 0;
    d3gl::GenTextures(1, &tex);
    if (!tex) return nullptr;
    d3gl::BindTexture(d3gl::TEXTURE_2D, tex);
    d3gl::TexParameteri(d3gl::TEXTURE_2D, d3gl::TEXTURE_MIN_FILTER,
                        d3gl::LINEAR);
    d3gl::TexParameteri(d3gl::TEXTURE_2D, d3gl::TEXTURE_MAG_FILTER,
                        d3gl::LINEAR);
    d3gl::TexParameteri(d3gl::TEXTURE_2D, d3gl::TEXTURE_WRAP_S,
                        d3gl::CLAMP_TO_EDGE);
    d3gl::TexParameteri(d3gl::TEXTURE_2D, d3gl::TEXTURE_WRAP_T,
                        d3gl::CLAMP_TO_EDGE);
    d3gl::PixelStorei(d3gl::UNPACK_ALIGNMENT, 1);
    d3gl::TexImage2D(d3gl::TEXTURE_2D, 0, d3gl::RGBA,
                     (d3gl::GLsizei)tw, (d3gl::GLsizei)th, 0, d3gl::RGBA,
                     d3gl::UNSIGNED_BYTE, buf.data());
    d3gl::BindTexture(d3gl::TEXTURE_2D, 0);
    Item it2{tex, tw, th};
    auto inserted = m.emplace(std::move(k), it2);
    return &inserted.first->second;
  }
};

// --3d loop: own SDL video/audio setup on an OPENGL window (same SDL
// activity path on Android); scene via d3::App3d, HUD via hud::build over
// the file-local ortho GL above. Returns 0 on a clean run (user quit);
// nonzero when GL is unavailable — the caller then falls back to 2D.
int runGl3d() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::fprintf(stderr, "afterglow --3d: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_Window* win = SDL_CreateWindow("Afterglow", VW, VH,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
  if (!win) {
    std::fprintf(stderr, "afterglow --3d: window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  if (!ctx) {
    std::fprintf(stderr, "afterglow --3d: GL context: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }
  SDL_GL_MakeCurrent(win, ctx);
  SDL_GL_SetSwapInterval(1); // vsync parity with the 2D SDL_Renderer path
  // Prescribed cast: SDL_GL_GetProcAddress reinterpreted through void*.
  d3gl::GetProc get = (d3gl::GetProc)(void*)SDL_GL_GetProcAddress;
  if (!d3gl::load(get)) {
    std::fprintf(stderr, "afterglow --3d: GL loader missing symbols\n");
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  int sw = VW, sh = VH;
  SDL_GetWindowSize(win, &sw, &sh);
  d3::App3d app3;
  if (!app3.init(findAssets(), sw, sh)) {
    std::fprintf(stderr, "afterglow --3d: engine init failed\n");
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }
  GlHud hudGl;
  if (!hudGl.init()) {
    std::fprintf(stderr, "afterglow --3d: hud GL init failed\n");
    app3.shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

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
  GlTextCache gtc{&sora, &corm};

  Game g;
  g.reset();
  Rng rng(1234);
  bool mute = false;
  bool running = true;
  uint64_t prev = SDL_GetTicksNS();
  double acc = 0;
  const double STEP = 1.0 / 120.0;
  std::vector<float> triVerts, lineVerts;
  triVerts.reserve(4096);
  lineVerts.reserve(1024);

  while (running) {
    SDL_GetWindowSize(win, &sw, &sh);
    app3.eng.resize(sw, sh);
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
    // Fixed-step parity with the 2D path; each step re-renders the scene
    // and the last step wins (split step/render once Engine matures).
    while (acc >= STEP && n < 8) {
      app3.frame(g, rng, STEP);
      acc -= STEP;
      n++;
    }
    if (n == 0) app3.frame(g, rng, 0.0); // keep presenting at high refresh

    // HUD overlay (screen space, mirrors DOM HUD + 2D overlay): rects as
    // colored ortho quads, text as cached glyph quads.
    hud::HudFrame hf = hud::build(g, sw, sh);
    triVerts.clear();
    lineVerts.clear();
    auto emitFill = [&](const hud::HudRect& rc) {
      float x0 = (float)(rc.x / sw * 2.0 - 1.0);
      float y0 = (float)(1.0 - rc.y / sh * 2.0);
      float x1 = (float)((rc.x + rc.w) / sw * 2.0 - 1.0);
      float y1 = (float)(1.0 - (rc.y + rc.h) / sh * 2.0);
      float cr = rc.fill.r, cg = rc.fill.g, cb = rc.fill.b, ca = rc.fill.a;
      const float v[36] = {x0, y0, cr, cg, cb, ca, x1, y0, cr, cg, cb, ca,
                           x0, y1, cr, cg, cb, ca, x1, y0, cr, cg, cb, ca,
                           x1, y1, cr, cg, cb, ca, x0, y1, cr, cg, cb, ca};
      triVerts.insert(triVerts.end(), v, v + 36);
    };
    auto emitOutline = [&](const hud::HudRect& rc) {
      float x0 = (float)(rc.x / sw * 2.0 - 1.0);
      float y0 = (float)(1.0 - rc.y / sh * 2.0);
      float x1 = (float)((rc.x + rc.w) / sw * 2.0 - 1.0);
      float y1 = (float)(1.0 - (rc.y + rc.h) / sh * 2.0);
      float cr = rc.fill.r, cg = rc.fill.g, cb = rc.fill.b, ca = rc.fill.a;
      const float v[48] = {
          x0, y0, cr, cg, cb, ca, x1, y0, cr, cg, cb, ca,
          x1, y0, cr, cg, cb, ca, x1, y1, cr, cg, cb, ca,
          x1, y1, cr, cg, cb, ca, x0, y1, cr, cg, cb, ca,
          x0, y1, cr, cg, cb, ca, x0, y0, cr, cg, cb, ca};
      lineVerts.insert(lineVerts.end(), v, v + 48);
    };
    for (auto& rc : hf.rects) {
      if (rc.outline) emitOutline(rc);
      else emitFill(rc);
    }
    hudGl.drawColor(d3gl::TRIANGLES, triVerts, sw, sh);
    hudGl.drawColor(d3gl::LINES, lineVerts, sw, sh);
    for (auto& tx : hf.texts) {
      int px = std::max(8, (int)tx.size);
      if (auto* it = gtc.get(tx.text, tx.title, px, tx.color)) {
        float x0 = (float)(tx.x / sw * 2.0 - 1.0);
        float y0 = (float)(1.0 - tx.y / sh * 2.0);
        float x1 = (float)((tx.x + it->w) / sw * 2.0 - 1.0);
        float y1 = (float)(1.0 - (tx.y + it->h) / sh * 2.0);
        // Row 0 in memory is the glyph top; GL texcoord (0,0) is the
        // first byte, so top verts sample v=0.
        const float v[24] = {x0, y0, 0, 0, x1, y0, 1, 0, x0, y1, 0, 1,
                             x1, y0, 1, 0, x1, y1, 1, 1, x0, y1, 0, 1};
        hudGl.drawText(it->tex, v, tx.color, sw, sh);
      }
    }
    SDL_GL_SwapWindow(win);
  }

  gtc.clear();
  hudGl.shutdown();
  app3.shutdown();
  if (audio) SDL_DestroyAudioStream(audio);
  audio::shutdown();
  SDL_GL_DestroyContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
} // namespace

int runGame(int argc, char** argv) {
  bool want3d = false;
  for (int i = 1; i < argc; i++)
    if (!std::strcmp(argv[i], "--3d")) want3d = true;
  if (want3d) {
    // Desktop/Android GL path owns its window + context; on any GL failure
    // runGl3d cleans up and returns nonzero so we fall back to 2D below.
    if (runGl3d() == 0) return 0;
    std::fprintf(stderr, "afterglow: --3d GL unavailable, falling back to 2D\n");
  }
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
