#include "ui/ui.h"
#include "gfx/shader.h"
#include "core/log.h"
#include "rlgl.h"
#include "embedded_fonts.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace ag::ui {

namespace {
const char* kFaceFile[(int)Face::Count] = {"CormorantGaramond-Medium", "CormorantGaramond-SemiBold", "CormorantGaramond-MediumItalic",
                                           "Inter-Regular", "Inter-Medium", "Inter-SemiBold"};
const EmbeddedFont* findFont(const char* name) {
    for (int i = 0; i < kEmbeddedFontCount; ++i)
        if (std::strcmp(kEmbeddedFonts[i].name, name) == 0) return &kEmbeddedFonts[i];
    return nullptr;
}
std::vector<int> glyphSet() {
    std::vector<int> cp;
    for (int c = 32; c < 127; ++c) cp.push_back(c);
    for (int c = 160; c < 256; ++c) cp.push_back(c);
    for (int c : {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2191, 0x2193, 0x2212}) cp.push_back(c);
    return cp;
}
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
Vector4 col4(Color c) { return Vector4{c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; }
}  // namespace

bool Ui::init() {
    std::string fs = shaderSource("ui_rect.frag");
    if (!fs.empty()) shader_ = LoadShaderFromMemory(nullptr, fs.c_str());
    inited_ = IsShaderValid(shader_);
    if (!inited_) { AG_WARN("AG: ui: rect shader unavailable, flat fallback"); return true; }
    locRect_ = GetShaderLocation(shader_, "u_rect");
    locRadius_ = GetShaderLocation(shader_, "u_radius");
    locScreenH_ = GetShaderLocation(shader_, "u_screenH");
    locFill_ = GetShaderLocation(shader_, "u_fill");
    locFill2_ = GetShaderLocation(shader_, "u_fill2");
    locGrad_ = GetShaderLocation(shader_, "u_grad");
    locGlass_ = GetShaderLocation(shader_, "u_glass");
    locGlassTint_ = GetShaderLocation(shader_, "u_glassTint");
    locBorder_ = GetShaderLocation(shader_, "u_border");
    locBorderCol_ = GetShaderLocation(shader_, "u_borderCol");
    locShadow_ = GetShaderLocation(shader_, "u_shadow");
    locShadowCol_ = GetShaderLocation(shader_, "u_shadowCol");
    locTime_ = GetShaderLocation(shader_, "u_time");
    return true;
}

void Ui::shutdown() {
    unsigned int def = GetFontDefault().texture.id;
    for (auto& kv : fonts_) if (kv.second.texture.id && kv.second.texture.id != def) UnloadFont(kv.second);
    fonts_.clear();
    if (inited_) UnloadShader(shader_);
    inited_ = false;
}

void Ui::begin(float dt, bool readInput) {
    dt_ = dt > 0.f ? std::min(dt, 0.1f) : 1.f / 60.f;
    time += dt_;
    W = (float)GetScreenWidth(); H = (float)GetScreenHeight();
    fbW = std::max(1, GetRenderWidth()); fbH = std::max(1, GetRenderHeight());
    dpi = W > 0.f ? (float)fbW / W : 1.f;
    // sub-linear: small windows keep readable type, large ones do not balloon
    s = std::max(0.5f, std::pow(std::min(W / 1920.f, H / 1080.f), 0.8f));
    focusCount_ = 0;
    in = Input{};
    if (!readInput) return;
    in.mouse = GetMousePosition();
    Vector2 md = GetMouseDelta();
    in.mouseMoved = std::fabs(md.x) + std::fabs(md.y) > 0.5f;
    in.mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    in.rightClicked = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.wheel = GetMouseWheelMove();
    auto pr = [](int k) { return IsKeyPressed(k) || IsKeyPressedRepeat(k); };
    in.up = pr(KEY_UP) || pr(KEY_W);
    in.down = pr(KEY_DOWN) || pr(KEY_S);
    in.left = pr(KEY_LEFT) || pr(KEY_A);
    in.right = pr(KEY_RIGHT) || pr(KEY_D);
    in.accept = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE);
    in.back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    in.tabNext = (IsKeyPressed(KEY_TAB) && !shift) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_PAGE_DOWN);
    in.tabPrev = (IsKeyPressed(KEY_TAB) && shift) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_PAGE_UP);
    if (in.up || in.down) {
        keyboardNav = true;
        if (lastFocusCount_ > 0) focus_ = (focus_ + (in.down ? 1 : -1) + lastFocusCount_) % lastFocusCount_;
    }
    if (in.mouseMoved || in.clicked) keyboardNav = false;
}

