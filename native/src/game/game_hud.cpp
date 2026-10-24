// Afterglow game — the in-game HUD: arousal meters, connection, rhythm gauge, her voice and hints, the action bar
// and the acts picker. Everything is measured and fits the window (ui/layout.h); see docs/HUD_NOTES.md.
#include "game/game.h"
#include "scene/lighting.h"
#include "ui/layout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float approach(float v, float t, float rate, float dt) { return v + (t - v) * (1.f - std::exp(-rate * dt)); }

const char* desireHint(Desire d) {
    switch (d) {
        case Desire::Faster: return "Quicker strokes  \xc2\xb7  W / scroll faster";
        case Desire::Slower: return "Slow the rhythm  \xc2\xb7  S / gentler strokes";
        case Desire::Deeper: return "Longer, deeper strokes  \xc2\xb7  D";
        case Desire::Gentler: return "Shorter, softer strokes  \xc2\xb7  A";
        case Desire::Touch: return "Let her touch herself  \xc2\xb7  R";
        case Desire::Kiss: return "Hold her close  \xc2\xb7  E";
        case Desire::Change: return "Choose another position  \xc2\xb7  Tab";
        default: return "";
    }
}
}  // namespace

// ------------------------------------------------------------------ HUD
void Game::drawHud() {
    if (posIndex_ < 0) return;
    ui::Ui& U = ui;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    float hudA = hudHidden_ ? 0.f : clamp01(hudT_ * 1.5f);
    if (night_.finished) hudA *= clamp01(1.f - afterT_ * 0.6f);
    U.alpha = hudA;
    if (hudA > 0.01f) {
        // ---- arousal: two slim meters, top left ----
        float x = U.u(56.f), y = U.u(46.f), w = U.u(330.f);
        auto meter = [&](const char* who, float v, Color c, float yy, bool pulse) {
            U.text(ui::Face::SansBold, U.u(14.f), who, x, yy, ui::withA(U.th.ink, 0.85f), ui::Left, 0.22f);
            char b[16]; std::snprintf(b, sizeof b, "%d", (int)std::lround(v * 100.f));
            U.text(ui::Face::Sans, U.u(14.f), b, x + w, yy, U.th.inkFaint, ui::Right);
            float th = U.u(5.f), by = yy + U.u(26.f);
            U.fillRect(Rectangle{x, by, w, th}, Color{255, 236, 224, 34}, th * 0.5f);
            float fw = std::max(th, w * v);
            ui::Box b2; b2.radius = th * 0.5f; b2.fill = ui::withA(c, 0.75f); b2.fill2 = c; b2.grad = 1;
            b2.shadow = U.u(10.f + (pulse ? 8.f * (0.5f + 0.5f * std::sin(time_ * 9.f)) : 0.f)); b2.shadowCol = ui::withA(c, 0.45f);
            U.box(Rectangle{x, by, fw, th}, b2);
            U.glow(x + fw, by + th * 0.5f, U.u(14.f), ui::withA(c, 0.6f));
        };
        std::string herN = castName(0), hisN = castName(1);
        for (auto& ch : herN) ch = (char)std::toupper((unsigned char)ch);
        for (auto& ch : hisN) ch = (char)std::toupper((unsigned char)ch);
        meter(herN.c_str(), night_.her, U.th.rose, y, night_.her > 0.85f);
        if (!p.solo) {
            meter(hisN.c_str(), night_.him, U.th.gold, y + U.u(52.f), night_.him > 0.82f);
            if (night_.him > 0.82f && !night_.finished) {
                float a = 0.55f + 0.45f * std::sin(time_ * 5.f);
                U.text(ui::Face::SerifItalic, U.u(22.f), night_.edging ? "Holding back\xe2\x80\xa6" : "He's close \xe2\x80\x94 ease off",
                       x, y + U.u(112.f), ui::withA(U.th.gold, a));
            }
        }
        // ---- connection + score, top right ----
        float rx = U.W - U.u(56.f), ry = U.u(44.f);
        float pulse = 1.f + 0.06f * std::sin(time_ * (2.f + 3.f * night_.her));
        U.glow(rx - U.u(16.f), ry + U.u(16.f), U.u(26.f) * night_.connection, ui::withA(U.th.rose, 0.35f));
        U.heart(rx - U.u(16.f), ry + U.u(16.f), U.u(26.f) * pulse, ui::mixc(Color{120, 80, 90, 255}, U.th.rose, night_.connection));
        char cb[24]; std::snprintf(cb, sizeof cb, "%d%%", (int)std::lround(night_.connection * 100.f));
        U.text(ui::Face::SerifBold, U.u(30.f), cb, rx - U.u(40.f), ry, U.th.ink, ui::Right);
        U.text(ui::Face::SansBold, U.u(12.f), "CONNECTION", rx - U.u(40.f), ry + U.u(36.f), U.th.inkFaint, ui::Right, 0.22f);
        char sb[24]; std::snprintf(sb, sizeof sb, "%d", (int)std::lround(night_.score));
        if (screen_ != Screen::Story) U.text(ui::Face::Sans, U.u(17.f), sb, rx, ry + U.u(64.f), U.th.inkDim, ui::Right);
        // ---- position name: centred between the meters and the connection block ----
        {
            float half = std::min(U.W * 0.5f - (x + w + U.u(40.f)), (rx - U.u(190.f)) - U.W * 0.5f);
            if (half > U.u(40.f))
                U.textIn(ui::Face::SerifItalic, U.u(26.f), p.name, Rectangle{U.W * 0.5f - half, U.u(40.f), 2.f * half, U.u(26.f)},
                         ui::withA(U.th.inkDim, 0.9f), ui::Center);
        }

        // ---- rhythm gauge, right edge: where you are, and (hints) what she wants ----
        if (!p.solo || true) {
            float gx = U.W - U.u(70.f), gy0 = U.H * 0.32f, gh = U.H * 0.36f, gw = U.u(6.f);
            U.fillRect(Rectangle{gx, gy0, gw, gh}, Color{255, 236, 224, 26}, gw * 0.5f);
            if (settings.hints == 2) {
                float c = 0.5f, a = night_.wantAmp;
                float top = gy0 + gh * clamp01(c - a * 0.5f), bot = gy0 + gh * clamp01(c + a * 0.5f);
                ui::Box band; band.radius = U.u(5.f); band.fill = ui::withA(U.th.rose, 0.18f); band.border = std::max(1.f, U.u(1.f));
                band.borderCol = ui::withA(U.th.rose, 0.45f);
                U.box(Rectangle{gx - U.u(6.f), top, gw + U.u(12.f), std::max(U.u(8.f), bot - top)}, band);
            }
            float my = gy0 + gh * clamp01(depth_);
            U.glow(gx + gw * 0.5f, my, U.u(18.f), ui::withA(U.th.accent, 0.55f));
            ui::Box k; k.radius = U.u(7.f); k.fill = Color{255, 244, 236, 255};
            U.box(Rectangle{gx + gw * 0.5f - U.u(7.f), my - U.u(7.f), U.u(14.f), U.u(14.f)}, k);
            // tempo dots: your rhythm vs. hers
            float tx = gx + gw * 0.5f, ty = gy0 + gh + U.u(34.f);
            U.text(ui::Face::SansBold, U.u(11.f), "TEMPO", tx, ty, U.th.inkFaint, ui::Center, 0.2f);
            float yours = night_.tempo, wants = night_.wantTempo;
            char tb[32]; std::snprintf(tb, sizeof tb, "%.1f", (double)yours);
            U.text(ui::Face::SerifBold, U.u(24.f), tb, tx, ty + U.u(16.f), ui::mixc(U.th.inkDim, U.th.ink, night_.sync), ui::Center);
            if (settings.hints == 2 && !p.solo) {
                const char* arrow = yours < wants - 0.25f ? "\xe2\x86\x91" : (yours > wants + 0.3f ? "\xe2\x86\x93" : "\xe2\x80\xa2");
                U.text(ui::Face::Sans, U.u(18.f), arrow, tx, ty + U.u(46.f), ui::withA(U.th.rose, 0.9f), ui::Center);
            }
            if (rhythm_) U.text(ui::Face::SansBold, U.u(11.f), "AUTO", tx, gy0 - U.u(28.f), U.th.accent, ui::Center, 0.2f);
        }

        // ---- her voice (subtitle) + desire hint ----
        float subA = clamp01(subtitleT_ * 2.5f) * clamp01((subtitleDur_ - subtitleT_) * 1.5f);
        if (!subtitle_.empty() && subA > 0.01f) {
            float a0 = U.alpha;
            U.alpha = a0 * subA;
            float sy = U.H - U.u(236.f) - (barOpen_ ? U.u(170.f) : 0.f);
            std::string q = "\xe2\x80\x9c" + subtitle_ + "\xe2\x80\x9d";
            float spx = U.u(34.f), maxW = U.W - U.u(240.f);
            float tw = std::min(U.measure(ui::Face::SerifItalic, spx, q).x, maxW);
            int lines = 1;
            float th = U.measureParagraph(ui::Face::SerifItalic, spx, q, maxW, 1.2f, &lines);
            U.glow(U.W * 0.5f, sy + U.u(20.f), tw * 0.35f, Color{0, 0, 0, 120});
            // long lines wrap upwards: the last line keeps its place above the hints
            U.paragraph(ui::Face::SerifItalic, spx, q, U.W * 0.5f - maxW * 0.5f, sy - (th - spx * 1.2f), maxW, U.th.ink, 1.2f, ui::Center);
            (void)lines;
            U.alpha = a0;
        }
        if (settings.hints == 2 && desireShow_ > 0.01f && shownDesire_ != Desire::None) {
            float a0 = U.alpha;
            U.alpha = a0 * desireShow_;
            const char* h = desireHint(shownDesire_);
            float hy = U.H - U.u(178.f) - (barOpen_ ? U.u(170.f) : 0.f);
            float hw = std::min(U.measure(ui::Face::SansMedium, U.u(17.f), h).x + U.u(44.f), U.W - U.u(80.f));
            float prog = night_.desireTimeout > 0.f ? clamp01(1.f - night_.desireAge / night_.desireTimeout) : 0.f;
            Rectangle r{U.W * 0.5f - hw * 0.5f, hy, hw, U.u(38.f)};
            ui::Box b; b.radius = r.height * 0.5f; b.fill = Color{30, 16, 22, 150}; b.border = std::max(1.f, U.u(1.f));
            b.borderCol = ui::withA(U.th.rose, 0.55f);
            U.box(r, b);
            ui::Box pr; pr.radius = U.u(1.5f); pr.fill = ui::withA(U.th.rose, 0.8f);
            U.box(Rectangle{r.x + r.height * 0.5f, r.y + r.height - U.u(5.f), (r.width - r.height) * prog, U.u(2.f)}, pr);
            U.textIn(ui::Face::SansMedium, U.u(17.f), h, Rectangle{r.x + U.u(22.f), hy, r.width - U.u(44.f), r.height - U.u(2.f)}, U.th.ink, ui::Center);
            U.alpha = a0;
        }
        // ---- toasts ----
        for (size_t i = 0; i < toasts_.size(); ++i) {
            float t = toasts_[i].first;
            float a = clamp01(t * 4.f) * clamp01((2.4f - t) * 1.6f);
            float a0 = U.alpha;
            U.alpha = a0 * a;
            U.textIn(ui::Face::SerifBold, U.u(28.f), toasts_[i].second,
                     Rectangle{U.u(60.f), U.H * 0.36f - U.u(30.f) * t - U.u(36.f) * (float)i, U.W - U.u(120.f), U.u(28.f)}, U.th.accent, ui::Center);
            U.alpha = a0;
        }
        // ---- how to move (first minute, or while nothing happens) ----
        float idleHints = night_.sinceStroke > 6.f ? 1.f : 0.f;
        float hintA = std::max(clamp01((45.f - hintsT_) * 0.5f), idleHints) * (barOpen_ ? 0.f : 1.f);
        if (hintA > 0.01f) {
            float a0 = U.alpha;
            U.alpha = a0 * hintA;
            const char* line = p.solo ? "Drag or scroll to set her pace  \xc2\xb7  W/S tempo  \xc2\xb7  A/D intensity"
                                      : "Drag or scroll to move  \xc2\xb7  hold Space for a rhythm  \xc2\xb7  W/S tempo  \xc2\xb7  A/D depth";
            U.textIn(ui::Face::SansMedium, U.u(16.f), line, Rectangle{U.u(40.f), U.H - U.u(128.f), U.W - U.u(80.f), U.u(16.f)},
                     ui::withA(U.th.ink, 0.9f), ui::Center);
            U.alpha = a0;
        }
    }
    if (!hudHidden_ && hudA > 0.01f) { U.alpha = hudA; drawActionBar(); }
    U.alpha = 1.f;
    if (introCard_ > 0.f && !hudHidden_) drawIntroCard(clamp01(std::min(introCard_ / 0.6f, (14.f - introCard_) / 0.8f)));
    barT_ = approach(barT_, barOpen_ ? 1.f : 0.f, 10.f, frameDt_);
    if (barT_ > 0.01f) drawPositionBar(barT_);
    // climax veil
    if (climaxGlow_ > 0.02f) {
        U.alpha = 1.f;
        U.glow(U.W * 0.5f, U.H * 0.5f, U.W * 0.45f * climaxGlow_, ui::withA(Color{255, 200, 170, 255}, 0.12f * climaxGlow_));
    }
}

