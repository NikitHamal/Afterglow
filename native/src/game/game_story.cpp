#include "game/game.h"
#include "core/log.h"
#include "core/paths.h"
#include "raylib.h"
#include "ui/layout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
}  // namespace

// ------------------------------------------------------------------ loading + progress
void Game::loadStories() {
    stories_.clear();
    std::string dir = paths::join(paths::assetRoot(), "story");
    for (const std::string& f : paths::listFilesRecursive(dir, {".json"})) {
        if (paths::dirname(f) != dir) continue;   // story/voice/manifest.json and friends are not stories
        Story s;
        std::string err;
        if (s.load(f, err)) { AG_INFO("AG: story '%s': %d chapters, %d nodes", s.title.c_str(), (int)s.chapters.size(), (int)s.nodes.size()); stories_.push_back(std::move(s)); }
        else AG_WARN("AG: story %s: %s", f.c_str(), err.c_str());
    }
    std::sort(stories_.begin(), stories_.end(), [](const Story& a, const Story& b) { return a.title < b.title; });
    storyProgressPath_ = paths::join(paths::userDir(), "story_progress.json");
    storySavesPath_ = paths::join(paths::userDir(), "story_saves.json");
    if (!harness) storyProgress_.load(storyProgressPath_);
    storySaves_ = harness ? std::vector<StorySave>(7) : loadStorySaves(storySavesPath_, 7);
}

const Story* Game::storyById(const std::string& id) const {
    for (const Story& s : stories_) if (s.id == id) return &s;
    return nullptr;
}

void Game::beginStory(const Story* s, const std::string& node, const std::map<std::string, float>* vars, int line) {
    if (!s) return;
    ensureCast(true);    // the stories are written for Vivian and Marcus
    stage(Stage::Night);
    enterStoryWorld();
    story_.start(s, node, vars, line);
    story_.autoMode = settings.storyAuto;
    storyPlaying_ = false;
    storyOverlay_ = 0;
    storyCardT_ = 0.f;
    storyLastChapter_.clear();
    black_ = 1.f; blackDir_ = -1;   // the film fades in
    night_.begin((unsigned)(GetTime() * 1000.0) ^ 0x51ed270bu);
    go(Screen::Story, 0.8f);
}

void Game::storyAutosave() {
    if (harness || !story_.active() || !story_.node()) return;
    StorySave& sv = storySaves_[0];
    sv.used = true; sv.story = story_.story()->id; sv.node = story_.node()->id; sv.line = 0; sv.vars = story_.vars;
    int ci = story_.story()->chapterIndex(story_.node()->chapter);
    sv.chapterTitle = ci >= 0 ? story_.story()->chapters[(size_t)ci].title : "";
    std::time_t t = std::time(nullptr);
    char b[32]; std::strftime(b, sizeof b, "%d %b %H:%M", std::localtime(&t));
    sv.when = b;
    saveStorySaves(storySavesPath_, storySaves_);
}

void Game::storyCamera(const std::string& cam, float blend) {
    if (cam == "side") camMode_ = 0;
    else if (cam == "front") camMode_ = 1;
    else if (cam == "top") camMode_ = 2;
    else if (cam == "his_view" || cam == "fpv") camMode_ = 3;
    else if (cam == "face" || cam == "her_face") camMode_ = 4;
    else if (cam == "wide") {
        camera.setOrbit(scene.info.bedCenter + V3{0.f, 0.15f, 0.2f}, 0.75f, 0.32f, 3.6f, blend);
        camUser_ = true;
        return;
    }
    cameraForPosition(blend);
    camUser_ = true;   // the story directs the camera: no cinematic cuts
}

