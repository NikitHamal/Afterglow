#include "game/game.h"
#include "app/updater.h"
#include "app/platform.h"
#include "gfx/texture.h"
#include "ui/layout.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
const char* kToneLabel[8] = {"Porcelain", "Fair", "Honey", "Olive", "Caramel", "Bronze", "Deep", "Ebony"};
const Color kToneSwatch[8] = {{246, 214, 196, 255}, {238, 200, 172, 255}, {214, 160, 120, 255}, {192, 152, 106, 255},
                              {170, 108, 72, 255}, {150, 96, 62, 255}, {104, 62, 44, 255}, {66, 40, 30, 255}};
const Color kHairSwatch[6] = {{52, 32, 24, 255}, {104, 56, 34, 255}, {132, 52, 28, 255}, {150, 44, 30, 255}, {24, 20, 22, 255}, {196, 140, 82, 255}};
const char* kHairLabel[6] = {"Espresso", "Chestnut", "Copper", "Auburn", "Raven", "Honey"};

// a column of sections and rows inside a panel: the cursor moves down as rows are taken
struct Column {
    ui::Ui& U;
    float x, w, y;
    void section(const char* t, float after = 30.f) { U.sectionLabel(t, x, y, w); y += U.u(after); }
    void gap(float g) { y += U.u(g); }
};

// one chip per name, sized to the names and flowed over the column: returns the clicked index or -1. A row of
// choices (body types, builds, and later the model) is one call.
int chipRow(ui::Ui& U, Column& c, const std::string& idPrefix, const std::vector<std::string>& names, const std::vector<bool>& active) {
    float bottom = c.y;
    std::vector<Rectangle> rects = ui::chipFlow(U, names, c.x, c.y, c.w, &bottom);
    int hit = -1;
    for (size_t i = 0; i < names.size(); ++i)
        if (U.chip(idPrefix + names[i], names[i], rects[i], i < active.size() && active[i])) hit = (int)i;
    c.y = bottom;
    return hit;
}
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
    U.glow(U.W * 0.5f, U.H - U.u(110.f), U.u(90.f), ui::withA(U.th.accent, 0.08f));
    U.text(ui::Face::SansBold, U.u(19.f), "PRESS ANY KEY", U.W * 0.5f, U.H - U.u(122.f), U.th.ink, ui::Center, 0.32f);
    U.alpha = ui::easeOut((t - 1.6f) / 1.0f) * 0.8f;
    U.text(ui::Face::Sans, U.u(14.f), "For adults only. A work of fiction between two consenting adults.", U.W * 0.5f, U.H - U.u(56.f),
           U.th.inkFaint, ui::Center);
    U.alpha = 1.f;
    int key = GetKeyPressed();
    bool anyKey = key != 0 && key != KEY_F1 && key != KEY_F5 && key != KEY_F11 && key != KEY_LEFT_ALT && key != KEY_RIGHT_ALT;
    if (t > 0.6f && !switching_ && (anyKey || U.in.clicked)) go(Screen::Menu, 0.45f);
}

