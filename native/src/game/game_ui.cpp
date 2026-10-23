#include "game/game.h"
#include "app/updater.h"
#include "app/platform.h"
#include "gfx/texture.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
const char* kToneLabel[3] = {"Honey", "Caramel", "Deep"};
const Color kToneSwatch[3] = {{214, 160, 120, 255}, {170, 108, 72, 255}, {104, 62, 44, 255}};
const Color kHairSwatch[6] = {{52, 32, 24, 255}, {104, 56, 34, 255}, {132, 52, 28, 255}, {150, 44, 30, 255}, {24, 20, 22, 255}, {196, 140, 82, 255}};
const char* kHairLabel[6] = {"Espresso", "Chestnut", "Copper", "Auburn", "Raven", "Honey"};
}  // namespace

void Game::heroLogo(float cx, float y, float size, float a) {
    ui::Ui& U = ui;
    float a0 = U.alpha;
    U.alpha = a0 * a;
    U.glow(cx, y + size * 0.55f, size * 1.6f, ui::withA(U.th.accent, 0.10f));
    U.text(ui::Face::Serif, size, "Afterglow", cx, y, U.th.ink, ui::Center, 0.02f);
    float lw = size * 0.9f;
    U.line(cx - lw, y + size * 1.12f, cx - size * 0.12f, y + size * 1.12f, std::max(1.f, U.u(1.f)), ui::withA(U.th.accent, 0.6f));
    U.line(cx + size * 0.12f, y + size * 1.12f, cx + lw, y + size * 1.12f, std::max(1.f, U.u(1.f)), ui::withA(U.th.accent, 0.6f));
    U.diamond(cx, y + size * 1.12f, U.u(5.f), U.th.accent);
    U.alpha = a0;
}

// ------------------------------------------------------------------ title
void Game::drawTitle() {
    ui::Ui& U = ui;
    float t = screenTime_;
    // soft vignette bands so the type sits on the scene (logo in the dark above the bed, prompt below)
    ui::Box top; top.fill = Color{6, 3, 5, 200}; top.fill2 = Color{6, 3, 5, 0}; top.grad = 2;
    U.box(Rectangle{0.f, 0.f, U.W, U.H * 0.42f}, top);
    ui::Box band; band.fill = Color{6, 3, 5, 0}; band.fill2 = Color{6, 3, 5, 150}; band.grad = 2;
    U.box(Rectangle{0.f, U.H * 0.7f, U.W, U.H * 0.3f}, band);
    float a = ui::easeOut(t / 1.6f);
    float ly = U.H * 0.12f;
    heroLogo(U.W * 0.5f, ly - U.u(20.f) * (1.f - a), U.u(150.f), a);
    U.alpha = ui::easeOut((t - 0.8f) / 1.2f);
    U.text(ui::Face::SerifItalic, U.u(30.f), "a night for two", U.W * 0.5f, ly + U.u(190.f), U.th.inkDim, ui::Center);
    float blink = 0.55f + 0.45f * std::sin(t * 2.4f);
    U.alpha = ui::easeOut((t - 1.6f) / 1.0f) * blink;
    U.text(ui::Face::SansBold, U.u(15.f), "PRESS ANY KEY", U.W * 0.5f, U.H - U.u(120.f), U.th.ink, ui::Center, 0.32f);
    U.alpha = ui::easeOut((t - 1.6f) / 1.0f) * 0.8f;
    U.text(ui::Face::Sans, U.u(14.f), "For adults only. A work of fiction between two consenting adults.", U.W * 0.5f, U.H - U.u(56.f),
           U.th.inkFaint, ui::Center);
    U.alpha = 1.f;
    if (t > 0.6f && !switching_ && (GetKeyPressed() != 0 || U.in.clicked)) go(Screen::Menu, 0.45f);
}