// ------------------------------------------------------------------ drawing
void Game::drawStories() {
    ui::Ui& U = ui;
    float t = screenTime_;
    float ea = ui::easeOut(t / 0.6f);
    U.backdrop(0.65f * ea);
    U.alpha = ea;
    float x = U.u(120.f), y = U.u(70.f);
    U.text(ui::Face::Serif, U.u(64.f), "Stories", x, y, U.th.ink);
    U.text(ui::Face::SerifItalic, U.u(24.f), "slow nights, your choices", x + U.u(4.f), y + U.u(84.f), U.th.inkDim);
    y += U.u(150.f);
    if (stories_.empty()) {
        U.text(ui::Face::Sans, U.u(20.f), "No stories found in assets/story.", x, y, U.th.inkFaint);
    }
    float cw = (U.W - 2.f * x - U.u(2.f * 28.f)) / 3.f, ch = std::min(U.H - y - U.u(200.f), U.u(700.f));
    // titles and taglines take the tallest height of the three, so the chapter lists line up across the cards
    float titleH = 0.f, tagH = 0.f;
    {
        float a0 = U.alpha;
        U.alpha = 0.f;
        for (size_t i = 0; i < stories_.size() && i < 3; ++i) {
            titleH = std::max(titleH, U.paragraph(ui::Face::Serif, U.u(44.f), stories_[i].title, 0.f, 0.f, cw - U.u(68.f), U.th.ink, 1.05f));
            tagH = std::max(tagH, U.paragraph(ui::Face::SerifItalic, U.u(22.f), stories_[i].tagline, 0.f, 0.f, cw - U.u(68.f), U.th.inkDim, 1.3f));
        }
        U.alpha = a0;
    }
    for (size_t i = 0; i < stories_.size() && i < 3; ++i) {
        const Story& s = stories_[i];
        Rectangle r{x + (float)i * (cw + U.u(28.f)), y + (1.f - ui::easeOut((t - 0.1f * (float)i) / 0.6f)) * U.u(30.f), cw, ch};
        bool over = CheckCollisionPointRec(U.in.mouse, r);
        float h = U.anim("st#" + std::to_string(i), over ? 1.f : 0.f, 12.f);
        ui::Box b; b.radius = U.u(26.f); b.glass = true; b.glassTint = U.th.glass; b.glassAmt = 0.6f;
        b.fill = Color{255, 236, 224, (unsigned char)(6 + 14 * h)};
        b.border = std::max(1.f, U.u(1.2f)); b.borderCol = ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.8f), h);
        b.shadow = U.u(30.f + 20.f * h); b.shadowCol = ui::withA(h > 0.3f ? U.th.accent : Color{0, 0, 0, 255}, 0.18f + 0.1f * h);
        U.box(r, b);
        ui::ClipScope clip(U, r, "story card");
        float ix = r.x + U.u(34.f), iw = r.width - U.u(68.f), iy = r.y + U.u(34.f);
        U.textIn(ui::Face::SansBold, U.u(13.f), s.setting.empty() ? "A STORY" : s.setting, Rectangle{ix, iy, iw, U.u(13.f)}, U.th.accent, ui::Left, 0.22f);
        iy += U.u(30.f);
        U.paragraph(ui::Face::Serif, U.u(44.f), s.title, ix, iy, iw, U.th.ink, 1.05f);
        iy += titleH + U.u(10.f);
        U.paragraph(ui::Face::SerifItalic, U.u(22.f), s.tagline, ix, iy, iw, U.th.inkDim, 1.3f);
        iy += tagH + U.u(18.f);
        const auto& ch = storyProgress_.chapters[s.id];
        const auto& en = storyProgress_.endings[s.id];
        char b2[96];
        std::snprintf(b2, sizeof b2, "%d of %d chapters  \xc2\xb7  %d of %d endings", (int)ch.size(), (int)s.chapters.size(), (int)en.size(), (int)s.endings.size());
        U.textIn(ui::Face::Sans, U.u(15.f), b2, Rectangle{ix, iy, iw, U.u(15.f)}, U.th.inkFaint);
        iy += U.u(38.f);
        // chapters reached: start from any of them
        U.sectionLabel("CHAPTERS", ix, iy, iw);
        iy += U.u(28.f);
        for (size_t c = 0; c < s.chapters.size(); ++c) {
            bool reached = StoryProgress::has(ch, s.chapters[c].id) || c == 0;
            Rectangle cr{ix, iy, iw, U.u(30.f)};
            bool cover = reached && CheckCollisionPointRec(U.in.mouse, cr);
            char nb[16]; std::snprintf(nb, sizeof nb, "%d", (int)c + 1);
            U.text(ui::Face::SansBold, U.u(13.f), nb, ix, iy + U.u(7.f), reached ? U.th.accent : U.th.inkFaint);
            U.textIn(ui::Face::Serif, U.u(21.f), reached ? s.chapters[c].title : "\xe2\x80\x94", Rectangle{ix + U.u(28.f), iy + U.u(2.f), iw - U.u(28.f), U.u(21.f)},
                     cover ? U.th.ink : (reached ? U.th.inkDim : U.th.inkFaint));
            if (cover && U.in.clicked && !switching_) { if (ui.onSound) ui.onSound(1); beginStory(&s, s.chapters[c].start); }
            iy += U.u(32.f);
            if (iy > r.y + r.height - U.u(140.f)) break;
        }
        float by = r.y + r.height - U.u(84.f);
        bool canCont = storySaves_[0].used && storySaves_[0].story == s.id;
        if (U.button("st#go" + std::to_string(i), canCont ? "Continue" : "Begin", Rectangle{ix, by, iw * 0.48f, U.u(50.f)}, true) && !switching_) {
            if (canCont) beginStory(&s, storySaves_[0].node, &storySaves_[0].vars, storySaves_[0].line);
            else beginStory(&s, s.chapters[0].start);
        }
        if (U.button("st#new" + std::to_string(i), canCont ? "From the start" : "Gallery", Rectangle{ix + iw * 0.52f, by, iw * 0.48f, U.u(50.f)}) && !switching_) {
            if (canCont) beginStory(&s, s.chapters[0].start);
            else { gallerySel_ = (int)i; storyOverlay_ = 4; }
        }
    }
    // under the cards (tall windows), never below the screen's bottom margin
    float backY = std::min(U.H - U.u(110.f), y + ch + U.u(50.f));
    if (U.button("st#back", "Back", Rectangle{x, backY, U.u(170.f), U.u(50.f)}) || (U.in.back && !switching_ && storyOverlay_ == 0)) go(Screen::Menu, 0.4f);
    if (U.button("st#load", "Load a save", Rectangle{x + U.u(190.f), backY, U.u(210.f), U.u(50.f)})) storyOverlay_ = 3;
    U.alpha = 1.f;
    if (storyOverlay_ == 3) drawStorySlots(false);
    if (storyOverlay_ == 4) drawGallery();
}