// ------------------------------------------------------------------ main menu
void Game::drawMenu() {
    ui::Ui& U = ui;
    float t = screenTime_;
    ui::Box shade; shade.fill = Color{8, 4, 6, 215}; shade.fill2 = Color{8, 4, 6, 0}; shade.grad = 1;
    U.box(Rectangle{0.f, 0.f, U.W * 0.55f, U.H}, shade);
    ui::ScaleScope screenFit(U, U.fitScale(1920.f, 1080.f, 0.f));
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
    items.push_back({"m_story", "Stories"});
    items.push_back({"m_new", canContinue ? "A new night" : "Free play"});
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
            else if (id == "m_story") go(Screen::Stories, 0.4f);
            else if (id == "m_cust") go(Screen::Customize, 0.4f);
            else if (id == "m_set") { settingsReturn_ = Screen::Menu; go(Screen::Settings, 0.3f); }
            else if (id == "m_quit") { saveAll(); quit_ = true; }
        }
        y += U.u(74.f);
    }
    U.alpha = ea;
    if (confirmNew_) {
        U.textIn(ui::Face::SerifItalic, U.u(22.f), "Start over? Your current night will be lost \xe2\x80\x94 choose again to confirm.",
                 Rectangle{x, y + U.u(10.f), U.W - x - U.u(40.f), U.u(22.f)}, U.th.rose);
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
    ui::ClipScope clip(U, Rectangle{x, y, w, h}, "update card");
    U.text(ui::Face::SansBold, U.u(13.f), "UPDATE AVAILABLE", x + U.u(26.f), y + U.u(22.f), U.th.accent, ui::Left, 0.22f);
    char b[96];
    std::snprintf(b, sizeof b, "Afterglow %s", updater->latest().c_str());
    U.textIn(ui::Face::Serif, U.u(34.f), b, Rectangle{x + U.u(26.f), y + U.u(44.f), w - U.u(52.f), U.u(34.f)}, U.th.ink);
    std::string notes = updater->notes();
    if (s == Updater::State::Downloading) {
        float p = updater->progress();
        std::snprintf(b, sizeof b, "Downloading\xe2\x80\xa6 %d%%", (int)std::lround(p * 100.f));
        U.text(ui::Face::Sans, U.u(16.f), b, x + U.u(26.f), y + U.u(92.f), U.th.inkDim);
        U.fillRect(Rectangle{x + U.u(26.f), y + U.u(126.f), w - U.u(52.f), U.u(5.f)}, Color{255, 236, 224, 34}, U.u(2.5f));
        ui::Box pb; pb.radius = U.u(2.5f); pb.fill = U.th.accent; pb.fill2 = U.th.rose; pb.grad = 1;
        U.box(Rectangle{x + U.u(26.f), y + U.u(126.f), std::max(U.u(5.f), (w - U.u(52.f)) * p), U.u(5.f)}, pb);
    } else {
        std::string line = s == Updater::State::Failed ? updater->error() : (notes.empty() ? "A fresh build is ready." : notes);
        U.textIn(ui::Face::Sans, U.u(15.f), line, Rectangle{x + U.u(26.f), y + U.u(90.f), w - U.u(52.f), U.u(15.f)}, s == Updater::State::Failed ? U.th.rose : U.th.inkDim);
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
    float pw = U.u(700.f), ph = U.H - U.u(112.f);
    float px = U.W - U.u(56.f) - pw, py = U.u(56.f);
    U.alpha = ea;
    Rectangle panel{px + (1.f - ea) * U.u(40.f), py, pw, ph};
    U.panel(panel, 28.f, 0.6f);
    bool changed = false;
    {
        ui::ClipScope clip(U, panel, "customize");
        // the content shrinks as a whole when it would not fit above the footer (small windows, more rows)
        ui::ScaleScope fit(U, U.fitGet("customize"));
        float x = panel.x + U.u(48.f), w = pw - 2.f * U.u(48.f);
        Column col{U, x, w, py + U.u(40.f)};
        U.text(ui::Face::Serif, U.u(56.f), "Customize", x, col.y, U.th.ink);
        col.gap(78.f);
        int before = custTab_;
        U.tabs("cust#tabs", &custTab_, {"VIVIAN", "MARCUS"}, Rectangle{x, col.y, w, U.u(44.f)});
        if (custTab_ != before) { showcaseWho_ = custTab_; stage(Stage::Showcase); }
        col.gap(64.f);
        const float top = col.y;
        std::vector<Color> toneCols(kToneSwatch, kToneSwatch + 8);
        std::vector<std::string> toneNames(kToneLabel, kToneLabel + 8);
        char agebuf[24];
        float colW = (w - U.u(48.f)) * 0.5f, col2 = x + colW + U.u(48.f);
        // age shapes ship with a later character update; until then the slider says so instead of doing nothing silently
        auto soonTag = [&](const Character& c, float lx, float ly) {
            if (!c.loaded() || c.hasMorph("age_up") || c.hasMorph("age_dn")) return;
            float tx = lx + U.measure(ui::Face::SansMedium, U.u(19.f), "Age").x + U.u(10.f);
            float tp = U.u(11.f);
            Vector2 m = U.measure(ui::Face::SansBold, tp, "COMING SOON", 0.12f);
            U.fillRect(Rectangle{tx, ly + U.u(2.f), m.x + U.u(14.f), U.u(19.f)}, Color{255, 236, 224, 26}, U.u(9.5f));
            U.text(ui::Face::SansBold, tp, "COMING SOON", tx + U.u(7.f), ly + U.u(5.5f), U.th.inkFaint, ui::Left, 0.12f);
        };
        if (custTab_ == 0) {
            HerLook& h = profile.her;
            col.section("BODY TYPE");
            // body types: the content's presets (characters/presets.json), with their skin tone and age
            const Json& pr = clipsPresets_["vivian"]["presets"];
            static const char* kSl[8] = {"bust", "hips", "waist", "butt", "weight", "muscle", "belly", "age"};
            std::vector<std::string> names;
            std::vector<bool> act;
            for (auto& kv : pr.obj) {
                const Json& sl = kv.second["sliders"];
                float cur[7] = {h.bust, h.hips, h.waist, h.butt, h.weight, h.muscle, h.belly};
                float d = 0.f;
                for (int k = 0; k < 7; ++k) d += std::fabs(cur[k] - sl[kSl[k]].f(0.f));
                int toneWant = skinToneIndex(kv.second["tone"].str());
                names.push_back(kv.first);
                act.push_back(d < 0.05f && (toneWant < 0 || toneWant == h.tone));
            }
            int pick = chipRow(U, col, "hp#", names, act);
            if (pick >= 0) {
                const Json& preset = pr[names[(size_t)pick]];
                const Json& sl = preset["sliders"];
                h.bust = sl["bust"].f(0.f); h.hips = sl["hips"].f(0.f); h.waist = sl["waist"].f(0.f); h.butt = sl["butt"].f(0.f);
                h.weight = sl["weight"].f(0.f); h.muscle = sl["muscle"].f(0.f); h.belly = std::max(0.f, sl["belly"].f(0.f));
                int toneWant = skinToneIndex(preset["tone"].str());
                if (toneWant >= 0) h.tone = toneWant;
                if (sl["age"].isNumber()) { float a = sl["age"].f(); h.age = a < 0.f ? 42.f + a * 17.f : 42.f + a * 18.f; }
                changed = true;
            }
            col.gap(14.f);
            col.section("FIGURE");
            struct S { const char* id; const char* label; float* v; float lo; };
            const S sl[] = {{"h_bust", "Bust", &h.bust, -1.f}, {"h_hips", "Hips", &h.hips, -1.f}, {"h_waist", "Waist", &h.waist, -1.f},
                            {"h_butt", "Bottom", &h.butt, -1.f}, {"h_weight", "Weight", &h.weight, -1.f}, {"h_muscle", "Tone", &h.muscle, -1.f},
                            {"h_belly", "Belly", &h.belly, 0.f}};
            for (int i = 0; i < 7; ++i) {
                float sx = i % 2 ? col2 : x, sy = col.y + (float)(i / 2) * U.u(52.f);
                changed |= U.slider(sl[i].id, sl[i].label, sl[i].v, sl[i].lo, 1.f, Rectangle{sx, sy, colW, U.u(46.f)}, sl[i].lo < 0.f);
            }
            // adults only: 21 to 60 years
            std::snprintf(agebuf, sizeof agebuf, "%d years", (int)std::lround(h.age));
            changed |= U.slider("h_age", "Age", &h.age, kMinAge, kMaxAge, Rectangle{col2, col.y + 3.f * U.u(52.f), colW, U.u(46.f)}, false, agebuf);
            soonTag(scene.vivian, col2, col.y + 3.f * U.u(52.f));
            col.gap(4.f * 52.f + 6.f);
            col.section("SKIN & HAIR");
            changed |= U.swatches("h_tone", "Skin tone", &h.tone, toneCols, toneNames, Rectangle{x, col.y, w, U.u(72.f)});
            col.gap(80.f);
            changed |= U.swatches("h_hair", "Hair colour", &h.hair, std::vector<Color>(kHairSwatch, kHairSwatch + 6),
                                  std::vector<std::string>(kHairLabel, kHairLabel + 6), Rectangle{x, col.y, colW, U.u(72.f)});
            U.textIn(ui::Face::SansMedium, U.u(19.f), "Hair style", Rectangle{col2, col.y, colW, U.u(19.f)}, U.th.inkDim);
            U.chip("h_style", "Low chignon", Rectangle{col2, col.y + U.u(36.f), std::min(colW, U.u(170.f)), U.u(36.f)}, true);
            col.gap(80.f);
        } else {
            HisLook& h = profile.him;
            col.section("BUILD");
            // builds: the content's presets (characters/presets.json)
            const Json& pr = clipsPresets_["marcus"]["presets"];
            std::vector<std::string> names;
            std::vector<bool> act;
            for (auto& kv : pr.obj) {
                const Json& sl = kv.second["sliders"];
                float mu = sl["muscle"].f(0.f), we = sl["weight"].f(0.f), be = std::max(0.f, sl["belly"].f(0.f));
                names.push_back(kv.first);
                act.push_back(std::fabs(h.muscle - mu) + std::fabs(h.weight - we) + std::fabs(h.belly - be) < 0.05f);
            }
            int pick = chipRow(U, col, "mp#", names, act);
            if (pick >= 0) {
                const Json& sl = pr[names[(size_t)pick]]["sliders"];
                h.muscle = sl["muscle"].f(0.f); h.weight = sl["weight"].f(0.f); h.belly = std::max(0.f, sl["belly"].f(0.f));
                if (sl["length"].isNumber()) h.length = sl["length"].f();
                if (sl["girth"].isNumber()) h.girth = sl["girth"].f();
                changed = true;
            }
            col.gap(20.f);
            changed |= U.slider("m_muscle", "Muscle", &h.muscle, -1.f, 1.f, Rectangle{x, col.y, colW, U.u(46.f)}, true);
            changed |= U.slider("m_weight", "Weight", &h.weight, -1.f, 1.f, Rectangle{col2, col.y, colW, U.u(46.f)}, true);
            changed |= U.slider("m_belly", "Belly", &h.belly, 0.f, 1.f, Rectangle{x, col.y + U.u(64.f), colW, U.u(46.f)});
            std::snprintf(agebuf, sizeof agebuf, "%d years", (int)std::lround(h.age));
            changed |= U.slider("m_age", "Age", &h.age, kMinAge, kMaxAge, Rectangle{col2, col.y + U.u(64.f), colW, U.u(46.f)}, false, agebuf);
            soonTag(scene.marcus, col2, col.y + U.u(64.f));
            col.gap(140.f);
            col.section("ENDOWMENT", 32.f);
            changed |= U.slider("m_length", "Length", &h.length, -1.f, 1.f, Rectangle{x, col.y, colW, U.u(46.f)}, true);
            changed |= U.slider("m_girth", "Girth", &h.girth, -1.f, 1.f, Rectangle{col2, col.y, colW, U.u(46.f)}, true);
            col.gap(80.f);
            col.section("SKIN");
            changed |= U.swatches("m_tone", "Skin tone", &h.tone, toneCols, toneNames, Rectangle{x, col.y, w, U.u(76.f)});
            col.gap(80.f);
        }
        // footer
        float by = py + ph - U.u(86.f);
        U.fitReport("customize", top, col.y + U.u(8.f), by);
        if (U.button("cust#reset", "Reset", Rectangle{x, by, U.u(150.f), U.u(50.f)})) {
            if (custTab_ == 0) { int tone = profile.her.tone, hair = profile.her.hair; float age = profile.her.age; profile.her = HerLook{}; profile.her.tone = tone; profile.her.hair = hair; profile.her.age = age; }
            else { int tone = profile.him.tone; float age = profile.him.age; profile.him = HisLook{}; profile.him.tone = tone; profile.him.age = age; }
            applyLooks();
        }
        if (U.button("cust#done", "Done", Rectangle{x + w - U.u(200.f), by, U.u(200.f), U.u(50.f)}, true) || (U.in.back && !switching_)) {
            if (!harness) profile.save(profilePath_);
            go(Screen::Menu, 0.4f);
        }
    }
    if (changed) applyLooks();
    if (custTab_ == 1 && scene.marcus.loaded()) {
        // the preview shows his endowment at rest; the sliders shape it when aroused
        float k = 0.35f;
        scene.marcus.setBody("length", profile.him.length * k);
        scene.marcus.setBody("girth", profile.him.girth * k);
    }
    // the preview's name and caption, in the room left of the panel
    U.alpha = ea * 0.85f;
    float left = std::max(U.u(40.f), panel.x - U.u(40.f));
    U.textIn(ui::Face::Sans, U.u(15.f), "Drag to turn  \xc2\xb7  Scroll to look closer", Rectangle{0.f, U.H - U.u(56.f), left, U.u(20.f)},
             U.th.inkFaint, ui::Center);
    U.textIn(ui::Face::Serif, U.u(46.f), custTab_ == 0 ? "Vivian" : "Marcus", Rectangle{U.u(80.f), U.u(64.f), left - U.u(80.f), U.u(46.f)}, U.th.ink);
    U.textIn(ui::Face::SerifItalic, U.u(22.f), custTab_ == 0 ? "forty-two, warm, unhurried" : "forty, steady, attentive",
             Rectangle{U.u(82.f), U.u(122.f), left - U.u(82.f), U.u(22.f)}, U.th.inkDim);
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ settings
void Game::drawSettings() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.5f);
    const bool lighting = setTab_ == 2;
    // lighting previews live: the panel moves aside and the scene stays clear (even from pause)
    float side = U.anim("set#side", lighting ? 1.f : 0.f, 9.f);
    if (settingsReturn_ == Screen::Pause) U.backdrop(1.f - side);
    ui::ScaleScope screenFit(U, U.fitScale(1060.f, 800.f, 24.f));   // tiny windows: the whole panel shrinks
    float pw = U.u(1060.f - 300.f * side), ph = U.u(800.f);
    float cx = U.W * 0.5f + side * (U.W * 0.5f - U.u(56.f) - pw * 0.5f);
    float px = cx - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(24.f);
    U.alpha = ea;
    Rectangle panel{px, py, pw, ph};
    U.panel(panel, 28.f, 0.62f);
    ui::ClipScope clip(U, panel, "settings");
    float x = px + U.u(56.f), w = pw - U.u(112.f), y = py + U.u(44.f);
    U.text(ui::Face::Serif, U.u(56.f), "Settings", x, y, U.th.ink);
    y += U.u(88.f);
    U.tabs("set#tabs", &setTab_, {"DISPLAY", "GRAPHICS", "LIGHTING", "AUDIO", "CONTROLS", "GAME"}, Rectangle{x, y, w, U.u(44.f)});
    y += U.u(78.f);
    // a tab's rows shrink together when they would reach the footer
    const std::string fitId = "settings#" + std::to_string(setTab_);
    const float top = y;
    ui::ScaleScope fit(U, U.fitGet(fitId));
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
            y += U.u(10.f) + U.paragraph(ui::Face::Sans, U.u(15.f), "Lower the render resolution or the frame rate limit on laptops: the picture stays smooth and the fans stay quiet.",
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
            U.toggle("s_dof", "Depth of field in story close-ups", &settings.depthOfField, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            if (U.toggle("s_hair", "Hair physics", &settings.hairPhysics, Rectangle{x, y, w, rowH})) {
                scene.vivian.proc.hair = scene.marcus.proc.hair = settings.hairPhysics;
            }
            y += rowH + U.u(10.f);
            const char* qd[3] = {"Low: 1K shadows and textures, no ambient occlusion, 85% resolution. For integrated graphics.",
                                 "Medium: 2K shadows and textures, lighter ambient occlusion.",
                                 "High: 4K shadows and skin, full ambient occlusion, soft hair edges."};
            float used = U.paragraph(ui::Face::Sans, U.u(15.f), qd[settings.quality], x, y + U.u(6.f), w, U.th.inkFaint);
            y += U.u(6.f) + used;
            if (settings.quality != before.quality || textureMaxSize() != (settings.quality == 0 ? 1024 : (settings.quality == 1 ? 2048 : (1 << 14)))) {
                U.textIn(ui::Face::SansMedium, U.u(15.f), "Texture detail changes apply the next time you start the game.", Rectangle{x, y + U.u(8.f), w, U.u(15.f)}, U.th.accent);
                y += U.u(23.f);
            }
            break;
        }
        case 2: {
            U.sectionLabel("MOOD", x, y, w);
            y += U.u(34.f);
            struct M { const char* name; const char* line; };
            const M moods[3] = {{"Warm evening", "Lamp-lit and natural"}, {"Candlelit night", "Warmer, deeper shadows"}, {"Bright daylight", "Soft window light"}};
            float cw = (w - U.u(2.f * 14.f)) / 3.f, ch = U.u(96.f);
            for (int i = 0; i < 3; ++i) {
                Rectangle r{x + (float)i * (cw + U.u(14.f)), y, cw, ch};
                bool act = settings.lightMood == i;
                // the panel is see-through here (live preview): a dark base keeps the cards readable over a bright room
                U.fillRect(r, Color{14, 8, 11, 150}, r.height * 0.5f);
                if (U.chip(std::string("lm#") + std::to_string(i), "", r, act)) settings.lightMood = i;
                U.text(ui::Face::Serif, U.u(25.f), moods[i].name, r.x + r.width * 0.5f, r.y + U.u(18.f), act ? U.th.ink : U.th.inkDim, ui::Center);
                U.text(ui::Face::Sans, U.u(15.f), moods[i].line, r.x + r.width * 0.5f, r.y + U.u(58.f), act ? U.th.inkDim : U.th.inkFaint, ui::Center);
            }
            y += ch + U.u(36.f);
            char b[32];
            std::snprintf(b, sizeof b, "%+.1f EV", (double)(settings.brightness * 0.8f));
            U.slider("s_bright", "Brightness", &settings.brightness, -1.f, 1.f, Rectangle{x, y, w, U.u(52.f)}, true, b);
            y += rowH + U.u(16.f);
            const char* wt = settings.warmth < -0.15f ? "cooler" : (settings.warmth > 0.15f ? "warmer" : "neutral");
            U.slider("s_warm", "Warmth", &settings.warmth, -1.f, 1.f, Rectangle{x, y, w, U.u(52.f)}, true, wt);
            y += rowH + U.u(20.f);
            if (U.button("s_lreset", "Reset lighting", Rectangle{x, y, U.u(220.f), U.u(46.f)})) { settings.lightMood = 0; settings.brightness = settings.warmth = 0.f; }
            U.textIn(ui::Face::Sans, U.u(15.f), "M cycles the mood during the night.", Rectangle{x + U.u(240.f), y, w - U.u(240.f), U.u(46.f)}, U.th.inkFaint);
            y += U.u(46.f);
            break;
        }
        case 3: {
            U.slider("s_master", "Master volume", &settings.master, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_voice", "Voices", &settings.voice, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_fx", "Effects", &settings.effects, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_amb", "Room ambience", &settings.ambience, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            U.slider("s_mus", "Music", &settings.music, 0.f, 1.f, Rectangle{x, y, w, U.u(52.f)}); y += rowH + U.u(16.f);
            audio.setMasterVolume(settings.master);
            break;
        }
        case 4: {
            char b[16]; std::snprintf(b, sizeof b, "%.1f\xc3\x97", (double)settings.mouseSens);
            U.slider("s_sens", "Mouse sensitivity", &settings.mouseSens, 0.2f, 3.f, Rectangle{x, y, w, U.u(52.f)}, false, b);
            y += rowH + U.u(16.f);
            U.toggle("s_inv", "Invert vertical drag", &settings.invertY, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            int mode = settings.holdRhythm ? 0 : 1;
            if (U.choice("s_rh", "Rhythm key (Space)", &mode, {"Hold", "Toggle"}, Rectangle{x, y, w, rowH})) settings.holdRhythm = mode == 0;
            y += rowH + U.u(24.f);
            y += U.paragraph(ui::Face::Sans, U.u(15.f),
                             "Drag up and down to move, or scroll in strokes. Hold Space for an automatic rhythm: W/S set its tempo, A/D its depth. "
                             "Right-drag turns the camera, C changes the view, V is his point of view. Tab opens the positions.",
                             x, y, w, U.th.inkFaint);
            break;
        }
        case 5: {
            U.choice("s_hints", "Her hints", &settings.hints, {"Off", "Subtle", "Clear"}, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_cine", "Cinematic camera", &settings.cinematicCamera, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
            U.toggle("s_sauto", "Stories play on after each voiced line", &settings.storyAuto, Rectangle{x, y, w, rowH}); y += rowH + U.u(10.f);
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
            U.textIn(ui::Face::Sans, U.u(16.f), b, Rectangle{x, y + U.u(6.f), w - (updater ? U.u(190.f) : 0.f), U.u(44.f)}, U.th.inkFaint);
            if (updater && U.button("s_check", "Check now", Rectangle{x + w - U.u(170.f), y + U.u(6.f), U.u(170.f), U.u(44.f)})) updater->checkAsync();
            y += U.u(84.f);
            y += U.paragraph(ui::Face::Sans, U.u(14.f),
                        "Sound: \"Lewd Sound Pack\" by Shinlalala (CC BY-SA 4.0). Fonts: Cormorant Garamond and Inter (SIL OFL 1.1). "
                        "Engine: raylib, cgltf, stb. Characters made with MPFB / MakeHuman (CC0). Adults only; all characters are consenting adults.",
                        x, y, w, U.th.inkFaint);
            break;
        }
    }
    if (settings.vsync != before.vsync || settings.fpsCap != before.fpsCap || settings.windowMode != before.windowMode) displayChanged = true;
    if (displayChanged && applyDisplay) applyDisplay(settings, settings.windowMode != before.windowMode);
    float by = py + ph - U.u(86.f);
    U.fitReport(fitId, top, y + U.u(8.f), by);
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
    ui::ScaleScope screenFit(U, U.fitScale(560.f, 600.f, 24.f));
    float pw = U.u(560.f), ph = U.u(600.f);
    float px = U.W * 0.5f - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(20.f);
    U.alpha = ea;
    U.panel(Rectangle{px, py, pw, ph}, 28.f, 0.55f);
    ui::ClipScope clip(U, Rectangle{px, py, pw, ph}, "pause");
    U.text(ui::Face::Serif, U.u(64.f), "Paused", U.W * 0.5f, py + U.u(44.f), U.th.ink, ui::Center);
    float y = py + U.u(150.f), x = px + U.u(70.f);
    if (U.menuItem("p_res", "Resume", x, y, pw - U.u(140.f)) || (U.in.back && !switching_ && screenTime_ > 0.2f)) go(pauseReturn_, 0.15f);
    y += U.u(70.f);
    if (U.menuItem("p_pos", "Acts", x, y, pw - U.u(140.f))) { barOpen_ = true; go(pauseReturn_, 0.15f); }
    y += U.u(70.f);
    if (U.menuItem("p_set", "Settings", x, y, pw - U.u(140.f))) { settingsReturn_ = Screen::Pause; go(Screen::Settings, 0.2f); }
    y += U.u(70.f);
    if (U.menuItem("p_end", pauseReturn_ == Screen::Story ? "Skip ahead" : "End the night", x, y, pw - U.u(140.f))) {
        if (pauseReturn_ == Screen::Story) {
            storyPlaying_ = false;
            if (story_.node() && !story_.node()->playNext.empty()) story_.jump(story_.node()->playNext);
            go(Screen::Story, 0.4f);
        } else { endNight(false); go(Screen::Results, 0.6f); }
    }
    y += U.u(70.f);
    if (U.menuItem("p_menu", "Main menu", x, y, pw - U.u(140.f))) {
        if (pauseReturn_ == Screen::Story) { storyAutosave(); story_.stop(); storyPlaying_ = false; go(Screen::Menu, 0.5f); return; }
        profile.night = night_.snapshot();
        if (!harness) profile.save(profilePath_);
        go(Screen::Menu, 0.5f);
    }
    char b[96];
    int mins = (int)(night_.elapsed / 60.f), secs = (int)night_.elapsed % 60;
    std::snprintf(b, sizeof b, "%d:%02d together  \xc2\xb7  connection %d%%", mins, secs, (int)std::lround(night_.connection * 100.f));
    U.textIn(ui::Face::Sans, U.u(15.f), b, Rectangle{px + U.u(30.f), py + ph - U.u(52.f), pw - U.u(60.f), U.u(15.f)}, U.th.inkFaint, ui::Center);
    U.alpha = 1.f;
}

// ------------------------------------------------------------------ results
void Game::drawResults() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.9f);
    U.backdrop(ea * 0.85f);
    ui::ScaleScope screenFit(U, U.fitScale(900.f, 720.f, 24.f));
    float pw = U.u(900.f), ph = U.u(720.f);
    float px = U.W * 0.5f - pw * 0.5f, py = U.H * 0.5f - ph * 0.5f + (1.f - ea) * U.u(30.f);
    U.alpha = ea;
    U.panel(Rectangle{px, py, pw, ph}, 30.f, 0.55f);
    ui::ClipScope clip(U, Rectangle{px, py, pw, ph}, "results");
    float cx = U.W * 0.5f;
    U.text(ui::Face::SansBold, U.u(14.f), "THE NIGHT", cx, py + U.u(48.f), U.th.accent, ui::Center, 0.32f);
    U.glow(cx, py + U.u(140.f), U.u(220.f), ui::withA(U.th.rose, 0.10f));
    U.textIn(ui::Face::Serif, U.u(92.f), results_.rating, Rectangle{px + U.u(40.f), py + U.u(82.f), pw - U.u(80.f), U.u(92.f)}, U.th.ink, ui::Center);
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
        U.textIn(ui::Face::Serif, U.u(54.f), cells[i].v, Rectangle{ccx - gw * 0.5f + U.u(6.f), gy + (1.f - a) * U.u(10.f), gw - U.u(12.f), U.u(54.f)}, U.th.ink, ui::Center);
        U.textIn(ui::Face::SansBold, U.u(12.f), cells[i].k, Rectangle{ccx - gw * 0.5f + U.u(6.f), gy + U.u(70.f), gw - U.u(12.f), U.u(12.f)}, U.th.inkFaint, ui::Center, 0.22f);
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