// ------------------------------------------------------------------ main menu
void Game::drawMenu() {
    ui::Ui& U = ui;
    float t = screenTime_;
    ui::Box shade; shade.fill = Color{8, 4, 6, 215}; shade.fill2 = Color{8, 4, 6, 0}; shade.grad = 1;
    U.box(Rectangle{0.f, 0.f, U.W * 0.55f, U.H}, shade);
    float x = U.u(150.f);
    float ea = ui::easeOut(t / 0.8f);
    U.alpha = ea;
    U.glow(x + U.u(170.f), U.u(250.f), U.u(220.f), ui::withA(U.th.accent, 0.07f));
    U.text(ui::Face::Serif, U.u(104.f), "Afterglow", x - U.u(6.f), U.u(170.f) - U.u(12.f) * (1.f - ea), U.th.ink, ui::Left, 0.02f);
    U.text(ui::Face::SerifItalic, U.u(26.f), "a night for two", x, U.u(300.f), U.th.inkDim);

    bool canContinue = profile.night.valid;
    float y = U.u(420.f);
    struct Item { const char* id; const char* label; };
    std::vector<Item> items;
    if (canContinue) items.push_back({"m_cont", "Continue the night"});
    items.push_back({"m_new", canContinue ? "A new night" : "Begin the night"});
    items.push_back({"m_cust", "Customize"});
    items.push_back({"m_set", "Settings"});
    items.push_back({"m_quit", "Quit"});
    for (size_t i = 0; i < items.size(); ++i) {
        U.alpha = ui::easeOut((t - 0.12f - 0.07f * (float)i) / 0.6f);
        float off = (1.f - U.alpha) * U.u(-24.f);
        if (U.menuItem(items[i].id, items[i].label, x - U.u(30.f) + off, y, U.u(560.f))) {
            std::string id = items[i].id;
            if (id == "m_cont") { startNight(true); go(Screen::Play, 0.6f); }
            else if (id == "m_new") {
                if (canContinue && !confirmNew_) confirmNew_ = true;
                else { startNight(false); go(Screen::Play, 0.6f); }
            }
            else if (id == "m_cust") go(Screen::Customize, 0.4f);
            else if (id == "m_set") { settingsReturn_ = Screen::Menu; go(Screen::Settings, 0.3f); }
            else if (id == "m_quit") { saveAll(); quit_ = true; }
        }
        y += U.u(74.f);
    }
    U.alpha = ea;
    if (confirmNew_) {
        U.text(ui::Face::SerifItalic, U.u(22.f), "Start over? Your current night will be lost \xe2\x80\x94 choose again to confirm.",
               x, y + U.u(10.f), U.th.rose);
    }
    // footer: stats + version
    char b[160];
    const Stats& st = profile.stats;
    if (st.nights > 0)
        std::snprintf(b, sizeof b, "%d night%s  \xc2\xb7  %d together  \xc2\xb7  best %d", st.nights, st.nights == 1 ? "" : "s", st.together, st.bestScore);
    else std::snprintf(b, sizeof b, "Your first night awaits");
    U.text(ui::Face::Sans, U.u(16.f), b, x, U.H - U.u(96.f), U.th.inkFaint);
    std::snprintf(b, sizeof b, "Version %s", Updater::current().c_str());
    U.text(ui::Face::Sans, U.u(14.f), b, x, U.H - U.u(66.f), ui::withA(U.th.inkFaint, 0.7f));
    U.alpha = 1.f;
    if (updater) drawUpdateCard(U.W - U.u(500.f), U.H - U.u(230.f), U.u(440.f));
    if (U.in.back && !switching_) go(Screen::Title, 0.4f);
}