void Game::drawStoryLog() {
    ui::Ui& U = ui;
    U.backdrop(0.9f);
    float pw = std::min(U.u(1100.f), U.W - U.u(60.f)), ph = U.H - U.u(160.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.u(80.f), pw, ph};
    U.panel(r, 26.f, 0.62f);
    U.text(ui::Face::Serif, U.u(46.f), "So far", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    U.text(ui::Face::Sans, U.u(15.f), "Scroll to read  \xc2\xb7  Esc to close", r.x + r.width - U.u(48.f), r.y + U.u(48.f), U.th.inkFaint, ui::Right);
    storyLogScroll_ = std::max(0.f, storyLogScroll_ + U.in.wheel * U.u(60.f));
    float y = r.y + r.height - U.u(60.f) + storyLogScroll_;
    BeginScissorMode((int)r.x, (int)(r.y + U.u(100.f)), (int)r.width, (int)(r.height - U.u(130.f)));
    for (int i = (int)story_.history.size() - 1; i >= 0 && y > r.y; --i) {
        const StoryPlayer::Past& p = story_.history[(size_t)i];
        bool narr0 = p.who.empty() || p.who == "narrator";
        float lineH = U.measureParagraph(narr0 ? ui::Face::SerifItalic : ui::Face::Serif, U.u(21.f), p.text, r.width - U.u(210.f), 1.3f) + U.u(12.f);
        y -= lineH;
        if (y > r.y + r.height) continue;
        if (y < r.y + U.u(100.f)) break;   // only whole entries inside the scrolled area
        bool narr = p.who.empty() || p.who == "narrator";
        std::string who = p.who == "vivian" ? "Vivian" : (p.who == "marcus" ? "Marcus" : (p.who == "you" ? "You" : ""));
        Color c = p.who == "vivian" ? U.th.rose : (p.who == "marcus" ? U.th.gold : U.th.accent);
        if (!who.empty()) U.text(ui::Face::SansBold, U.u(14.f), who, r.x + U.u(48.f), y + U.u(5.f), c);
        U.paragraph(narr ? ui::Face::SerifItalic : ui::Face::Serif, U.u(21.f), p.text, r.x + U.u(150.f), y, r.width - U.u(210.f), narr ? U.th.inkDim : U.th.ink, 1.3f);
    }
    EndScissorMode();
    if (U.in.back || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_H)) { storyOverlay_ = 0; storyLogScroll_ = 0.f; }
}