void Game::drawIntroCard(float a) {
    ui::Ui& U = ui;
    U.alpha = a;
    ui::ScaleScope screenFit(U, U.fitScale(620.f, 250.f, 24.f));
    float w = U.u(620.f), h = U.u(250.f), x = U.W * 0.5f - w * 0.5f, y = U.H * 0.5f - h * 0.5f - U.u(40.f) + (1.f - a) * U.u(16.f);
    U.panel(Rectangle{x, y, w, h}, 24.f, 0.6f);
    ui::ClipScope clip(U, Rectangle{x, y, w, h}, "intro card");
    U.text(ui::Face::SansBold, U.u(13.f), "YOUR FIRST NIGHT", x + w * 0.5f, y + U.u(26.f), U.th.accent, ui::Center, 0.24f);
    U.paragraph(ui::Face::SerifItalic, U.u(25.f), "Move together, then listen. She'll tell you what she wants \xe2\x80\x94 and he'll tell you when to slow down.",
                x + U.u(40.f), y + U.u(58.f), w - U.u(80.f), U.th.ink, 1.3f, ui::Center);
    // the key hints: measured, centred, and shrunk together if the row would not fit the card
    struct H { const char* key; const char* label; };
    const H hs[4] = {{"Drag", "Move"}, {"Space", "Rhythm"}, {"Tab", "Positions"}, {"Enter", "Got it"}};
    float gap = U.u(24.f), total = -gap;
    for (const H& k : hs) total += U.keyHintWidth(k.key, k.label, 16.f) + gap;
    float avail = w - U.u(60.f);
    ui::ScaleScope rowFit(U, std::min(1.f, avail / std::max(1.f, total)));
    gap = U.u(24.f); total = -gap;
    for (const H& k : hs) total += U.keyHintWidth(k.key, k.label, 16.f) + gap;
    float hx = x + w * 0.5f - total * 0.5f, hy = y + h - U.u(64.f);
    for (const H& k : hs) hx += U.keyHint(hx, hy, k.key, k.label, 16.f) + gap;
    U.alpha = 1.f;
}

