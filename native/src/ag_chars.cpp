// Afterglow native — ag_chars.cpp
// Port of js/chars.js (108L: CHARS preset table + getSkin palette) and the
// canvas half of js/charui.js (396L: customize overlay P).
//
// SCOPE: renderer + state mutations only. The web build drives DOM
// (#charSel cards, #custPanel inputs/sliders, localStorage, GLB hooks); here
// the canvas draws text-free shapes (rects/gradients/knobs/swatches) and the
// platform draws all text labels on top. Tap() hit-tests virtual 1280x720
// coords and mutates g.ch directly.
//
// BUILD WIRING TODO: add this file to native/CMakeLists.txt afterglow_core
// sources AND delete the chars:: / charui:: stubs in ag_port_todo.cpp
// (duplicate definitions until then).
#include "ag_2dmods.h"
#include "ag_gfx.h"

#include <algorithm>
#include <unordered_map>

namespace ag {
namespace {

// ---- file-local state (TODO: move into Game once it is writable) ----
// js/charui.js keeps `custOpen` as a module var + #custPanel DOM class.
// Game has no overlay flag, so the flag lives here (shared across Game
// instances — single-session native builds only have one).
static bool custOpen_ = false;
// js/chars.js CHARS extras with no CharPreset field: isGLB / glbPath, plus
// glbCustom part visibility + rune tint (customize panel GLB section).
// TODO: add CharPreset::isGLB/glbPath/glbCustom and persist via platform
// prefs (js uses localStorage CHAR_KEY); wire toggles to glbSetPartVisible.
static std::unordered_map<std::string, bool> glbParts_;
static std::string glbRune_ = "#ff3366"; // js #cpRunePick default value
inline bool glbPartOn(const std::string& p) {
  auto it = glbParts_.find(p);
  return it == glbParts_.end() ? true : it->second;
}

// ---- js/chars.js lerpHex3 ----
inline std::string lerpHex3(std::string_view a, std::string_view m,
                            std::string_view b, F64 t) {
  return t < 0.5 ? lerpHex(a, m, t * 2.0) : lerpHex(m, b, (t - 0.5) * 2.0);
}

// ---- js/chars.js CHARS table (verbatim values; quotes as UTF-8 escapes
// so bytes are exact regardless of compiler execution charset) ----
struct PresetRow {
  const char* preset;
  const char* name;
  const char* quote;
  F64 skinTone;
  const char* hairColor;
  const char* hairStyle;
  F64 bodyScale, breastSize;
  const char* nippleColor;
  const char* blushColor;
  const char* lipColor;
  const char* eyeColor;
  const char* pubicHair;
  bool isGLB;
  const char* glbPath; // TODO: no CharPreset field; kept for 3D wiring
};
// clang-format off
static const PresetRow PRESETS[6] = {
  {"yuki", "Yuki",
   "\xE2\x80\x9C" "Mm" "\xE2\x80\xA6" " finally we" "\xE2\x80\x99"
   "re alone. You remember how I like it" "\xE2\x80\xA6" " right?" "\xE2\x80\x9D",
   0.18, "#231318", "long", 0.45, 0.45,
   "#c25f63", "#e86070", "#b3555f", "#4a2c33", "trim",
   false, ""},
  {"mara", "Mara",
   "\xE2\x80\x9C" "Hey" "\xE2\x80\xA6" " don" "\xE2\x80\x99"
   "t keep me waiting. Come closer" "\xE2\x80\xA6" "\xE2\x80\x9D",
   0.46, "#1a0e0a", "long", 0.65, 0.68,
   "#a04848", "#c05858", "#943040", "#3a2018", "trim",
   false, ""},
  {"sable", "Sable",
   "\xE2\x80\x9C" "You sure you can handle this" "\xE2\x80\xA6"
   "? Good." "\xE2\x80\x9D",
   0.76, "#14080a", "natural", 0.58, 0.55,
   "#6a2828", "#883040", "#6a2435", "#2a1208", "full",
   false, ""},
  {"goatchan", "Goat-chan",
   "\xE2\x80\x9C" "Mmh" "\xE2\x80\xA6" " are you ready for something wild? Don"
   "\xE2\x80\x99" "t hold back" "\xE2\x80\xA6" " " "\xE2\x99\xA5" "\xE2\x80\x9D",
   0.15, "#961e32", "horns", 0.50, 0.85,
   "#f090a0", "#f05068", "#d04050", "#cc1030", "trim",
   true, "assets/goatchan/goatchan.glb"},
  {"kiyoko", "Kiyoko",
   "\xE2\x80\x9C" "Mmm" "\xE2\x80\xA6" " start with my ears? Then don"
   "\xE2\x80\x99" "t you dare stop" "\xE2\x80\xA6" " " "\xE2\x99\xA5" "\xE2\x80\x9D",
   0.30, "#e85a10", "long", 0.48, 0.62,
   "#b05050", "#e86070", "#b3555f", "#5a3018", "trim",
   true, "assets/kiyoko.005.glb"},
  {"anime", "Anime",
   "\xE2\x80\x9C" "Ehehe" "\xE2\x80\xA6" " senpai finally picked me! Be gentle"
   "\xE2\x80\xA6" " maybe" "\xE2\x99\xA5" "\xE2\x80\x9D",
   0.18, "#ff7ab0", "long", 0.47, 0.60,
   "#c06070", "#f08090", "#d06080", "#6a3050", "trim",
   true, "assets/free_download_female_anime.glb"},
};
// clang-format on
inline const PresetRow& findPreset(const std::string& key) {
  for (const auto& p : PRESETS)
    if (key == p.preset) return p;
  return PRESETS[0]; // js: CHARS[key] || CHARS.yuki
}
inline bool presetIsGLB(const Game& g) {
  // js syncPanelFromChar: G.char.isGLB || preset === 'goatchan'
  return findPreset(g.ch.preset).isGLB || g.ch.preset == "goatchan";
}

// ---- customize-panel option tables (verbatim js/charui.js values) ----
static constexpr const char* HAIR_SW[5] = {"#231318", "#1a0e0a", "#3d1e10",
                                           "#6b3c0e", "#c9a060"};
static constexpr const char* LIP_SW[5] = {"#b3555f", "#943040", "#6a2435",
                                          "#d07070", "#c08070"};
static constexpr const char* NIP_SW[5] = {"#c25f63", "#a04848", "#6a2828",
                                          "#e08080", "#b07060"};
static constexpr const char* HAIR_ST[3] = {"long", "bob", "natural"};
static constexpr const char* FPV_K[4] = {"full", "breasts", "hips", "face"};
static constexpr const char* PUB_K[3] = {"bare", "trim", "full"};
static constexpr const char* GLB_K[8] = {"tail",   "horns",  "ears",
                                         "socks",  "accessories",
                                         "nippless", "runes", "genital"};

// ---- shared panel geometry (virtual 1280x720; single source for draw+tap)
// Drawer on the right edge (mirrors #custPanel). 14px strips between
// sections are reserved for platform-drawn text labels (canvas is text-free).
struct R {
  F64 x = 0, y = 0, w = 0, h = 0;
};
inline bool hit(R r, F64 x, F64 y) {
  return r.w > 0 && r.h > 0 && x >= r.x && x <= r.x + r.w && y >= r.y &&
         y <= r.y + r.h;
}
inline void grid(R* out, int n, int cols, F64 x0, F64 y, F64 iw, F64 bh,
                 F64 gap) {
  if (n <= 0 || !out || cols <= 0) return;
  F64 bw = (iw - (cols - 1) * gap) / cols;
  for (int i = 0; i < n; i++) {
    int r = i / cols, c = i % cols;
    out[i] = {x0 + c * (bw + gap), y + r * (bh + gap), bw, bh};
  }
}
struct Layout {
  R panel, close;
  R pre[6];
  R pos[7], oral;
  R skin, body, breast; // hit rects; track drawn centered inside
  R hsw[6];             // 5 swatches + custom-picker placeholder (no-op)
  R hs[3];
  R fpv[4];
  R pub[3];
  R lip[6], nip[6]; // 5 swatches + custom placeholder each
  R glb[8], rune;   // GLB parts + rune-tint placeholder (no-op)
  bool showGlb = false;
};
inline Layout buildLayout(const Game& g) {
  Layout L{};
  L.panel = {868, 6, 396, 708};
  const F64 x0 = 880, iw = 372;
  L.close = {868 + 396 - 12 - 30, 14, 30, 26};
  F64 y = 64; // header (platform text) + first label strip
  grid(L.pre, 6, 3, x0, y, iw, 32, 8);
  y += 2 * 32 + 8;
  y += 14; // POSITION / ACT label
  grid(L.pos, 7, 4, x0, y, iw, 28, 6);
  { // oral occupies the 8th cell of the same grid
    F64 bw = (iw - 3 * 6) / 4;
    L.oral = {x0 + 3 * (bw + 6), y + 1 * (28 + 6), bw, 28};
  }
  y += 2 * 28 + 6;
  y += 14; // SKIN TONE label
  L.skin = {x0, y, iw, 26};
  y += 26;
  y += 14; // HAIR COLOR label
  for (int i = 0; i < 6; i++) L.hsw[i] = {x0 + i * 40, y, 30, 30};
  y += 30;
  y += 14; // HAIR STYLE label
  grid(L.hs, 3, 3, x0, y, iw, 28, 8);
  y += 28;
  y += 14; // FPV CLOSEUP label
  grid(L.fpv, 4, 4, x0, y, iw, 28, 6);
  y += 28;
  y += 14; // BODY SCALE label
  L.body = {x0, y, iw, 26};
  y += 26;
  y += 14; // BREAST SIZE label
  L.breast = {x0, y, iw, 26};
  y += 26;
  y += 14; // PUBIC HAIR label
  grid(L.pub, 3, 3, x0, y, iw, 28, 8);
  y += 28;
  y += 14; // LIP COLOR label
  for (int i = 0; i < 6; i++) L.lip[i] = {x0 + i * 40, y, 30, 30};
  y += 30;
  y += 14; // NIPPLE COLOR label
  for (int i = 0; i < 6; i++) L.nip[i] = {x0 + i * 40, y, 30, 30};
  y += 30;
  L.showGlb = presetIsGLB(g);
  if (L.showGlb) {
    y += 14; // GLB ATTIRE label
    grid(L.glb, 8, 4, x0, y, iw, 26, 6);
    y += 2 * 26 + 6;
    y += 10; // rune label strip
    L.rune = {x0, y, 30, 26};
    y += 26;
  }
  (void)y; // total ~590 (non-GLB) / ~698 (GLB) <= panel 714
  return L;
}

// ---- text-free canvas widgets ----
inline void box(Canvas& cv, R r, std::string_view fill, bool sel,
                std::string_view accent = "#ff5f86") {
  cv.setFillColorStr(fill);
  cv.fillRect(r.x, r.y, r.w, r.h);
  if (sel) {
    cv.setStrokeColorStr(accent);
    cv.setLineWidth(2);
    cv.strokeRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2);
  } else {
    cv.setStrokeColorStr("rgba(255,255,255,0.10)");
    cv.setLineWidth(1);
    cv.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  }
}
inline void swatch(Canvas& cv, R r, std::string_view col, bool sel,
                   bool placeholder = false) {
  if (placeholder) { // custom color picker: platform-owned <input type=color>
    cv.setFillColorStr("rgba(255,255,255,0.06)");
    cv.fillRect(r.x, r.y, r.w, r.h);
    cv.setStrokeColorStr("rgba(255,255,255,0.25)");
    cv.setLineWidth(1);
    cv.beginPath();
    cv.moveTo(r.x + 6, r.y + 6);
    cv.lineTo(r.x + r.w - 6, r.y + r.h - 6);
    cv.moveTo(r.x + r.w - 6, r.y + 6);
    cv.lineTo(r.x + 6, r.y + r.h - 6);
    cv.stroke();
    return;
  }
  box(cv, r, col, sel);
}
inline void slider(Canvas& cv, R r, F64 v, bool skinTrack) {
  const F64 ty = r.y + 9; // 8px track centered in 26px touch row
  if (skinTrack) {
    // js _skinBase stops: pale ivory -> warm olive -> deep brown
    cv.setFillGrad(gLinear(cv, r.x, 0, r.x + r.w, 0,
                           {{0, "#f4cba8"}, {0.5, "#d4936a"}, {1, "#7a4028"}}));
    cv.fillRect(r.x, ty, r.w, 8);
  } else {
    cv.setFillColorStr("rgba(255,255,255,0.12)");
    cv.fillRect(r.x, ty, r.w, 8);
    cv.setFillColorStr("#ff5f86");
    cv.fillRect(r.x, ty, r.w * clamp(v, 0.0, 1.0), 8);
  }
  F64 cx = r.x + clamp(v, 0.0, 1.0) * r.w;
  cv.setFillColorStr("#f6dfe6");
  cv.beginPath();
  cv.arc(cx, r.y + 13, 9, 0, TAU);
  cv.fill();
  cv.setStrokeColorStr("rgba(20,8,12,0.6)");
  cv.setLineWidth(1.5);
  cv.stroke();
}

} // namespace