void Game::drawStorySlots(bool saving) {
    ui::Ui& U = ui;
    U.backdrop(0.9f);
    ui::ScaleScope screenFit(U, U.fitScale(980.f, 760.f, 24.f));
    float pw = U.u(980.f), ph = U.u(760.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.62f);
    ui::ClipScope clip(U, r, saving ? "save slots" : "load slots");
    U.text(ui::Face::Serif, U.u(50.f), saving ? "Save" : "Load", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    float y = r.y + U.u(120.f);
    int first = saving ? 1 : 0, count = std::max(1, (int)storySaves_.size() - first);
    // every slot fits between the title and the close button
    float pitch = std::min(U.u(80.f), (r.height - U.u(120.f) - U.u(100.f)) / (float)count);
    for (int i = first; i < (int)storySaves_.size(); ++i) {
        StorySave& sv = storySaves_[(size_t)i];
        Rectangle sr{r.x + U.u(48.f), y, r.width - U.u(96.f), pitch - U.u(10.f)};
        bool usable = saving || sv.used;
        bool over = usable && CheckCollisionPointRec(U.in.mouse, sr);
        float h = U.anim("slot#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
        ui::Box b; b.radius = U.u(16.f); b.fill = Color{255, 236, 224, (unsigned char)(8 + 18 * h)};
        b.border = std::max(1.f, U.u(1.f)); b.borderCol = ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.8f), h);
        U.box(sr, b);
        std::string label = i == 0 ? "Autosave" : "Slot " + std::to_string(i);
        float sk = sr.height / U.u(68.f);   // rows squeezed to fit: their type follows
        U.text(ui::Face::SansBold, U.u(14.f) * std::max(0.8f, sk), label, sr.x + U.u(24.f), sr.y + U.u(12.f) * sk, U.th.accent, ui::Left, 0.12f);
        if (sv.used) {
            const Story* st = storyById(sv.story);
            std::string desc = (st ? st->title : sv.story) + (sv.chapterTitle.empty() ? "" : "  \xc2\xb7  " + sv.chapterTitle);
            float whenW = U.measure(ui::Face::Sans, U.u(14.f), sv.when).x;
            U.textIn(ui::Face::Serif, U.u(23.f) * std::max(0.8f, sk), desc, Rectangle{sr.x + U.u(24.f), sr.y + U.u(32.f) * sk, sr.width - U.u(72.f) - whenW, U.u(23.f) * sk},
                     U.th.ink);
            U.text(ui::Face::Sans, U.u(14.f), sv.when, sr.x + sr.width - U.u(24.f), sr.y + (sr.height - U.u(14.f)) * 0.5f, U.th.inkFaint, ui::Right);
        } else U.text(ui::Face::SerifItalic, U.u(22.f) * std::max(0.8f, sk), "Empty", sr.x + U.u(24.f), sr.y + U.u(32.f) * sk, U.th.inkFaint);
        if (over && U.in.clicked) {
            if (ui.onSound) ui.onSound(1);
            if (saving && story_.active()) {
                sv.used = true; sv.story = story_.story()->id; sv.node = story_.node()->id; sv.line = story_.lineIndex(); sv.vars = story_.vars;
                int ci = story_.story()->chapterIndex(story_.node()->chapter);
                sv.chapterTitle = ci >= 0 ? story_.story()->chapters[(size_t)ci].title : "";
                std::time_t t = std::time(nullptr);
                char tb[32]; std::strftime(tb, sizeof tb, "%d %b %H:%M", std::localtime(&t));
                sv.when = tb;
                if (!harness) saveStorySaves(storySavesPath_, storySaves_);
                toasts_.push_back({0.f, "Saved"});
                storyOverlay_ = 0;
            } else if (!saving && sv.used) {
                const Story* st = storyById(sv.story);
                storyOverlay_ = 0;
                if (st) beginStory(st, sv.node, &sv.vars, sv.line);
            }
        }
        y += pitch;
    }
    if (U.button("slot#close", "Close", Rectangle{r.x + r.width - U.u(200.f), r.y + r.height - U.u(80.f), U.u(160.f), U.u(48.f)}) || U.in.back) storyOverlay_ = 0;
}