void Ui::end() {
    lastFocusCount_ = focusCount_;
    if (lastFocusCount_ > 0) focus_ = std::max(0, std::min(focus_, lastFocusCount_ - 1));
    if (!in.mouseDown && !active_.empty()) active_.clear();
}

float Ui::anim(const std::string& id, float target, float speed) {
    WState& w = st(id);
    if (!w.init) { w.value = target; w.init = true; }
    w.value += (target - w.value) * std::min(1.f, dt_ * speed);
    return w.value;
}

Color Ui::fade(Color c, float a) const {
    c.a = (unsigned char)std::lround(c.a * clamp01(a * alpha));
    return c;
}

// ---------------------------------------------------------------- text
Font& Ui::font(Face f, float px) {
    float phys = px * dpi;
    int sz = phys < 40.f ? (int)std::lround(phys / 2.f) * 2 : (int)std::lround(phys / 4.f) * 4;
    sz = std::max(8, std::min(sz, 240));
    uint64_t key = ((uint64_t)(int)f << 32) | (uint64_t)sz;
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    Font fnt = GetFontDefault();
    if (const EmbeddedFont* ef = findFont(kFaceFile[(int)f])) {
        static std::vector<int> cps = glyphSet();
        Font l = LoadFontFromMemory(".ttf", ef->data, ef->size, sz, cps.data(), (int)cps.size());
        if (l.texture.id) {
            SetTextureFilter(l.texture, TEXTURE_FILTER_BILINEAR);
            // the atlas is all we draw from: drop the per-glyph CPU images
            for (int i = 0; i < l.glyphCount; ++i) { UnloadImage(l.glyphs[i].image); l.glyphs[i].image.data = nullptr; }
            fnt = l;
        }
    }
    return fonts_[key] = fnt;
}

Vector2 Ui::measure(Face f, float px, const std::string& t, float spacing) {
    if (t.empty()) return Vector2{0.f, px};
    return MeasureTextEx(font(f, px), t.c_str(), px, spacing * px);
}

void Ui::text(Face f, float px, const std::string& t, float x, float y, Color c, int align, float spacing) {
    if (t.empty() || c.a == 0 || alpha <= 0.002f) return;
    Font& fnt = font(f, px);
    if (align != Left) {
        Vector2 m = MeasureTextEx(fnt, t.c_str(), px, spacing * px);
        x -= align == Center ? m.x * 0.5f : m.x;
    }
    DrawTextEx(fnt, t.c_str(), Vector2{std::round(x * dpi) / dpi, std::round(y * dpi) / dpi}, px, spacing * px, fade(c));
}

float Ui::paragraph(Face f, float px, const std::string& t, float x, float y, float width, Color c, float lineGap, int align) {
    std::istringstream words(t);
    std::string w, line;
    float yy = y;
    auto flush = [&]() {
        if (line.empty()) return;
        float lx = align == Center ? x + width * 0.5f : (align == Right ? x + width : x);
        text(f, px, line, lx, yy, c, align);
        yy += px * lineGap;
        line.clear();
    };
    while (words >> w) {
        std::string cand = line.empty() ? w : line + " " + w;
        if (!line.empty() && measure(f, px, cand).x > width) { flush(); line = w; }
        else line = cand;
    }
    flush();
    return yy - y;
}