// Every act and control in one bar: clickable, with its key, lit when active, dimmed when this act has no such move.
void Game::drawActionBar() {
    ui::Ui& U = ui;
    const PositionInfo& p = positions_[(size_t)posIndex_];
    auto has = [&](const char* id) { for (auto& q : positions_) if (q.id == id) return actAllowed(id); return false; };
    const char* camNames[5] = {"Side", "Front", "Above", "His view", "Her face"};
    struct Btn { const char* key; std::string label; bool enabled, active; int action; const char* tip; };
    std::vector<Btn> bs = {
        {"Tab", "Acts", true, barOpen_, 0, "Choose what you do together"},
        {"Space", "Rhythm", true, rhythm_, 1, "An easy rhythm: hold Space, or click to keep it going"},
        {"R", "Touch", p.canTouch, rubTarget_ > 0.5f, 2, "Her fingers join in"},
        {"E", "Kiss", p.canKiss, embraceTarget_ > 0.5f, 3, "Lean in close and kiss"},
        {"O", "Her lips", has("oral"), p.id == "oral", 4, "Her lips on him"},
        {"L", "His tongue", has("lick"), p.id == "lick", 5, "His mouth on her"},
        {"X", "Alone", has("solo"), p.id == "solo", 6, "She shows him how she likes it"},
        {"C", camNames[std::max(0, std::min(4, camMode_))], true, false, 7, "Next camera"},
        {"M", moodName((Mood)settings.lightMood), true, false, 8, "Next lighting mood"},
    };
    int tipIdx = -1;
    Rectangle tipR{};
    auto measureBar = [&](std::vector<float>& ws) {
        float px = U.u(16.f), kp = U.u(13.f), gap = U.u(10.f), total = -gap;
        ws.clear();
        for (auto& b : bs) {
            float kw = std::max(U.measure(ui::Face::SansBold, kp, b.key).x + U.u(14.f), U.u(26.f));
            float w = kw + U.u(10.f) + U.measure(ui::Face::SansMedium, px, b.label).x + U.u(30.f);
            ws.push_back(w); total += w + gap;
        }
        return total + U.u(28.f);   // the glass around it
    };
    std::vector<float> ws;
    float natural = measureBar(ws);
    ui::ScaleScope barFit(U, std::min(1.f, (U.W - U.u(32.f)) / std::max(1.f, natural)));
    float total = measureBar(ws) - U.u(28.f);
    float px = U.u(16.f), kp = U.u(13.f), h = U.u(48.f), gap = U.u(10.f);
    float x = U.W * 0.5f - total * 0.5f, y = U.H - U.u(28.f) - h;
    actionBarRect_ = Rectangle{x - U.u(14.f), y - U.u(10.f), total + U.u(28.f), h + U.u(20.f)};
    ui::Box back; back.radius = U.u(22.f); back.glass = true; back.glassTint = U.th.glass; back.glassAmt = 0.65f;
    back.fill = Color{255, 240, 230, 6}; back.border = std::max(1.f, U.u(1.f)); back.borderCol = U.th.hairline;
    back.shadow = U.u(24.f); back.shadowCol = Color{0, 0, 0, 90};
    U.box(actionBarRect_, back);
    for (size_t i = 0; i < bs.size(); ++i) {
        const Btn& b = bs[i];
        Rectangle r{x, y, ws[i], h};
        bool over = b.enabled && CheckCollisionPointRec(U.in.mouse, r);
        if (CheckCollisionPointRec(U.in.mouse, r)) { tipIdx = (int)i; tipR = r; }
        float hv = U.anim("ab#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
        float on = U.anim("ab#on" + std::to_string(i), b.active ? 1.f : 0.f, 12.f);
        float pr = U.anim("ab#pr" + std::to_string(i), over && U.in.mouseDown ? 1.f : 0.f, 20.f);
        r = Rectangle{r.x + pr * U.u(1.5f), r.y + pr * U.u(1.5f), r.width - pr * U.u(3.f), r.height - pr * U.u(3.f)};
        float a0 = U.alpha;
        if (!b.enabled) U.alpha = a0 * 0.38f;
        ui::Box pill; pill.radius = h * 0.5f;
        pill.fill = ui::mixc(Color{255, 236, 224, (unsigned char)(10 + 22 * hv)}, ui::withA(U.th.accent, 0.24f), on);
        pill.border = std::max(1.f, U.u(1.2f));
        pill.borderCol = ui::mixc(Color{255, 232, 214, 50}, ui::withA(U.th.accent, 0.95f), std::max(on, hv * 0.6f));
        if (on > 0.01f || hv > 0.01f) { pill.shadow = U.u(14.f) * std::max(on, hv); pill.shadowCol = ui::withA(U.th.accent, 0.25f); }
        U.box(r, pill);
        float kw = std::max(U.measure(ui::Face::SansBold, kp, b.key).x + U.u(14.f), U.u(26.f));
        Rectangle kr{r.x + U.u(12.f), r.y + (h - U.u(26.f)) * 0.5f, kw, U.u(26.f)};
        ui::Box key; key.radius = U.u(6.f); key.fill = Color{16, 9, 12, 170}; key.border = std::max(1.f, U.u(1.f)); key.borderCol = Color{255, 232, 214, 90};
        U.box(kr, key);
        U.text(ui::Face::SansBold, kp, b.key, kr.x + kr.width * 0.5f, kr.y + (kr.height - kp) * 0.5f - U.u(1.f), U.th.ink, ui::Center);
        U.text(ui::Face::SansMedium, px, b.label, kr.x + kw + U.u(10.f), r.y + (h - px) * 0.5f - U.u(1.f), ui::mixc(U.th.inkDim, U.th.ink, std::max(on, hv)));
        U.alpha = a0;
        if (over && U.in.clicked && !night_.finished) {
            if (ui.onSound) ui.onSound(1);
            switch (b.action) {
                case 0: barOpen_ = !barOpen_; break;
                case 1: rhythmToggle_ = !rhythmToggle_; break;
                case 2: rubTarget_ = rubTarget_ > 0.5f ? 0.f : 1.f; break;
                case 3: embraceTarget_ = embraceTarget_ > 0.5f ? 0.f : 1.f; break;
                case 4: actById("oral"); break;
                case 5: actById("lick"); break;
                case 6: actById("solo"); break;
                case 7: camMode_ = nextCamMode(camMode_, p.solo); cameraForPosition(0.9f); camUser_ = false; break;
                case 8: settings.lightMood = (settings.lightMood + 1) % 3; break;
            }
        }
        x += ws[i] + gap;
    }
    // what the hovered button does (or why it is dimmed)
    if (barOpen_) tipIdx = -1;   // the open picker explains itself
    float tipA = U.anim("ab#tip", tipIdx >= 0 ? 1.f : 0.f, 10.f);
    if (tipIdx >= 0) { lastTip_ = tipIdx; lastTipR_ = tipR; }
    if (tipA > 0.01f && lastTip_ >= 0 && lastTip_ < (int)bs.size()) {
        const Btn& b = bs[(size_t)lastTip_];
        std::string tip = b.enabled ? b.tip : std::string("Not in this act");
        float tp = U.u(15.f);
        float tw = U.measure(ui::Face::SansMedium, tp, tip).x + U.u(32.f);
        float tx = std::max(U.u(12.f), std::min(U.W - tw - U.u(12.f), lastTipR_.x + lastTipR_.width * 0.5f - tw * 0.5f));
        float ty = actionBarRect_.y - U.u(46.f) + (1.f - tipA) * U.u(6.f);
        float a0 = U.alpha;
        U.alpha = a0 * tipA;
        ui::Box tb; tb.radius = U.u(15.f); tb.fill = Color{20, 12, 16, 228}; tb.border = std::max(1.f, U.u(1.f)); tb.borderCol = ui::withA(U.th.accent, 0.45f);
        tb.shadow = U.u(14.f); tb.shadowCol = Color{0, 0, 0, 120};
        U.box(Rectangle{tx, ty, tw, U.u(32.f)}, tb);
        U.text(ui::Face::SansMedium, tp, tip, tx + tw * 0.5f, ty + (U.u(32.f) - tp) * 0.5f - U.u(1.f), b.enabled ? U.th.ink : U.th.inkDim, ui::Center);
        U.alpha = a0;
    }
}

void Game::drawPositionBar(float a) {
    ui::Ui& U = ui;
    U.alpha = a;
    const char* groups[4] = {"Foreplay", "Oral", "Positions", "Solo"};
    int counts[4] = {0, 0, 0, 0};
    for (auto& q : positions_) for (int g = 0; g < 4; ++g) if (q.group == groups[g] && actAllowed(q.id)) ++counts[g];
    // no foreplay acts in the clips yet: the Foreplay row offers kissing and her touch, which join the current act
    const bool foreplayToggles = counts[0] == 0;
    auto itemsIn = [&](int g) { return (g == 0 && foreplayToggles) ? 2 : counts[g]; };
    // as many cards per row as the window allows: a group with more acts than that wraps onto more rows
    int perRow = 1, rows = 0, maxRow = 0;
    auto layout = [&]() {
        float cw = U.u(158.f), gap = U.u(12.f), headW = U.u(150.f);
        float availW = U.W - U.u(80.f) - U.u(56.f) - headW;
        perRow = std::max(1, (int)std::floor((availW + gap) / (cw + gap)));
        rows = 0; maxRow = 0;
        for (int g = 0; g < 4; ++g) {
            int n = itemsIn(g);
            if (n <= 0) continue;
            rows += (n + perRow - 1) / perRow;
            maxRow = std::max(maxRow, std::min(n, perRow));
        }
    };
    layout();
    // taller than the room above the action bar: the whole picker shrinks (and is laid out again at that size)
    float natH = (float)rows * U.u(110.f) + U.u(70.f), availH = U.H - U.u(120.f) - U.u(96.f);
    ui::ScaleScope fit(U, std::min(1.f, availH / std::max(1.f, natH)));
    layout();
    float cw = U.u(158.f), ch = U.u(96.f), gap = U.u(12.f), headW = U.u(150.f);
    float rowW = headW + (float)maxRow * (cw + gap) - gap;
    float rowH = ch + U.u(14.f);
    float pw = rowW + U.u(56.f), ph = (float)rows * rowH + U.u(70.f);
    float x0 = U.W * 0.5f - pw * 0.5f, y0 = U.H - U.u(120.f) - ph + (1.f - ui::easeOut(a)) * U.u(50.f);
    Rectangle panel{x0, y0, pw, ph};
    U.panel(panel, 24.f, 0.64f);
    ui::ClipScope clip(U, panel, "acts picker");
    float hintW = U.measure(ui::Face::Sans, U.u(13.f), "Number keys or click  \xc2\xb7  Tab closes").x;
    U.textIn(ui::Face::SansBold, U.u(13.f), "CHOOSE WHAT YOU DO TOGETHER",
             Rectangle{x0 + U.u(28.f), y0 + U.u(22.f), std::max(U.u(40.f), pw - U.u(84.f) - hintW), U.u(13.f)}, U.th.accent, ui::Left, 0.22f);
    U.text(ui::Face::Sans, U.u(13.f), "Number keys or click  \xc2\xb7  Tab closes", x0 + pw - U.u(28.f), y0 + U.u(22.f), U.th.inkFaint, ui::Right);
    float y = y0 + U.u(54.f);
    auto cardAt = [&](int k) {   // the k-th card of a group, wrapping after perRow
        return Rectangle{x0 + U.u(28.f) + headW + (float)(k % perRow) * (cw + gap), y + (float)(k / perRow) * rowH, cw, ch};
    };
    auto groupLabel = [&](int g) {
        U.textIn(ui::Face::Serif, U.u(26.f), groups[g], Rectangle{x0 + U.u(28.f), y + ch * 0.5f - U.u(16.f), headW - U.u(12.f), U.u(26.f)}, U.th.inkDim);
    };
    for (int g = 0; g < 4; ++g) {
        int n = itemsIn(g);
        if (n <= 0) continue;
        groupLabel(g);
        if (g == 0 && foreplayToggles) {
            const PositionInfo& cp = positions_[(size_t)posIndex_];
            struct T { const char* key; const char* name; bool can, on; };
            const T ts[2] = {{"E", "Kissing", cp.canKiss, embraceTarget_ > 0.5f}, {"R", "Her Touch", cp.canTouch, rubTarget_ > 0.5f}};
            for (int k = 0; k < 2; ++k) {
                Rectangle r = cardAt(k);
                bool over = ts[k].can && CheckCollisionPointRec(U.in.mouse, r);
                float h = U.anim(std::string("posbar#f") + ts[k].key, over ? 1.f : 0.f, 14.f);
                float a0 = U.alpha;
                if (!ts[k].can) U.alpha = a0 * 0.4f;
                if (over && U.in.clicked) {
                    float& tgt = k == 0 ? embraceTarget_ : rubTarget_;
                    tgt = ts[k].on ? 0.f : 1.f;
                    barOpen_ = false;
                }
                ui::Box b; b.radius = U.u(16.f);
                b.fill = ts[k].on ? ui::withA(U.th.accent, 0.22f) : Color{255, 236, 224, (unsigned char)(10 + 20 * h)};
                b.border = std::max(1.f, U.u(1.2f));
                b.borderCol = ts[k].on ? ui::withA(U.th.accent, 0.95f) : ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.6f), h);
                b.shadow = U.u(14.f) * (h + (ts[k].on ? 0.6f : 0.f)); b.shadowCol = ui::withA(U.th.accent, 0.2f);
                float lift = h * U.u(3.f);
                U.box(Rectangle{r.x, r.y - lift, r.width, r.height}, b);
                U.text(ui::Face::SansBold, U.u(13.f), ts[k].key, r.x + U.u(14.f), r.y + U.u(11.f) - lift, ts[k].on ? U.th.accent : U.th.inkFaint);
                U.textIn(ui::Face::Serif, U.u(22.f), ts[k].name, Rectangle{r.x + U.u(14.f), r.y + U.u(38.f) - lift, r.width - U.u(28.f), U.u(22.f)},
                         ts[k].on ? U.th.ink : ui::mixc(U.th.inkDim, U.th.ink, h));
                U.textIn(ui::Face::Sans, U.u(12.f), !ts[k].can ? "not in this act" : (ts[k].on ? "on \xc2\xb7 click to stop" : "joins this act"),
                         Rectangle{r.x + U.u(14.f), r.y + ch - U.u(24.f) - lift, r.width - U.u(28.f), U.u(12.f)}, ts[k].on ? U.th.accent : U.th.inkFaint);
                U.alpha = a0;
            }
        } else {
            int k = 0;
            for (size_t i = 0; i < positions_.size(); ++i) {
                const PositionInfo& p = positions_[i];
                if (p.group != groups[g] || !actAllowed(p.id)) continue;
                Rectangle r = cardAt(k++);
                bool cur = (int)i == posIndex_;
                bool over = CheckCollisionPointRec(U.in.mouse, r);
                float h = U.anim("posbar#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
                if (over && U.in.clicked) { setPosition((int)i); barOpen_ = false; }
                ui::Box b; b.radius = U.u(16.f);
                b.fill = cur ? ui::withA(U.th.accent, 0.22f) : Color{255, 236, 224, (unsigned char)(10 + 20 * h)};
                b.border = std::max(1.f, U.u(1.2f));
                b.borderCol = cur ? ui::withA(U.th.accent, 0.95f) : ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.6f), h);
                b.shadow = U.u(14.f) * (h + (cur ? 0.6f : 0.f)); b.shadowCol = ui::withA(U.th.accent, 0.2f);
                float lift = h * U.u(3.f);
                U.box(Rectangle{r.x, r.y - lift, r.width, r.height}, b);
                int idx = (int)i;
                if (idx < 10) {
                    char key[8]; std::snprintf(key, sizeof key, "%d", (idx + 1) % 10);
                    U.text(ui::Face::SansBold, U.u(13.f), key, r.x + U.u(14.f), r.y + U.u(11.f) - lift, cur ? U.th.accent : U.th.inkFaint);
                }
                float nov = night_.novelty.count(p.id) ? night_.novelty.at(p.id) : 1.f;
                for (int d = 0; d < 3; ++d) {
                    bool lit = nov > 0.34f * (float)d + 0.1f;
                    U.fillRect(Rectangle{r.x + r.width - U.u(16.f) - (float)d * U.u(9.f), r.y + U.u(14.f) - lift, U.u(5.f), U.u(5.f)},
                               lit ? ui::withA(U.th.rose, 0.85f) : Color{255, 236, 224, 40}, U.u(2.5f));
                }
                // two lines at most: a long name shrinks a little, then ends in an ellipsis
                float npx = U.u(22.f), nw = r.width - U.u(28.f);
                int lines = 1;
                U.measureParagraph(ui::Face::Serif, npx, p.name, nw, 1.0f, &lines);
                if (lines > 2) npx = U.u(18.f);
                U.measureParagraph(ui::Face::Serif, npx, p.name, nw, 1.0f, &lines);
                Color nc = cur ? U.th.ink : ui::mixc(U.th.inkDim, U.th.ink, h);
                if (lines <= 2) U.paragraph(ui::Face::Serif, npx, p.name, r.x + U.u(14.f), r.y + U.u(38.f) - lift, nw, nc, 1.0f);
                else U.textIn(ui::Face::Serif, npx, p.name, Rectangle{r.x + U.u(14.f), r.y + U.u(38.f) - lift, nw, npx}, nc);
            }
        }
        y += (float)((n + perRow - 1) / perRow) * rowH;
    }
    U.alpha = 1.f;
}

}  // namespace ag