void Game::drawStoryMenu() {
    ui::Ui& U = ui;
    U.backdrop(0.85f);
    ui::ScaleScope screenFit(U, U.fitScale(520.f, 560.f, 24.f));
    float pw = U.u(520.f), ph = U.u(560.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.6f);
    ui::ClipScope clip(U, r, "story menu");
    U.text(ui::Face::Serif, U.u(56.f), "Paused", U.W * 0.5f, r.y + U.u(40.f), U.th.ink, ui::Center);
    float y = r.y + U.u(140.f), x = r.x + U.u(70.f), w = pw - U.u(140.f);
    if (U.menuItem("sm_res", "Resume", x, y, w) || (U.in.back && screenTime_ > 0.2f)) storyOverlay_ = 0;
    y += U.u(70.f);
    if (U.menuItem("sm_save", "Save", x, y, w)) storyOverlay_ = 2;
    y += U.u(70.f);
    if (U.menuItem("sm_load", "Load", x, y, w)) storyOverlay_ = 3;
    y += U.u(70.f);
    if (U.menuItem("sm_set", "Settings", x, y, w)) { storyOverlay_ = 0; settingsReturn_ = Screen::Story; go(Screen::Settings, 0.2f); }
    y += U.u(70.f);
    if (U.menuItem("sm_quit", "Back to stories", x, y, w)) { storyAutosave(); storyOverlay_ = 0; story_.stop(); go(Screen::Stories, 0.5f); }
}

void Game::drawGallery() {
    ui::Ui& U = ui;
    U.backdrop(0.9f);
    ui::ScaleScope screenFit(U, U.fitScale(980.f, 680.f, 24.f));
    float pw = U.u(980.f), ph = U.u(680.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.62f);
    ui::ClipScope clip(U, r, "gallery");
    const Story* s = gallerySel_ >= 0 && gallerySel_ < (int)stories_.size() ? &stories_[(size_t)gallerySel_] : nullptr;
    U.text(ui::Face::Serif, U.u(50.f), "Gallery", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    if (s) U.textIn(ui::Face::SerifItalic, U.u(24.f), s->title, Rectangle{r.x + U.u(260.f), r.y + U.u(50.f), r.width - U.u(308.f), U.u(24.f)}, U.th.inkDim);
    float y = r.y + U.u(120.f);
    if (s) {
        const auto& un = storyProgress_.gallery[s->id];
        float pitch = std::min(U.u(66.f), (r.height - U.u(120.f) - U.u(100.f)) / (float)std::max<size_t>(1, s->gallery.size()));
        for (auto& g : s->gallery) {
            bool open = StoryProgress::has(un, g.first);
            Rectangle gr{r.x + U.u(48.f), y, r.width - U.u(96.f), pitch - U.u(10.f)};
            bool over = open && CheckCollisionPointRec(U.in.mouse, gr);
            ui::Box b; b.radius = U.u(14.f); b.fill = Color{255, 236, 224, (unsigned char)(over ? 26 : 8)};
            b.border = std::max(1.f, U.u(1.f)); b.borderCol = over ? ui::withA(U.th.accent, 0.8f) : U.th.hairline;
            U.box(gr, b);
            U.textIn(ui::Face::Serif, U.u(23.f), open ? g.second : "Not yet found", Rectangle{gr.x + U.u(24.f), gr.y, gr.width - U.u(48.f), gr.height},
                     open ? U.th.ink : U.th.inkFaint);
            if (over && U.in.clicked) {
                // replay from the node that unlocked it
                for (auto& kv : s->nodes) if (kv.second.gallery == g.first) { storyOverlay_ = 0; beginStory(s, kv.first); break; }
            }
            y += pitch;
        }
        if (s->gallery.empty()) U.text(ui::Face::Sans, U.u(18.f), "This story has no gallery scenes.", r.x + U.u(48.f), y, U.th.inkFaint);
    }
    if (U.button("gal#close", "Close", Rectangle{r.x + r.width - U.u(200.f), r.y + r.height - U.u(80.f), U.u(160.f), U.u(48.f)}) || U.in.back) storyOverlay_ = 0;
}

}  // namespace ag