void Game::drawUpdateCard(float x, float y, float w) {
    ui::Ui& U = ui;
    Updater::State s = updater->state();
    if (s != Updater::State::Available && s != Updater::State::Downloading && s != Updater::State::Ready &&
        !(s == Updater::State::Failed && !updater->latest().empty())) return;
    if (s == Updater::State::Available && updater->latest() == settings.skippedVersion) return;   // "Later" for this one
    float a = U.anim("upd#a", 1.f, 3.f);
    U.alpha = a;
    float h = U.u(170.f);
    U.panel(Rectangle{x, y, w, h}, 22.f, 0.6f);
    U.text(ui::Face::SansBold, U.u(13.f), "UPDATE AVAILABLE", x + U.u(26.f), y + U.u(22.f), U.th.accent, ui::Left, 0.22f);
    char b[96];
    std::snprintf(b, sizeof b, "Afterglow %s", updater->latest().c_str());
    U.text(ui::Face::Serif, U.u(34.f), b, x + U.u(26.f), y + U.u(44.f), U.th.ink);
    std::string notes = updater->notes();
    if (notes.size() > 70) notes = notes.substr(0, 67) + "...";
    if (s == Updater::State::Downloading) {
        float p = updater->progress();
        std::snprintf(b, sizeof b, "Downloading\xe2\x80\xa6 %d%%", (int)std::lround(p * 100.f));
        U.text(ui::Face::Sans, U.u(16.f), b, x + U.u(26.f), y + U.u(92.f), U.th.inkDim);
        U.fillRect(Rectangle{x + U.u(26.f), y + U.u(126.f), w - U.u(52.f), U.u(5.f)}, Color{255, 236, 224, 34}, U.u(2.5f));
        ui::Box pb; pb.radius = U.u(2.5f); pb.fill = U.th.accent; pb.fill2 = U.th.rose; pb.grad = 1;
        U.box(Rectangle{x + U.u(26.f), y + U.u(126.f), std::max(U.u(5.f), (w - U.u(52.f)) * p), U.u(5.f)}, pb);
    } else {
        std::string line = s == Updater::State::Failed ? updater->error() : (notes.empty() ? "A fresh build is ready." : notes);
        if (line.size() > 64) line = line.substr(0, 61) + "...";
        U.text(ui::Face::Sans, U.u(15.f), line, x + U.u(26.f), y + U.u(90.f), s == Updater::State::Failed ? U.th.rose : U.th.inkDim);
        Rectangle b1{x + U.u(26.f), y + h - U.u(56.f), U.u(190.f), U.u(40.f)};
        Rectangle b2{x + U.u(230.f), y + h - U.u(56.f), U.u(110.f), U.u(40.f)};
#if defined(_WIN32)
        const char* act = s == Updater::State::Ready ? "Install & restart" : "Update now";
#else
        const char* act = "Get it";
#endif
        if (U.button("upd#go", act, b1, true)) {
#if defined(_WIN32)
            if (s == Updater::State::Ready) { saveAll(); if (updater->install()) quit_ = true; }
            else updater->downloadAsync();
#else
            updater->install();
#endif
        }
        if (U.button("upd#later", "Later", b2)) { settings.skippedVersion = updater->latest(); saveAll(); }
    }
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ customisation
void Game::drawCustomize() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.6f);
    float px = U.W - U.u(56.f) - U.u(700.f), py = U.u(56.f), pw = U.u(700.f), ph = U.H - U.u(112.f);
    U.alpha = ea;
    U.panel(Rectangle{px + (1.f - ea) * U.u(40.f), py, pw, ph}, 28.f, 0.6f);
    float x = px + U.u(48.f) + (1.f - ea) * U.u(40.f), w = pw - U.u(96.f), y = py + U.u(40.f);
    U.text(ui::Face::Serif, U.u(56.f), "Customize", x, y, U.th.ink);
    y += U.u(84.f);
    int before = custTab_;
    U.tabs("cust#tabs", &custTab_, {"VIVIAN", "MARCUS"}, Rectangle{x, y, w, U.u(44.f)});
    if (custTab_ != before) { showcaseWho_ = custTab_; stage(Stage::Showcase); }
    y += U.u(70.f);
    bool changed = false;
    if (custTab_ == 0) {
        HerLook& h = profile.her;
        U.sectionLabel("PRESETS", x, y, w);
        y += U.u(30.f);
        struct Preset { const char* name; HerLook v; };
        const Preset presets[] = {
            {"Natural", {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, h.tone, h.hair}},
            {"Slim", {-0.55f, -0.45f, 0.45f, -0.4f, -0.75f, 0.15f, 0.f, h.tone, h.hair}},
            {"Curvy", {0.6f, 0.7f, 0.35f, 0.65f, 0.15f, 0.f, 0.1f, h.tone, h.hair}},
            {"Voluptuous", {1.f, 0.85f, -0.1f, 0.9f, 0.55f, 0.f, 0.3f, h.tone, h.hair}},
            {"Athletic", {-0.2f, 0.1f, 0.55f, 0.35f, -0.35f, 0.85f, 0.f, h.tone, h.hair}},
        };
        float cx = x, cw = (w - U.u(4.f * 10.f)) / 5.f;
        for (const Preset& p : presets) {
            bool act = std::fabs(h.bust - p.v.bust) + std::fabs(h.hips - p.v.hips) + std::fabs(h.waist - p.v.waist) +
                       std::fabs(h.butt - p.v.butt) + std::fabs(h.weight - p.v.weight) + std::fabs(h.muscle - p.v.muscle) < 0.05f;
            if (U.chip(std::string("hp#") + p.name, p.name, Rectangle{cx, y, cw, U.u(38.f)}, act)) { h = p.v; changed = true; }
            cx += cw + U.u(10.f);
        }
        y += U.u(66.f);
        U.sectionLabel("FIGURE", x, y, w);
        y += U.u(34.f);
        struct S { const char* id; const char* label; float* v; float lo; };
        const S sl[] = {{"h_bust", "Bust", &h.bust, -1.f}, {"h_hips", "Hips", &h.hips, -1.f}, {"h_waist", "Waist", &h.waist, -1.f},
                        {"h_butt", "Bottom", &h.butt, -1.f}, {"h_weight", "Weight", &h.weight, -1.f}, {"h_muscle", "Tone", &h.muscle, 0.f},
                        {"h_belly", "Belly", &h.belly, 0.f}};
        float colW = (w - U.u(48.f)) * 0.5f;
        for (int i = 0; i < 7; ++i) {
            float sx = x + (i % 2) * (colW + U.u(48.f)), sy = y + (float)(i / 2) * U.u(70.f);
            changed |= U.slider(sl[i].id, sl[i].label, sl[i].v, sl[i].lo, 1.f, Rectangle{sx, sy, colW, U.u(46.f)}, sl[i].lo < 0.f);
        }
        y += U.u(4.f * 70.f + 14.f);
        U.sectionLabel("SKIN & HAIR", x, y, w);
        y += U.u(34.f);
        changed |= U.swatches("h_tone", "Skin tone", &h.tone, {kToneSwatch[0], kToneSwatch[1], kToneSwatch[2]},
                              {kToneLabel[0], kToneLabel[1], kToneLabel[2]}, Rectangle{x, y, colW, U.u(80.f)});
        changed |= U.swatches("h_hair", "Hair colour", &h.hair, std::vector<Color>(kHairSwatch, kHairSwatch + 6),
                              std::vector<std::string>(kHairLabel, kHairLabel + 6), Rectangle{x + colW + U.u(48.f), y, colW, U.u(80.f)});
        y += U.u(100.f);
    } else {
        HisLook& h = profile.him;
        U.sectionLabel("PRESETS", x, y, w);
        y += U.u(30.f);
        struct Preset { const char* name; HisLook v; };
        const Preset presets[] = {
            {"Natural", {0.f, 0.f, 0.f, h.length, h.girth, h.tone}},
            {"Lean", {0.2f, -0.6f, 0.f, h.length, h.girth, h.tone}},
            {"Athletic", {0.85f, -0.2f, 0.f, h.length, h.girth, h.tone}},
            {"Broad", {0.55f, 0.55f, 0.15f, h.length, h.girth, h.tone}},
            {"Dad bod", {-0.2f, 0.45f, 0.75f, h.length, h.girth, h.tone}},
        };
        float cx = x, cw = (w - U.u(40.f)) / 5.f;
        for (const Preset& p : presets) {
            bool act = std::fabs(h.muscle - p.v.muscle) + std::fabs(h.weight - p.v.weight) + std::fabs(h.belly - p.v.belly) < 0.05f;
            if (U.chip(std::string("mp#") + p.name, p.name, Rectangle{cx, y, cw, U.u(38.f)}, act)) { h = p.v; changed = true; }
            cx += cw + U.u(10.f);
        }
        y += U.u(66.f);
        U.sectionLabel("BUILD", x, y, w);
        y += U.u(34.f);
        float colW = (w - U.u(48.f)) * 0.5f;
        changed |= U.slider("m_muscle", "Muscle", &h.muscle, -1.f, 1.f, Rectangle{x, y, colW, U.u(46.f)}, true);
        changed |= U.slider("m_weight", "Weight", &h.weight, -1.f, 1.f, Rectangle{x + colW + U.u(48.f), y, colW, U.u(46.f)}, true);
        changed |= U.slider("m_belly", "Belly", &h.belly, 0.f, 1.f, Rectangle{x, y + U.u(70.f), colW, U.u(46.f)});
        y += U.u(156.f);
        U.sectionLabel("ENDOWMENT", x, y, w);
        y += U.u(34.f);
        changed |= U.slider("m_length", "Length", &h.length, -1.f, 1.f, Rectangle{x, y, colW, U.u(46.f)}, true);
        changed |= U.slider("m_girth", "Girth", &h.girth, -1.f, 1.f, Rectangle{x + colW + U.u(48.f), y, colW, U.u(46.f)}, true);
        y += U.u(86.f);
        U.sectionLabel("SKIN", x, y, w);
        y += U.u(34.f);
        changed |= U.swatches("m_tone", "Skin tone", &h.tone, {kToneSwatch[0], kToneSwatch[1], kToneSwatch[2]},
                              {kToneLabel[0], kToneLabel[1], kToneLabel[2]}, Rectangle{x, y, colW, U.u(80.f)});
        y += U.u(100.f);
    }
    if (changed) applyLooks();
    if (custTab_ == 1 && scene.marcus.loaded()) {
        // the preview shows his endowment at rest; the sliders shape it when aroused
        float k = 0.35f;
        scene.marcus.setBody("length", profile.him.length * k);
        scene.marcus.setBody("girth", profile.him.girth * k);
    }
    // footer
    float by = py + ph - U.u(86.f);
    if (U.button("cust#reset", "Reset", Rectangle{x, by, U.u(150.f), U.u(50.f)})) {
        if (custTab_ == 0) { int tone = profile.her.tone, hair = profile.her.hair; profile.her = HerLook{}; profile.her.tone = tone; profile.her.hair = hair; }
        else { int tone = profile.him.tone; profile.him = HisLook{}; profile.him.tone = tone; }
        applyLooks();
    }
    if (U.button("cust#done", "Done", Rectangle{x + w - U.u(200.f), by, U.u(200.f), U.u(50.f)}, true) || (U.in.back && !switching_)) {
        if (!harness) profile.save(profilePath_);
        go(Screen::Menu, 0.4f);
    }
    // preview caption
    U.alpha = ea * 0.85f;
    U.text(ui::Face::Sans, U.u(15.f), "Drag to turn  \xc2\xb7  Scroll to look closer", (px) * 0.5f, U.H - U.u(70.f), U.th.inkFaint, ui::Center);
    U.text(ui::Face::Serif, U.u(46.f), custTab_ == 0 ? "Vivian" : "Marcus", U.u(80.f), U.u(64.f), U.th.ink);
    U.text(ui::Face::SerifItalic, U.u(22.f), custTab_ == 0 ? "forty-two, warm, unhurried" : "forty, steady, attentive", U.u(82.f), U.u(122.f), U.th.inkDim);
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ settings
void Game::drawSettings() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.5f);
    if (settingsReturn_ == Screen::Pause) U.backdrop(1.f);
    float pw = U.u(1060.f), ph = U.u(800.f);
    float px = U.W * 0.5f - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(24.f);
    U.alpha = ea;
    U.panel(Rectangle{px, py, pw, ph}, 28.f, 0.62f);
    float x = px + U.u(56.f), w = pw - U.u(112.f), y = py + U.u(44.f);
    U.text(ui::Face::Serif, U.u(56.f), "Settings", x, y, U.th.ink);
    y += U.u(88.f);
    U.tabs("set#tabs", &setTab_, {"DISPLAY", "GRAPHICS", "AUDIO", "CONTROLS", "GAME"}, Rectangle{x, y, w, U.u(44.f)});
    y += U.u(78.f);
    float rowH = U.u(60.f);
    Settings before = settings;
    bool displayChanged = false;
    switch (setTab_) {
        case 0: {
            if (U.choice("s_mode", "Window", &settings.windowMode, {"Windowed", "Borderless", "Fullscreen"}, Rectangle{x, y, w, rowH})) displayChanged = true;
            y += rowH + U.u(10.f);
            if (U.toggle("s_vsync", "Vertical sync", &settings.vsync, Rectangle{x, y, w, rowH})) displayChanged = true;
            y += rowH + U.u(10.f);
            U.choice("s_fps", "Frame rate limit", &settings.fpsCap, {"30", "60", "120", "144", "Off"}, Rectangle{x, y, w, rowH});
            y += rowH + U.u(10.f);
            char b[16]; std::snprintf(b, sizeof b, "%d%%", (int)std::lround(settings.renderScale * 100.f));
            U.slider("s_scale", "Render resolution", &settings.renderScale, 0.5f, 1.f, Rectangle{x, y, w, U.u(52.f)}, false, b);
            y += rowH + U.u(10.f);
            U.paragraph(ui::Face::Sans, U.u(15.f), "Lower the render resolution or the frame rate limit on laptops: the picture stays smooth and the fans stay quiet.",
                        x, y + U.u(10.f), w, U.th.inkFaint);
            break;
        }
        case 1: {
            if (U.choice("s_q", "Quality", &settings.quality, {"Low", "Medium", "High"}, Rectangle{x, y, w, rowH}))
                applyQuality(renderer.settings, (Quality)settings.quality);
            y += rowH + U.u(10.f);
            U.toggle("s_bloom", "Bloom", &settings.bloom, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_ssao", "Ambient occlusion", &settings.ssao, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_grain", "Film grain", &settings.filmGrain, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            if (U.toggle("s_hair", "Hair physics", &settings.hairPhysics, Rectangle{x, y, w, rowH})) {
                scene.vivian.proc.hair = scene.marcus.proc.hair = settings.hairPhysics;
            }
            y += rowH + U.u(10.f);
            const char* qd[3] = {"Low: 1K shadows and textures, no ambient occlusion, 85% resolution. For integrated graphics.",
                                 "Medium: 2K shadows and textures, lighter ambient occlusion.",
                                 "High: 4K shadows and skin, full ambient occlusion, soft hair edges."};
            float used = U.paragraph(ui::Face::Sans, U.u(15.f), qd[settings.quality], x, y + U.u(6.f), w, U.th.inkFaint);
            if (settings.quality != before.quality || textureMaxSize() != (settings.quality == 0 ? 1024 : (settings.quality == 1 ? 2048 : (1 << 14))))
                U.text(ui::Face::SansMedium, U.u(15.f), "Texture detail changes apply the next time you start the game.", x, y + U.u(14.f) + used, U.th.accent);
            break;
        }
        case 2: {
            U.slider("s_master", "Master volume", &settings.master, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_voice", "Her voice", &settings.voice, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_fx", "Effects", &settings.effects, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_amb", "Room ambience", &settings.ambience, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_mus", "Music", &settings.music, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            audio.setMasterVolume(settings.master);
            break;
        }
        case 3: {
            char b[16]; std::snprintf(b, sizeof b, "%.1f\xc3\x97", (double)settings.mouseSens);
            U.slider("s_sens", "Mouse sensitivity", &settings.mouseSens, 0.2f, 3.f, Rectangle{x, y, w, U.u(52.f)}, false, b);
            y += rowH + U.u(16.f);
            U.toggle("s_inv", "Invert vertical drag", &settings.invertY, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            int mode = settings.holdRhythm ? 0 : 1;
            if (U.choice("s_rh", "Rhythm key (Space)", &mode, {"Hold", "Toggle"}, Rectangle{x, y, w, rowH})) settings.holdRhythm = mode == 0;
            y += rowH + U.u(24.f);
            U.paragraph(ui::Face::Sans, U.u(15.f),
                        "Drag up and down to move, or scroll in strokes. Hold Space for an automatic rhythm: W/S set its tempo, A/D its depth. "
                        "Right-drag turns the camera, C changes the view, V is his point of view. Tab opens the positions.",
                        x, y, w, U.th.inkFaint);
            break;
        }
        case 4: {
            U.choice("s_hints", "Her hints", &settings.hints, {"Off", "Subtle", "Clear"}, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_cine", "Cinematic camera", &settings.cinematicCamera, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_unf", "Pause when the window loses focus", &settings.pauseWhenUnfocused, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_upd", "Check for updates on start", &settings.checkUpdates, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            char b[128];
            const char* st = "";
            if (updater) {
                switch (updater->state()) {
                    case Updater::State::Checking: st = "checking\xe2\x80\xa6"; break;
                    case Updater::State::UpToDate: st = "you're up to date"; break;
                    case Updater::State::Available: case Updater::State::Downloading: case Updater::State::Ready: st = "an update is available"; break;
                    case Updater::State::Failed: st = "couldn't reach the update server"; break;
                    default: break;
                }
            }
            std::snprintf(b, sizeof b, "Version %s%s%s", Updater::current().c_str(), st[0] ? "  \xc2\xb7  " : "", st);
            U.text(ui::Face::Sans, U.u(16.f), b, x, y + U.u(18.f), U.th.inkFaint);
            if (updater && U.button("s_check", "Check now", Rectangle{x + w - U.u(170.f), y + U.u(6.f), U.u(170.f), U.u(44.f)})) updater->checkAsync();
            break;
        }
    }
    if (settings.vsync != before.vsync || settings.fpsCap != before.fpsCap || settings.windowMode != before.windowMode) displayChanged = true;
    if (displayChanged && applyDisplay) applyDisplay(settings, settings.windowMode != before.windowMode);
    float by = py + ph - U.u(86.f);
    if (U.button("set#back", "Done", Rectangle{x + w - U.u(200.f), by, U.u(200.f), U.u(50.f)}, true) || (U.in.back && !switching_)) {
        saveAll();
        go(settingsReturn_, 0.25f);
    }
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ pause
void Game::drawPause() {
    ui::Ui& U = ui;
    float ea = ui::easeOut(screenTime_ / 0.35f);
    U.backdrop(ea);
    float pw = U.u(560.f), ph = U.u(600.f);
    float px = U.W * 0.5f - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(20.f);
    U.alpha = ea;
    U.panel(Rectangle{px, py, pw, ph}, 28.f, 0.55f);
    U.text(ui::Face::Serif, U.u(64.f), "Paused", U.W * 0.5f, py + U.u(44.f), U.th.ink, ui::Center);
    float y = py + U.u(150.f), x = px + U.u(70.f);
    if (U.menuItem("p_res", "Resume", x, y, pw - U.u(140.f)) || (U.in.back && !switching_ && screenTime_ > 0.2f)) go(Screen::Play, 0.15f);
    y += U.u(70.f);
    if (U.menuItem("p_pos", "Positions", x, y, pw - U.u(140.f))) { barOpen_ = true; go(Screen::Play, 0.15f); }
    y += U.u(70.f);
    if (U.menuItem("p_set", "Settings", x, y, pw - U.u(140.f))) { settingsReturn_ = Screen::Pause; go(Screen::Settings, 0.2f); }
    y += U.u(70.f);
    if (U.menuItem("p_end", "End the night", x, y, pw - U.u(140.f))) { endNight(false); go(Screen::Results, 0.6f); }
    y += U.u(70.f);
    if (U.menuItem("p_menu", "Main menu", x, y, pw - U.u(140.f))) {
        profile.night = night_.snapshot();
        if (!harness) profile.save(profilePath_);
        go(Screen::Menu, 0.5f);
    }
    char b[96];
    int mins = (int)(night_.elapsed / 60.f), secs = (int)night_.elapsed % 60;
    std::snprintf(b, sizeof b, "%d:%02d together  \xc2\xb7  connection %d%%", mins, secs, (int)std::lround(night_.connection * 100.f));
    U.text(ui::Face::Sans, U.u(15.f), b, U.W * 0.5f, py + ph - U.u(52.f), U.th.inkFaint, ui::Center);
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ results
void Game::drawResults() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.9f);
    U.backdrop(ea * 0.85f);
    float pw = U.u(900.f), ph = U.u(720.f);
    float px = U.W * 0.5f - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(30.f);
    U.alpha = ea;
    U.panel(Rectangle{px, py, pw, ph}, 30.f, 0.55f);
    float cx = U.W * 0.5f;
    U.text(ui::Face::SansBold, U.u(14.f), "THE NIGHT", cx, py + U.u(48.f), U.th.accent, ui::Center, 0.32f);
    U.glow(cx, py + U.u(140.f), U.u(220.f), ui::withA(U.th.rose, 0.10f));
    U.text(ui::Face::Serif, U.u(92.f), results_.rating, cx, py + U.u(82.f), U.th.ink, ui::Center);
    U.paragraph(ui::Face::SerifItalic, U.u(27.f), results_.line, px + U.u(90.f), py + U.u(204.f), pw - U.u(180.f), U.th.inkDim, 1.3f, ui::Center);
    // stats grid
    struct Cell { std::string v, k; };
    char b0[32], b1[32], b2[32], b3[32];
    std::snprintf(b0, sizeof b0, "%d:%02d", (int)results_.minutes, (int)(results_.minutes * 60.f) % 60);
    std::snprintf(b1, sizeof b1, "%d", results_.herClimaxes);
    std::snprintf(b2, sizeof b2, "%d%%", (int)std::lround(results_.connection * 100.f));
    std::snprintf(b3, sizeof b3, "%d / %d", results_.desiresMet, results_.desiresMet + results_.desiresMissed);
    Cell cells[4] = {{b0, "TOGETHER"}, {b1, results_.herClimaxes == 1 ? "CLIMAX" : "CLIMAXES"}, {b2, "CONNECTION"}, {b3, "DESIRES MET"}};
    float gx = px + U.u(70.f), gw = (pw - U.u(140.f)) / 4.f, gy = py + U.u(330.f);
    for (int i = 0; i < 4; ++i) {
        float a = ui::easeOut((t - 0.4f - 0.1f * (float)i) / 0.6f);
        U.alpha = ea * a;
        float ccx = gx + gw * ((float)i + 0.5f);
        U.text(ui::Face::Serif, U.u(54.f), cells[i].v, ccx, gy + (1.f - a) * U.u(10.f), U.th.ink, ui::Center);
        U.text(ui::Face::SansBold, U.u(12.f), cells[i].k, ccx, gy + U.u(70.f), U.th.inkFaint, ui::Center, 0.22f);
        if (i > 0) U.fillRect(Rectangle{gx + gw * (float)i, gy + U.u(8.f), std::max(1.f, U.u(1.f)), U.u(80.f)}, U.th.hairline);
    }
    U.alpha = ea * ui::easeOut((t - 0.9f) / 0.6f);
    char sb[64];
    int shown = (int)std::lround((float)results_.score * clamp01((t - 0.9f) / 1.2f));
    std::snprintf(sb, sizeof sb, "%d", shown);
    U.text(ui::Face::SerifBold, U.u(44.f), sb, cx, py + U.u(448.f), U.th.accent, ui::Center);
    bool best = results_.score >= profile.stats.bestScore && results_.score > 0;
    U.text(ui::Face::SansBold, U.u(12.f), best ? "SCORE  \xc2\xb7  NEW BEST" : "SCORE", cx, py + U.u(502.f), best ? U.th.rose : U.th.inkFaint, ui::Center, 0.22f);
    if (results_.together) { U.heart(cx - U.u(118.f), py + U.u(472.f), U.u(20.f), U.th.rose); U.heart(cx + U.u(118.f), py + U.u(472.f), U.u(20.f), U.th.rose); }
    U.alpha = ea * ui::easeOut((t - 1.2f) / 0.5f);
    float by = py + ph - U.u(104.f);
    if (U.button("r_again", "Another night", Rectangle{cx - U.u(250.f), by, U.u(240.f), U.u(54.f)}, true)) { startNight(false); go(Screen::Play, 0.7f); }
    if (U.button("r_menu", "Main menu", Rectangle{cx + U.u(10.f), by, U.u(240.f), U.u(54.f)}) || (U.in.back && !switching_ && t > 1.f)) {
        go(Screen::Menu, 0.6f);
    }
    U.alpha = 1.f;
}

}  // namespace ag