// ---------------------------------------------------------------- shapes
void Ui::box(Rectangle r, const Box& b) {
    if (alpha <= 0.002f) return;
    if (!inited_) {
        Color c = b.glass ? withA(b.glassTint, 0.86f) : b.fill;
        if (b.radius > 0.f) DrawRectangleRounded(r, std::min(1.f, 2.f * b.radius / std::max(1.f, std::min(r.width, r.height))), 8, fade(c));
        else DrawRectangleRec(r, fade(c));
        return;
    }
    float m = (b.shadow > 0.f ? b.shadow * 1.3f : 0.f) + 2.f;
    Rectangle q{r.x - m, r.y - m, r.width + 2.f * m, r.height + 2.f * m};
    rlDrawRenderBatchActive();
    BeginShaderMode(shader_);
    float rect[4] = {r.x * dpi, r.y * dpi, r.width * dpi, r.height * dpi};
    float rad = b.radius * dpi, scrH = (float)fbH, border = b.border * dpi, shadow = b.shadow * dpi, t = time;
    bool glass = b.glass && blurTex != 0;
    Color fillC = b.fill;
    if (b.glass && !glass) fillC = mixc(withA(b.glassTint, 0.9f), b.fill, b.fill.a / 255.f);   // no blur available: solid tint
    Vector4 fill = col4(fillC), fill2 = col4(b.fill2), bc = col4(b.borderCol), sc = col4(b.shadowCol), gt = col4(b.glassTint);
    gt.w = b.glassAmt;
    int grad = b.grad, gl = glass ? 1 : 0;
    SetShaderValue(shader_, locRect_, rect, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locRadius_, &rad, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locScreenH_, &scrH, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locFill_, &fill, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locFill2_, &fill2, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locGrad_, &grad, SHADER_UNIFORM_INT);
    SetShaderValue(shader_, locGlass_, &gl, SHADER_UNIFORM_INT);
    SetShaderValue(shader_, locGlassTint_, &gt, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locBorder_, &border, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locBorderCol_, &bc, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locShadow_, &shadow, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locShadowCol_, &sc, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader_, locTime_, &t, SHADER_UNIFORM_FLOAT);
    Texture2D tex{rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Rectangle src{0.f, 0.f, 1.f, 1.f};
    if (glass) {
        tex = Texture2D{blurTex, blurW, blurH, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        float sx = (float)blurW / (float)fbW, sy = (float)blurH / (float)fbH;
        // FBO textures are bottom-up: a negative source height flips them (see DrawTexturePro)
        src = Rectangle{q.x * dpi * sx, (float)blurH - (q.y + q.height) * dpi * sy, q.width * dpi * sx, -q.height * dpi * sy};
    }
    DrawTexturePro(tex, src, q, Vector2{0.f, 0.f}, 0.f, Color{255, 255, 255, (unsigned char)std::lround(255.f * clamp01(alpha))});
    EndShaderMode();
}

void Ui::panel(Rectangle r, float radius, float tint) {
    Box b;
    b.radius = u(radius);
    b.glass = true; b.glassTint = th.glass; b.glassAmt = tint;
    b.fill = Color{255, 240, 230, 6};
    b.border = std::max(1.f, u(1.2f)); b.borderCol = th.hairline;
    b.shadow = u(40.f); b.shadowCol = Color{0, 0, 0, 90};
    box(r, b);
}

void Ui::backdrop(float amount) {
    if (amount <= 0.002f) return;
    float a0 = alpha;
    alpha = a0 * clamp01(amount);
    Box b;
    b.glass = true; b.glassTint = th.glass; b.glassAmt = 0.42f;
    box(Rectangle{0.f, 0.f, W, H}, b);
    alpha = a0;
}

void Ui::fillRect(Rectangle r, Color c, float radius) {
    Box b; b.radius = radius; b.fill = c;
    box(r, b);
}

void Ui::glow(float x, float y, float radius, Color c) {
    Box b; b.radius = 1.f; b.shadow = radius; b.shadowCol = c;
    box(Rectangle{x - 1.f, y - 1.f, 2.f, 2.f}, b);
}

void Ui::line(float x0, float y0, float x1, float y1, float w, Color c) {
    DrawLineEx(Vector2{x0, y0}, Vector2{x1, y1}, w, fade(c));
}

void Ui::heart(float x, float y, float size, Color c) {
    Color k = fade(c);
    float r = size * 0.27f;
    DrawCircleV(Vector2{x - size * 0.24f, y - size * 0.12f}, r, k);
    DrawCircleV(Vector2{x + size * 0.24f, y - size * 0.12f}, r, k);
    DrawTriangle(Vector2{x - size * 0.49f, y - size * 0.04f}, Vector2{x, y + size * 0.5f}, Vector2{x + size * 0.49f, y - size * 0.04f}, k);
}

void Ui::diamond(float x, float y, float size, Color c) { DrawPoly(Vector2{x, y}, 4, size, 0.f, fade(c)); }

// ---------------------------------------------------------------- widgets
bool Ui::interact(const std::string& id, Rectangle r, int fidx, bool enabled, WState& w) {
    bool over = enabled && hit(r) && (active_.empty() || active_ == id);
    if (over && in.mouseMoved && focus_ != fidx) focus_ = fidx;
    bool hot = enabled && (over || (keyboardNav && focused(fidx)));
    if (hot && !w.wasHot) sound(0);
    w.wasHot = hot;
    float k = std::min(1.f, dt_ * 14.f);
    w.hover += ((hot ? 1.f : 0.f) - w.hover) * k;
    w.focusT += ((keyboardNav && focused(fidx) ? 1.f : 0.f) - w.focusT) * k;
    bool fire = false;
    if (over && in.clicked) active_ = id;
    bool held = active_ == id && in.mouseDown && over;
    if (active_ == id && in.released) { fire = over; active_.clear(); }
    if (enabled && keyboardNav && focused(fidx) && in.accept) { fire = true; w.press = 1.f; }
    w.press += ((held ? 1.f : 0.f) - w.press) * std::min(1.f, dt_ * 18.f);
    if (fire) sound(1);
    return fire;
}

bool Ui::menuItem(const std::string& id, const std::string& label, float x, float y, float w, bool enabled) {
    int fidx = takeFocus();
    float hgt = u(66.f);
    Rectangle r{x, y, w, hgt};
    WState& ws = st(id);
    bool fire = interact(id, r, fidx, enabled, ws);
    float h = ws.hover;
    float px = u(46.f);
    Color c = enabled ? mixc(th.inkDim, th.ink, h) : th.inkFaint;
    if (h > 0.01f) {
        glow(x + u(40.f) + measure(Face::Serif, px, label).x * 0.5f, y + hgt * 0.5f, u(70.f) * h, withA(th.accent, 0.10f * h));
        diamond(x + u(10.f), y + hgt * 0.53f, u(6.f) * h, withA(th.accent, h));
    }
    text(Face::Serif, px, label, x + u(30.f) + u(10.f) * h - ws.press * u(3.f), y + (hgt - px) * 0.5f - u(4.f), c);
    return fire;
}

bool Ui::button(const std::string& id, const std::string& label, Rectangle r, bool primary, bool enabled) {
    int fidx = takeFocus();
    WState& ws = st(id);
    bool fire = interact(id, r, fidx, enabled, ws);
    float h = ws.hover, p = ws.press;
    float shrink = p * u(2.f);
    Rectangle q{r.x + shrink, r.y + shrink, r.width - 2.f * shrink, r.height - 2.f * shrink};
    Box b;
    b.radius = q.height * 0.5f;
    if (primary) {
        b.fill = mixc(Color{236, 164, 120, 255}, Color{248, 190, 146, 255}, h);
        b.fill2 = mixc(Color{214, 112, 132, 255}, Color{232, 132, 152, 255}, h);
        b.grad = 1;
        b.shadow = u(18.f + 14.f * h); b.shadowCol = withA(th.rose, 0.28f + 0.2f * h);
    } else {
        b.fill = Color{255, 236, 224, (unsigned char)(10 + 22 * h)};
        b.border = std::max(1.f, u(1.2f)); b.borderCol = mixc(th.hairline, withA(th.accent, 0.75f), std::max(h, ws.focusT));
        b.shadow = u(16.f) * h; b.shadowCol = withA(th.accent, 0.18f * h);
    }
    if (!enabled) { b.fill = withA(b.fill, 0.25f); b.fill2 = withA(b.fill2, 0.25f); }
    box(q, b);
    float px = u(19.f);
    Color tc = primary ? Color{40, 18, 24, 255} : (enabled ? mixc(th.inkDim, th.ink, h) : th.inkFaint);
    text(Face::SansBold, px, label, q.x + q.width * 0.5f, q.y + (q.height - px) * 0.5f - u(1.f), tc, Center, 0.12f);
    return fire;
}

bool Ui::slider(const std::string& id, const std::string& label, float* v, float lo, float hi, Rectangle r, bool bipolar,
                const std::string& valueText) {
    int fidx = takeFocus();
    WState& ws = st(id);
    float before = *v;
    float trackX = r.x, trackW = r.width, trackY = r.y + r.height * 0.72f;
    Rectangle hitR{r.x - u(8.f), r.y, r.width + u(16.f), r.height};
    bool over = hit(hitR) && (active_.empty() || active_ == id);
    if (over && in.mouseMoved && focus_ != fidx) focus_ = fidx;
    bool hot = over || active_ == id || (keyboardNav && focused(fidx));
    if (hot && !ws.wasHot) sound(0);
    ws.wasHot = hot;
    float k = std::min(1.f, dt_ * 14.f);
    ws.hover += ((hot ? 1.f : 0.f) - ws.hover) * k;
    if (over && in.clicked) active_ = id;
    if (active_ == id && in.mouseDown) {
        float t = clamp01((in.mouse.x - trackX) / std::max(1.f, trackW));
        *v = lo + t * (hi - lo);
    }
    if (keyboardNav && focused(fidx)) {
        float step = (hi - lo) / 20.f;
        if (in.left) *v = std::max(lo, *v - step);
        if (in.right) *v = std::min(hi, *v + step);
    }
    // detents every 5%
    int b0 = (int)std::floor((before - lo) / (hi - lo) * 20.f), b1 = (int)std::floor((*v - lo) / (hi - lo) * 20.f);
    if (b0 != b1) sound(3);
    float h = ws.hover;
    float px = u(19.f);
    text(Face::SansMedium, px, label, r.x, r.y, mixc(th.inkDim, th.ink, h));
    char buf[32];
    std::string vt = valueText;
    if (vt.empty()) {
        if (bipolar) std::snprintf(buf, sizeof buf, "%+d", (int)std::lround(*v * 100.f));
        else std::snprintf(buf, sizeof buf, "%d%%", (int)std::lround((*v - lo) / (hi - lo) * 100.f));
        vt = buf;
    }
    text(Face::Sans, px, vt, r.x + r.width, r.y, mixc(th.inkFaint, th.ink, h), Right);
    float t = clamp01((*v - lo) / (hi - lo));
    float th2 = u(4.f);
    fillRect(Rectangle{trackX, trackY - th2 * 0.5f, trackW, th2}, Color{255, 236, 224, 40}, th2 * 0.5f);
    float a = bipolar ? 0.5f : 0.f;
    float x0 = trackX + trackW * std::min(a, t), x1 = trackX + trackW * std::max(a, t);
    Box fb; fb.radius = th2 * 0.5f; fb.fill = withA(th.accent, 0.95f); fb.fill2 = withA(th.rose, 0.95f); fb.grad = 1;
    box(Rectangle{x0, trackY - th2 * 0.5f, std::max(x1 - x0, th2), th2}, fb);
    if (bipolar) fillRect(Rectangle{trackX + trackW * 0.5f - u(1.f), trackY - u(7.f), u(2.f), u(14.f)}, Color{255, 236, 224, 70}, u(1.f));
    float kx = trackX + trackW * t, kr = u(8.f) + u(2.5f) * h;
    if (h > 0.01f) glow(kx, trackY, u(22.f) * h, withA(th.accent, 0.35f * h));
    Box kb; kb.radius = kr; kb.fill = Color{255, 246, 238, 255}; kb.shadow = u(8.f); kb.shadowCol = Color{0, 0, 0, 120};
    box(Rectangle{kx - kr, trackY - kr, 2.f * kr, 2.f * kr}, kb);
    return *v != before;
}

bool Ui::toggle(const std::string& id, const std::string& label, bool* v, Rectangle r) {
    int fidx = takeFocus();
    WState& ws = st(id);
    bool fire = interact(id, r, fidx, true, ws);
    if (fire) *v = !*v;
    float h = ws.hover;
    float px = u(19.f);
    text(Face::SansMedium, px, label, r.x, r.y + (r.height - px) * 0.5f, mixc(th.inkDim, th.ink, h));
    float sw = u(50.f), sh = u(26.f);
    Rectangle tr{r.x + r.width - sw, r.y + (r.height - sh) * 0.5f, sw, sh};
    float on = anim(id + "#on", *v ? 1.f : 0.f, 16.f);
    Box b; b.radius = sh * 0.5f;
    b.fill = mixc(Color{255, 236, 224, 34}, withA(th.accent, 0.92f), on);
    b.fill2 = mixc(Color{255, 236, 224, 34}, withA(th.rose, 0.92f), on); b.grad = 1;
    b.border = std::max(1.f, u(1.f)); b.borderCol = mixc(th.hairline, withA(th.accent, 0.8f), std::max(h * 0.6f, ws.focusT));
    box(tr, b);
    float kr = sh * 0.5f - u(3.f);
    float kx = tr.x + sh * 0.5f + (sw - sh) * on;
    Box kb; kb.radius = kr; kb.fill = Color{255, 248, 242, 255}; kb.shadow = u(6.f); kb.shadowCol = Color{0, 0, 0, 110};
    box(Rectangle{kx - kr, tr.y + sh * 0.5f - kr, 2.f * kr, 2.f * kr}, kb);
    return fire;
}

bool Ui::choice(const std::string& id, const std::string& label, int* idx, const std::vector<std::string>& opts, Rectangle r) {
    int fidx = takeFocus();
    WState& ws = st(id);
    int n = (int)opts.size();
    if (n == 0) return false;
    int before = *idx;
    float px = u(18.f);
    bool hasLabel = !label.empty();
    float segH = u(36.f);
    float segW = 0.f;
    for (auto& o : opts) segW = std::max(segW, measure(Face::SansMedium, px, o).x + u(30.f));
    float totalW = segW * (float)n;
    Rectangle cr{r.x + r.width - totalW, r.y + (r.height - segH) * 0.5f, totalW, segH};
    if (!hasLabel) cr.x = r.x;
    bool over = hit(cr) && (active_.empty() || active_ == id);
    if (over && in.mouseMoved && focus_ != fidx) focus_ = fidx;
    bool hot = over || (keyboardNav && focused(fidx));
    if (hot && !ws.wasHot) sound(0);
    ws.wasHot = hot;
    ws.hover += ((hot ? 1.f : 0.f) - ws.hover) * std::min(1.f, dt_ * 14.f);
    if (over && in.clicked) {
        int i = (int)((in.mouse.x - cr.x) / segW);
        if (i >= 0 && i < n) *idx = i;
    }
    if (keyboardNav && focused(fidx)) {
        if (in.left) *idx = (*idx + n - 1) % n;
        if (in.right || in.accept) *idx = (*idx + 1) % n;
    }
    if (*idx != before) sound(1);
    float h = ws.hover;
    if (hasLabel) text(Face::SansMedium, u(19.f), label, r.x, r.y + (r.height - u(19.f)) * 0.5f, mixc(th.inkDim, th.ink, h));
    Box bg; bg.radius = segH * 0.5f; bg.fill = Color{255, 236, 224, 16};
    bg.border = std::max(1.f, u(1.f)); bg.borderCol = mixc(th.hairline, withA(th.accent, 0.7f), std::max(h * 0.5f, keyboardNav && focused(fidx) ? 1.f : 0.f));
    box(cr, bg);
    float pos = anim(id + "#pos", (float)*idx, 16.f);
    Box pill; pill.radius = segH * 0.5f - u(3.f);
    pill.fill = withA(th.accent, 0.9f); pill.fill2 = withA(th.rose, 0.9f); pill.grad = 1;
    box(Rectangle{cr.x + u(3.f) + pos * segW, cr.y + u(3.f), segW - u(6.f), segH - u(6.f)}, pill);
    for (int i = 0; i < n; ++i) {
        float sel = 1.f - std::min(1.f, std::fabs(pos - (float)i));
        Color tc = mixc(th.inkDim, Color{40, 18, 24, 255}, sel);
        text(Face::SansMedium, px, opts[(size_t)i], cr.x + segW * ((float)i + 0.5f), cr.y + (segH - px) * 0.5f - u(1.f), tc, Center);
    }
    return *idx != before;
}

bool Ui::swatches(const std::string& id, const std::string& label, int* idx, const std::vector<Color>& colors,
                  const std::vector<std::string>& names, Rectangle r) {
    int fidx = takeFocus();
    WState& ws = st(id);
    int n = (int)colors.size();
    int before = *idx;
    float px = u(19.f);
    float rad = u(17.f), gap = u(12.f);
    float rowW = (float)n * (2.f * rad) + (float)(n - 1) * gap;
    float cy = r.y + r.height - rad - u(2.f);
    float x0 = r.x;
    Rectangle hr{x0 - u(4.f), cy - rad - u(4.f), rowW + u(8.f), 2.f * rad + u(8.f)};
    bool over = hit(hr) && (active_.empty() || active_ == id);
    if (over && in.mouseMoved && focus_ != fidx) focus_ = fidx;
    bool hot = over || (keyboardNav && focused(fidx));
    if (hot && !ws.wasHot) sound(0);
    ws.wasHot = hot;
    ws.hover += ((hot ? 1.f : 0.f) - ws.hover) * std::min(1.f, dt_ * 14.f);
    int hoverI = -1;
    for (int i = 0; i < n; ++i) {
        float cx = x0 + rad + (float)i * (2.f * rad + gap);
        float dx = in.mouse.x - cx, dy = in.mouse.y - cy;
        if (over && dx * dx + dy * dy <= rad * rad * 1.3f) hoverI = i;
    }
    if (hoverI >= 0 && in.clicked) *idx = hoverI;
    if (keyboardNav && focused(fidx) && n > 0) {
        if (in.left) *idx = (*idx + n - 1) % n;
        if (in.right || in.accept) *idx = (*idx + 1) % n;
    }
    if (*idx != before) sound(1);
    text(Face::SansMedium, px, label, r.x, r.y, mixc(th.inkDim, th.ink, ws.hover));
    int shown = hoverI >= 0 ? hoverI : *idx;
    if (shown >= 0 && shown < (int)names.size()) text(Face::Sans, px, names[(size_t)shown], r.x + r.width, r.y, th.inkFaint, Right);
    for (int i = 0; i < n; ++i) {
        float cx = x0 + rad + (float)i * (2.f * rad + gap);
        float sel = anim(id + "#s" + std::to_string(i), i == *idx ? 1.f : 0.f, 14.f);
        float hv = anim(id + "#h" + std::to_string(i), i == hoverI ? 1.f : 0.f, 16.f);
        float rr = rad * (0.86f + 0.1f * hv + 0.06f * sel);
        if (sel > 0.01f) {
            Box ring; ring.radius = rr + u(5.f); ring.border = std::max(1.5f, u(2.f)); ring.borderCol = withA(th.accent, sel);
            box(Rectangle{cx - rr - u(5.f), cy - rr - u(5.f), 2.f * (rr + u(5.f)), 2.f * (rr + u(5.f))}, ring);
        }
        Box b; b.radius = rr; b.fill = colors[(size_t)i]; b.shadow = u(6.f); b.shadowCol = Color{0, 0, 0, 100};
        b.border = std::max(1.f, u(1.f)); b.borderCol = Color{255, 255, 255, 50};
        box(Rectangle{cx - rr, cy - rr, 2.f * rr, 2.f * rr}, b);
    }
    return *idx != before;
}

bool Ui::tabs(const std::string& id, int* idx, const std::vector<std::string>& labels, Rectangle r) {
    int n = (int)labels.size();
    if (n == 0) return false;
    int before = *idx;
    float px = u(17.f);
    float cw = r.width / (float)n;
    for (int i = 0; i < n; ++i) {
        Rectangle c{r.x + cw * (float)i, r.y, cw, r.height};
        WState& ws = st(id + "#t" + std::to_string(i));
        bool over = hit(c) && active_.empty();
        if (over && !ws.wasHot) sound(0);
        ws.wasHot = over;
        ws.hover += ((over ? 1.f : 0.f) - ws.hover) * std::min(1.f, dt_ * 14.f);
        if (over && in.clicked) *idx = i;
        float sel = i == *idx ? 1.f : 0.f;
        text(Face::SansBold, px, labels[(size_t)i], c.x + cw * 0.5f, c.y + (c.height - px) * 0.5f - u(3.f),
             mixc(mixc(th.inkFaint, th.inkDim, ws.hover), th.ink, sel), Center, 0.16f);
    }
    if (in.tabNext) *idx = (*idx + 1) % n;
    if (in.tabPrev) *idx = (*idx + n - 1) % n;
    if (*idx != before) sound(1);
    float pos = anim(id + "#u", (float)*idx, 14.f);
    float uw = cw * 0.42f;
    fillRect(Rectangle{r.x, r.y + r.height - u(1.f), r.width, std::max(1.f, u(1.f))}, th.hairline);
    Box ub; ub.radius = u(1.5f); ub.fill = th.accent; ub.fill2 = th.rose; ub.grad = 1;
    box(Rectangle{r.x + cw * (pos + 0.5f) - uw * 0.5f, r.y + r.height - u(2.5f), uw, u(3.f)}, ub);
    return *idx != before;
}

bool Ui::chip(const std::string& id, const std::string& label, Rectangle r, bool active) {
    int fidx = takeFocus();
    WState& ws = st(id);
    bool fire = interact(id, r, fidx, true, ws);
    float h = ws.hover;
    float on = anim(id + "#on", active ? 1.f : 0.f, 14.f);
    Box b; b.radius = r.height * 0.5f;
    b.fill = mixc(Color{255, 236, 224, (unsigned char)(12 + 18 * h)}, withA(th.accent, 0.22f), on);
    b.border = std::max(1.f, u(1.f)); b.borderCol = mixc(th.hairline, withA(th.accent, 0.85f), std::max(on, std::max(h * 0.5f, ws.focusT)));
    box(r, b);
    float px = u(16.f);
    text(Face::SansMedium, px, label, r.x + r.width * 0.5f, r.y + (r.height - px) * 0.5f - u(1.f), mixc(mixc(th.inkDim, th.ink, h), th.ink, on), Center, 0.04f);
    return fire;
}

float Ui::keyHint(float x, float y, const std::string& key, const std::string& label, float px) {
    float kp = u(px * 0.82f);
    float lp = u(px);
    float kw = std::max(measure(Face::SansBold, kp, key).x + u(14.f), u(26.f));
    float kh = u(px * 1.55f);
    Box b; b.radius = u(6.f); b.fill = Color{20, 12, 16, 150}; b.border = std::max(1.f, u(1.f)); b.borderCol = Color{255, 232, 214, 110};
    b.shadow = u(10.f); b.shadowCol = Color{0, 0, 0, 90};
    box(Rectangle{x, y, kw, kh}, b);
    text(Face::SansBold, kp, key, x + kw * 0.5f, y + (kh - kp) * 0.5f - u(1.f), th.ink, Center);
    text(Face::SansMedium, lp, label, x + kw + u(9.f), y + (kh - lp) * 0.5f - u(1.f), th.ink);
    return kw + u(9.f) + measure(Face::Sans, lp, label).x;
}

void Ui::sectionLabel(const std::string& t, float x, float y, float w) {
    float px = u(14.f);
    text(Face::SansBold, px, t, x, y, th.accent, Left, 0.22f);
    float tw = measure(Face::SansBold, px, t, 0.22f).x;
    fillRect(Rectangle{x + tw + u(14.f), y + px * 0.55f, std::max(0.f, w - tw - u(14.f)), std::max(1.f, u(1.f))}, th.hairline);
}

}  // namespace ag::ui