// ---- js/chars.js getSkin ----
namespace chars {

SkinPair getSkin(const Game& g) {
  // js/chars.js:80-95 — 3-stop ramps; him uses compressed range t*.5+.1.
  static constexpr const char* B[3] = {"#f4cba8", "#d4936a", "#7a4028"};
  static constexpr const char* S[3] = {"#cf9273", "#a96840", "#582815"};
  static constexpr const char* D[3] = {"#c98a6d", "#9e6035", "#4e2010"};
  static constexpr const char* HB[3] = {"#dba06f", "#c07840", "#7a4828"};
  static constexpr const char* HS[3] = {"#a96c44", "#8a5030", "#5a3018"};
  F64 t = clamp(g.ch.skinTone, 0.0, 1.0);
  F64 h = t * 0.5 + 0.1;
  return {lerpHex3(B[0], B[1], B[2], t), lerpHex3(S[0], S[1], S[2], t),
          lerpHex3(D[0], D[1], D[2], t), lerpHex3(HB[0], HB[1], HB[2], h),
          lerpHex3(HS[0], HS[1], HS[2], h)};
}

void applyPreset(Game& g, const std::string& name) {
  // js/chars.js:99-105 — full replace from CHARS (fallback yuki).
  // TODO: persist via platform prefs (js lsSaveChar/CHAR_KEY) and notify
  // name HUD (js updatePlName) + GLB loader (glbPath/isGLB have no
  // CharPreset fields yet).
  const PresetRow& p = findPreset(name);
  g.ch.preset = p.preset;
  g.ch.name = p.name;
  g.ch.quote = p.quote;
  g.ch.skinTone = p.skinTone;
  g.ch.hairColor = p.hairColor;
  g.ch.hairStyle = p.hairStyle;
  g.ch.bodyScale = p.bodyScale;
  g.ch.breastSize = p.breastSize;
  g.ch.nippleColor = p.nippleColor;
  g.ch.blushColor = p.blushColor;
  g.ch.lipColor = p.lipColor;
  g.ch.eyeColor = p.eyeColor;
  g.ch.pubicHair = p.pubicHair;
}

} // namespace chars

// ---- js/charui.js customize drawer (canvas half) ----
namespace charui {

bool visible(const Game&) { return custOpen_; }
void toggle(Game&) { custOpen_ = !custOpen_; }

void draw(Canvas& cv, Game& g) {
  if (!custOpen_) return;
  Layout L = buildLayout(g);
  cv.save();
  // drawer shadow + body (DOM #custPanel styling lives in css/app.css;
  // canvas keeps neutral dark so platform text stays legible)
  cv.setFillColorStr("rgba(0,0,0,0.35)");
  cv.fillRect(L.panel.x + 6, L.panel.y + 8, L.panel.w, L.panel.h);
  cv.setFillGrad(gLinear(cv, 0, L.panel.y, 0, L.panel.y + L.panel.h,
                         {{0, "#2a1820"}, {1, "#120a0d"}}));
  cv.fillRect(L.panel.x, L.panel.y, L.panel.w, L.panel.h);
  cv.setStrokeColorStr("#4a2c3a");
  cv.setLineWidth(1);
  cv.strokeRect(L.panel.x + 0.5, L.panel.y + 0.5, L.panel.w - 1,
                L.panel.h - 1);
  cv.setFillColorStr("rgba(255,255,255,0.05)");
  cv.fillRect(L.panel.x, L.panel.y, L.panel.w, 40);
  cv.setFillColorStr("#ff5f86"); // left accent edge
  cv.fillRect(L.panel.x, L.panel.y, 3, L.panel.h);
  // close X (text-free)
  cv.setStrokeColorStr("#f6dfe6");
  cv.setLineWidth(2);
  cv.beginPath();
  cv.moveTo(L.close.x + 8, L.close.y + 6);
  cv.lineTo(L.close.x + L.close.w - 8, L.close.y + L.close.h - 6);
  cv.moveTo(L.close.x + L.close.w - 8, L.close.y + 6);
  cv.lineTo(L.close.x + 8, L.close.y + L.close.h - 6);
  cv.stroke();
  // presets
  for (int i = 0; i < 6; i++)
    box(cv, L.pre[i], "#1d1016", g.ch.preset == PRESETS[i].preset);
  // position / act (oral tinted like js style color:#ff85a0)
  for (int i = 0; i < 7; i++)
    box(cv, L.pos[i], "#1d1016", g.oral == 0 && g.pos == i);
  box(cv, L.oral, g.oral > 0 ? "#3a1c28" : "#1d1016", g.oral > 0, "#ff85a0");
  // sliders
  slider(cv, L.skin, g.ch.skinTone, true);
  slider(cv, L.body, g.ch.bodyScale, false);
  slider(cv, L.breast, g.ch.breastSize, false);
  // hair color swatches (+ custom placeholder)
  for (int i = 0; i < 5; i++)
    swatch(cv, L.hsw[i], HAIR_SW[i], g.ch.hairColor == HAIR_SW[i]);
  swatch(cv, L.hsw[5], "", false, true);
  // hair style / fpv / pubic toggles
  for (int i = 0; i < 3; i++)
    box(cv, L.hs[i], "#1d1016", g.ch.hairStyle == HAIR_ST[i]);
  for (int i = 0; i < 4; i++)
    box(cv, L.fpv[i], "#1d1016", g.fpvFocus == FPV_K[i]);
  for (int i = 0; i < 3; i++)
    box(cv, L.pub[i], "#1d1016", g.ch.pubicHair == PUB_K[i]);
  // lip / nipple swatches (+ custom placeholders)
  for (int i = 0; i < 5; i++)
    swatch(cv, L.lip[i], LIP_SW[i], g.ch.lipColor == LIP_SW[i]);
  swatch(cv, L.lip[5], "", false, true);
  for (int i = 0; i < 5; i++)
    swatch(cv, L.nip[i], NIP_SW[i], g.ch.nippleColor == NIP_SW[i]);
  swatch(cv, L.nip[5], "", false, true);
  // GLB section (only for GLB presets, mirrors syncPanelFromChar display)
  if (L.showGlb) {
    for (int i = 0; i < 8; i++)
      box(cv, L.glb[i], glbPartOn(GLB_K[i]) ? "#2c1824" : "#1d1016",
          glbPartOn(GLB_K[i]));
    swatch(cv, L.rune, glbRune_, false); // rune tint (platform color input)
  }
  cv.restore();
}

bool tap(Game& g, F64 x, F64 y) {
  if (!custOpen_) return false;
  Layout L = buildLayout(g);
  if (!hit(L.panel, x, y)) return false; // drawer never blocks the stage
  if (hit(L.close, x, y)) {
    custOpen_ = false;
    return true;
  }
  for (int i = 0; i < 6; i++)
    if (hit(L.pre[i], x, y)) {
      chars::applyPreset(g, PRESETS[i].preset);
      return true;
    }
  for (int i = 0; i < 7; i++)
    if (hit(L.pos[i], x, y)) {
      // js [data-pos]: G.oralT=0, G.pos=n, G.nod=1 (+ say/playMoan TODO:
      // needs speech + audio cue hooks, platform-owned).
      g.oral = 0;
      g.oralT = 0;
      g.pos = i;
      g.nod = 1;
      return true;
    }
  if (hit(L.oral, x, y)) {
    // js toggleOral (oral.js semantics TODO): flip her-oral state.
    g.oral = g.oral > 0 ? 0 : 1;
    g.oralT = g.oral > 0 ? 1 : 0;
    return true;
  }
  auto sliderVal = [](R r, F64 px) {
    return clamp((px - r.x) / (r.w > 0 ? r.w : 1), 0.0, 1.0);
  };
  if (hit(L.skin, x, y)) {
    g.ch.skinTone = sliderVal(L.skin, x);
    return true; // TODO: glbApplyCustom(preset) for GLB skin tint
  }
  if (hit(L.body, x, y)) {
    g.ch.bodyScale = sliderVal(L.body, x);
    return true;
  }
  if (hit(L.breast, x, y)) {
    g.ch.breastSize = sliderVal(L.breast, x);
    return true; // TODO: glbSetScale('breast', .65 + size*.9)
  }
  for (int i = 0; i < 5; i++)
    if (hit(L.hsw[i], x, y)) {
      g.ch.hairColor = HAIR_SW[i];
      return true; // TODO: glbSetPartColor(preset,'hair',col)
    }
  for (int i = 0; i < 3; i++)
    if (hit(L.hs[i], x, y)) {
      g.ch.hairStyle = HAIR_ST[i];
      return true;
    }
  for (int i = 0; i < 4; i++)
    if (hit(L.fpv[i], x, y)) {
      // js [data-fpv]: sets focus, enters fpv + btnZoom state (TODO: HUD).
      g.fpvFocus = FPV_K[i];
      if (g.view != "fpv") g.view = "fpv";
      return true;
    }
  for (int i = 0; i < 3; i++)
    if (hit(L.pub[i], x, y)) {
      g.ch.pubicHair = PUB_K[i];
      return true;
    }
  for (int i = 0; i < 5; i++)
    if (hit(L.lip[i], x, y)) {
      g.ch.lipColor = LIP_SW[i];
      return true;
    }
  for (int i = 0; i < 5; i++)
    if (hit(L.nip[i], x, y)) {
      g.ch.nippleColor = NIP_SW[i];
      return true;
    }
  if (L.showGlb) {
    for (int i = 0; i < 8; i++)
      if (hit(L.glb[i], x, y)) {
        // TODO: glbSetPartVisible(preset, part, vis).
        glbParts_[GLB_K[i]] = !glbPartOn(GLB_K[i]);
        return true;
      }
    if (hit(L.rune, x, y)) return true; // TODO: platform color input
  }
  return true; // inside panel: consume (custom slots/name field TODO text)
}

} // namespace charui
} // namespace ag
