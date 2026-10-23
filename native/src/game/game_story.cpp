#include "game/game.h"
#include "core/log.h"
#include "core/paths.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace ag {

namespace {
float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
// first n UTF-8 characters of s
std::string u8prefix(const std::string& s, int n) {
    if (n <= 0) return {};
    size_t i = 0;
    int c = 0;
    while (i < s.size()) {
        unsigned char ch = (unsigned char)s[i];
        if ((ch & 0xC0) != 0x80) { if (c == n) break; ++c; }
        ++i;
    }
    return s.substr(0, i);
}
std::string titleCase(std::string s) {
    for (auto& ch : s) if (ch == '_') ch = ' ';
    bool up = true;
    for (auto& ch : s) { if (up && std::isalpha((unsigned char)ch)) { ch = (char)std::toupper((unsigned char)ch); up = false; } else if (ch == ' ') up = true; }
    return s;
}
}  // namespace

// ------------------------------------------------------------------ loading + progress
void Game::loadStories() {
    stories_.clear();
    std::string dir = paths::join(paths::assetRoot(), "story");
    for (const std::string& f : paths::listFilesRecursive(dir, {".json"})) {
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
    stage(Stage::Night);
    story_.start(s, node, vars, line);
    storyPlaying_ = false;
    storyOverlay_ = 0;
    storyCardT_ = 0.f;
    storyLastChapter_.clear();
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

// ------------------------------------------------------------------ staging a node
void Game::stageStoryNode() {
    const StoryNode* n = story_.node();
    if (!n) return;
    const Story* s = story_.story();
    // progress: chapter reached, gallery, endings
    if (!n->chapter.empty()) {
        StoryProgress::add(storyProgress_.chapters[s->id], n->chapter);
        if (n->chapter != storyLastChapter_) { storyLastChapter_ = n->chapter; storyCardT_ = 0.f; storyCardKind_ = 0; }
    }
    if (!n->gallery.empty()) StoryProgress::add(storyProgress_.gallery[s->id], n->gallery);
    if (n->ending) StoryProgress::add(storyProgress_.endings[s->id], n->id);
    if (!n->location.empty() && n->location != storyLocation_) {
        storyLocation_ = n->location;
        if (storyCardKind_ != 0 || storyCardT_ > 0.5f) { storyCardT_ = 0.f; storyCardKind_ = 1; }
    }
    if (!harness) storyProgress_.save(storyProgressPath_);
    storyAutosave();
    // lighting mood for the scene
    if (!n->mood.empty()) storyMood_ = n->mood == "candle" ? 1 : (n->mood == "day" ? 2 : 0);
    // who is on stage: shown AND (nude, or an outfit asset for that state exists - none ship yet)
    if (n->showSet) { storyShow_[0] = storyShow_[1] = false; for (auto& w : n->show) { if (w == "vivian") storyShow_[0] = true; if (w == "marcus") storyShow_[1] = true; } }
    for (auto& o : n->outfit) { if (o.first == "vivian") storyOutfit_[0] = o.second; if (o.first == "marcus") storyOutfit_[1] = o.second; }
    // the pose: a free-play act at a depth, or a clip family
    if (!n->act.empty()) {
        for (size_t i = 0; i < positions_.size(); ++i)
            if (positions_[i].id == n->act) { posIndex_ = -1; setPosition((int)i, true); break; }
        depth_ = depthTarget_ = dragDepth_ = n->actT;
    }
    if (!n->cam.empty()) storyCamera(n->cam, 1.6f);
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

bool Game::storyCharVisible(int who) const {
    const std::string& o = storyOutfit_[who];
    return storyShow_[who] && (o.empty() || o == "nude");   // dressed / partial: hidden until outfit assets land
}

// ------------------------------------------------------------------ update
void Game::updateStory(float dt) {
    if (!story_.active()) return;
    const StoryNode* n = story_.node();
    if (story_.nodeChanged) { story_.nodeChanged = false; stageStoryNode(); n = story_.node(); }
    storyCardT_ += dt;
    if (storyPlaying_) {
        // the night runs until the node's goal, then the story continues
        updatePlay(dt);
        presentPlay(dt);
        const std::string& g = n ? n->playGoal : std::string();
        bool done = night_.finished;
        if (g == "her") done = done || night_.herClimaxes > 0;
        else if (g.rfind("time:", 0) == 0) done = done || night_.elapsed >= (float)std::atof(g.c_str() + 5);
        if (done && (!night_.finished || afterT_ > 2.5f)) {
            storyPlaying_ = false;
            story_.vars["together"] = night_.together ? 1.f : 0.f;
            story_.vars["herClimaxes"] = (float)night_.herClimaxes;
            story_.vars["connection"] = night_.connection;
            if (n && !n->playNext.empty()) story_.jump(n->playNext);
        }
        return;
    }
    // reading: the scene keeps breathing in its pose, voices and faces follow the lines
    bool open = storyOverlay_ == 0 && !switching_;
    if (open) {
        if (IsKeyPressed(KEY_A)) story_.autoMode = !story_.autoMode;
        story_.skipMode = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || storySkipToggle_;
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_H)) storyOverlay_ = 1;
        if (IsKeyPressed(KEY_ESCAPE)) storyOverlay_ = 5;
        std::vector<int> vis = story_.visibleChoices();
        if (story_.atChoices()) {
            story_.skipMode = false;
            for (size_t i = 0; i < vis.size() && i < 4; ++i) if (IsKeyPressed(KEY_ONE + (int)i)) { story_.choose(vis[i]); if (ui.onSound) ui.onSound(1); }
        } else if (story_.atPlay()) {
            // hand over to free play
            storyPlaying_ = true;
            night_.begin((unsigned)(GetTime() * 1000.0) ^ 0x9e3779b9u);
            night_.connection = clamp01(0.45f + 0.06f * (story_.vars.count("trust") ? story_.vars["trust"] : 0.f));
            for (size_t i = 0; i < positions_.size(); ++i) if (n && positions_[i].id == n->act) { posIndex_ = -1; setPosition((int)i, true); }
            depth_ = depthTarget_ = dragDepth_ = 0.3f;
            camUser_ = false; afterT_ = 0.f; slowmo_ = 1.f; together_ = false;
            hudT_ = 0.f; hintsT_ = 0.f;
            toasts_.push_back({0.f, "Your turn"});
        } else if (!story_.atEnding()) {
            bool click = ui.in.clicked && !CheckCollisionPointRec(ui.in.mouse, storyControlsRect_);
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || click || ui.in.wheel < 0.f) story_.advance();
        }
        story_.update(dt);
    }
    if (story_.lineChanged) {
        story_.lineChanged = false;
        if (const StoryLine* l = story_.line()) {
            if (!l->voice.empty()) voice(l->voice, 0.8f, 1.f);
            if (!l->sfx.empty()) sfx(l->sfx, 0.6f);
            if (!l->cam.empty()) storyCamera(l->cam, 1.2f);
            for (auto& e : l->expr) storyExpr_[e.first] = e.second;
            if (l->who == "vivian") storySpeak_ = 0;
            else if (l->who == "marcus") storySpeak_ = 1;
        }
    }
    // stage presentation: poses breathe, faces follow the lines, she glances at him or at you
    presentStory(dt);
}

void Game::presentStory(float dt) {
    Character& v = scene.vivian;
    Character& m = scene.marcus;
    scene.showVivian = v.loaded() && storyCharVisible(0);
    scene.showMarcus = m.loaded() && storyCharVisible(1);
    // the current act's pose, held at the node's depth
    if (posIndex_ >= 0) {
        const PositionInfo& p = positions_[(size_t)posIndex_];
        idlePhase_ = std::fmod(idlePhase_ + dt / 9.f, 1.f);
        for (Character* c : {&v, &m}) if (c->loaded()) c->clearLayers();
        if (p.solo && v.loaded()) v.setLayer(0, "solo.idle", idlePhase_, 1.f);
        else for (Character* c : {&v, &m}) if (c->loaded()) c->setLayer(0, p.baseClip, p.depthDriven ? depth_ : idlePhase_, 1.f);
    }
    blinkT_ -= dt;
    if (blinkT_ <= 0.f) { blinkV_ = 1.f; blinkT_ = 2.f + 3.f * hash1(time_ * 3.7f); }
    blinkV_ += (0.f - blinkV_) * std::min(1.f, dt * 14.f);
    mouthT_ = std::max(0.f, mouthT_ - dt);
    float u = mouthDur_ > 0.f ? 1.f - mouthT_ / mouthDur_ : 1.f;
    float talk = 0.f;
    if (!story_.lineDone()) talk = 0.25f + 0.2f * std::sin(time_ * 19.f);   // lips move while the line is written
    auto expr = [&](const char* k, float def) { auto it = storyExpr_.find(k); return it == storyExpr_.end() ? def : it->second; };
    if (v.loaded()) {
        v.setMorph("blink", clamp01(blinkV_));
        v.setMorph("smile", clamp01(expr("smile", 0.25f)));
        v.setMorph("pleasure", clamp01(expr("pleasure", 0.1f)));
        float voiceOpen = mouthT_ > 0.f ? std::sin(3.14159f * std::min(1.f, u * 1.15f)) * 0.6f : 0.f;
        v.setMorph("moan", clamp01(expr("moan", 0.f) + voiceOpen + (storySpeak_ == 0 ? talk : 0.f)));
        v.setMorph("kiss", clamp01(expr("kiss", 0.f)));
        v.flush = clamp01(expr("flush", 0.15f));
        v.proc.lookWeight = 0.5f;
        v.proc.lookTarget = scene.showMarcus ? m.bonePos("頭") : cam_.eye;
        v.proc.breathRate = 0.22f; v.proc.breathAmp = 1.f;
    }
    if (m.loaded()) {
        m.setMorph("blink", clamp01(blinkV_ * 0.9f));
        m.setMorph("moan", clamp01(storySpeak_ == 1 ? talk * 0.6f : 0.f));
        m.proc.lookWeight = 0.45f;
        m.proc.lookTarget = v.loaded() ? v.bonePos("頭") : cam_.eye;
    }
    renderer.settings.flash = 0.f;
    scene.update(dt);
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
    float cw = (U.W - 2.f * x - U.u(2.f * 28.f)) / 3.f, ch = U.H - y - U.u(200.f);
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
        float ix = r.x + U.u(34.f), iw = r.width - U.u(68.f), iy = r.y + U.u(34.f);
        U.text(ui::Face::SansBold, U.u(13.f), s.setting.empty() ? "A STORY" : s.setting, ix, iy, U.th.accent, ui::Left, 0.22f);
        iy += U.u(30.f);
        U.paragraph(ui::Face::Serif, U.u(44.f), s.title, ix, iy, iw, U.th.ink, 1.05f);
        iy += titleH + U.u(10.f);
        U.paragraph(ui::Face::SerifItalic, U.u(22.f), s.tagline, ix, iy, iw, U.th.inkDim, 1.3f);
        iy += tagH + U.u(18.f);
        const auto& ch = storyProgress_.chapters[s.id];
        const auto& en = storyProgress_.endings[s.id];
        char b2[96];
        std::snprintf(b2, sizeof b2, "%d of %d chapters  \xc2\xb7  %d of %d endings", (int)ch.size(), (int)s.chapters.size(), (int)en.size(), (int)s.endings.size());
        U.text(ui::Face::Sans, U.u(15.f), b2, ix, iy, U.th.inkFaint);
        iy += U.u(38.f);
        // chapters reached: start from any of them
        U.sectionLabel("CHAPTERS", ix, iy, iw);
        iy += U.u(28.f);
        for (size_t c = 0; c < s.chapters.size(); ++c) {
            bool reached = StoryProgress::has(ch, s.chapters[c].id) || c == 0;
            Rectangle cr{ix, iy, iw, U.u(30.f)};
            bool cover = reached && CheckCollisionPointRec(U.in.mouse, cr);
            char nb[8]; std::snprintf(nb, sizeof nb, "%d", (int)c + 1);
            U.text(ui::Face::SansBold, U.u(13.f), nb, ix, iy + U.u(7.f), reached ? U.th.accent : U.th.inkFaint);
            U.text(ui::Face::Serif, U.u(21.f), reached ? s.chapters[c].title : "\xe2\x80\x94", ix + U.u(28.f), iy + U.u(2.f),
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
    if (U.button("st#back", "Back", Rectangle{x, U.H - U.u(110.f), U.u(170.f), U.u(50.f)}) || (U.in.back && !switching_ && storyOverlay_ == 0)) go(Screen::Menu, 0.4f);
    if (U.button("st#load", "Load a save", Rectangle{x + U.u(190.f), U.H - U.u(110.f), U.u(210.f), U.u(50.f)})) storyOverlay_ = 3;
    U.alpha = 1.f;
    if (storyOverlay_ == 3) drawStorySlots(false);
    if (storyOverlay_ == 4) drawGallery();
}

void Game::drawStory() {
    ui::Ui& U = ui;
    if (!story_.active()) return;
    if (storyPlaying_) {
        drawHud();
        const StoryNode* n = story_.node();
        if (n) {
            const char* g = n->playGoal == "her" ? "Bring her to her peak" : (n->playGoal == "him" ? "Let him have his moment" :
                            (n->playGoal.rfind("time:", 0) == 0 ? "Take your time together" : "Find each other - together"));
            U.alpha = 1.f;
            float w = U.measure(ui::Face::SansMedium, U.u(15.f), g).x + U.u(120.f);
            ui::Box b; b.radius = U.u(16.f); b.fill = Color{24, 14, 18, 170}; b.border = std::max(1.f, U.u(1.f)); b.borderCol = ui::withA(U.th.accent, 0.5f);
            U.box(Rectangle{U.W * 0.5f - w * 0.5f, U.u(74.f), w, U.u(34.f)}, b);
            U.text(ui::Face::SansBold, U.u(12.f), "STORY", U.W * 0.5f - w * 0.5f + U.u(18.f), U.u(84.f), U.th.accent, ui::Left, 0.22f);
            U.text(ui::Face::SansMedium, U.u(15.f), g, U.W * 0.5f - w * 0.5f + U.u(80.f), U.u(82.f), U.th.ink);
        }
        return;
    }
    const StoryNode* n = story_.node();
    const Story* s = story_.story();
    // chapter / location card
    if (storyCardT_ < 4.f && n) {
        float a = clamp01(storyCardT_ * 1.6f) * clamp01((4.f - storyCardT_) * 1.2f);
        U.alpha = a;
        // a soft dark band behind the card: it reads over the lamp as well as over the dark
        int ci = storyCardKind_ == 0 ? s->chapterIndex(n->chapter) : -1;
        if (ci >= 0 || !storyLocation_.empty()) {
            float top = U.H * 0.24f + (ci >= 0 ? -U.u(6.f) : U.u(24.f)), hgt = ci >= 0 ? U.u(160.f) : U.u(56.f);
            ui::Box sc; sc.radius = U.u(40.f); sc.shadow = U.u(200.f); sc.shadowCol = Color{8, 4, 6, 170};
            U.box(Rectangle{U.W * 0.5f - U.u(330.f), top, U.u(660.f), hgt}, sc);
        }
        if (storyCardKind_ == 0) {
            if (ci >= 0) {
                char b[32]; std::snprintf(b, sizeof b, "CHAPTER %d", ci + 1);
                U.text(ui::Face::SansBold, U.u(15.f), b, U.W * 0.5f, U.H * 0.24f, U.th.accent, ui::Center, 0.3f);
                U.text(ui::Face::Serif, U.u(68.f), s->chapters[(size_t)ci].title, U.W * 0.5f, U.H * 0.24f + U.u(30.f), U.th.ink, ui::Center);
            }
        }
        if (!storyLocation_.empty())
            U.text(ui::Face::SerifItalic, U.u(24.f), titleCase(storyLocation_), U.W * 0.5f, U.H * 0.24f + (storyCardKind_ == 0 ? U.u(118.f) : U.u(40.f)),
                   U.th.inkDim, ui::Center);
        U.alpha = 1.f;
    }
    // ending
    if (story_.atEnding()) {
        float a = clamp01(screenTime_);
        U.backdrop(0.8f);
        U.alpha = a;
        U.text(ui::Face::SansBold, U.u(15.f), "AN ENDING", U.W * 0.5f, U.H * 0.3f, U.th.accent, ui::Center, 0.3f);
        U.text(ui::Face::Serif, U.u(76.f), n->endTitle, U.W * 0.5f, U.H * 0.3f + U.u(34.f), U.th.ink, ui::Center);
        U.paragraph(ui::Face::SerifItalic, U.u(26.f), n->endText, U.W * 0.5f - U.u(420.f), U.H * 0.3f + U.u(150.f), U.u(840.f), U.th.inkDim, 1.35f, ui::Center);
        char b[64];
        std::snprintf(b, sizeof b, "%d of %d endings found", (int)storyProgress_.endings[s->id].size(), (int)s->endings.size());
        U.text(ui::Face::Sans, U.u(16.f), b, U.W * 0.5f, U.H * 0.3f + U.u(300.f), U.th.inkFaint, ui::Center);
        if (U.button("end#back", "Back to stories", Rectangle{U.W * 0.5f - U.u(130.f), U.H * 0.3f + U.u(350.f), U.u(260.f), U.u(54.f)}, true)) {
            story_.stop(); go(Screen::Stories, 0.6f);
        }
        U.alpha = 1.f;
        return;
    }
    // choices
    if (story_.atChoices()) {
        std::vector<int> vis = story_.visibleChoices();
        float cw = U.u(820.f), chh = U.u(64.f), gap = U.u(16.f);
        float total = (float)vis.size() * (chh + gap) - gap;
        float y = U.H * 0.5f - total * 0.5f - U.u(60.f);
        for (size_t i = 0; i < vis.size(); ++i) {
            const StoryChoice& c = n->choices[(size_t)vis[i]];
            Rectangle r{U.W * 0.5f - cw * 0.5f, y + (float)i * (chh + gap), cw, chh};
            bool over = CheckCollisionPointRec(U.in.mouse, r);
            float h = U.anim("ch#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
            ui::Box b; b.radius = U.u(20.f); b.glass = true; b.glassTint = U.th.glass; b.glassAmt = 0.62f;
            b.fill = ui::mixc(Color{255, 236, 224, 8}, ui::withA(U.th.accent, 0.18f), h);
            b.border = std::max(1.f, U.u(1.2f)); b.borderCol = ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.9f), h);
            b.shadow = U.u(18.f + 14.f * h); b.shadowCol = ui::withA(U.th.accent, 0.12f + 0.15f * h);
            U.box(Rectangle{r.x - h * U.u(4.f), r.y, r.width + h * U.u(8.f), r.height}, b);
            char k[4]; std::snprintf(k, sizeof k, "%d", (int)i + 1);
            U.text(ui::Face::SansBold, U.u(14.f), k, r.x + U.u(26.f), r.y + (chh - U.u(14.f)) * 0.5f, U.th.accent);
            U.text(ui::Face::Serif, U.u(25.f), c.text, r.x + U.u(58.f), r.y + (chh - U.u(25.f)) * 0.5f - U.u(3.f), ui::mixc(U.th.inkDim, U.th.ink, h));
            if (over && U.in.clicked && storyOverlay_ == 0) { story_.choose(vis[i]); if (ui.onSound) ui.onSound(1); }
        }
    }
    // the text box
    const StoryLine* l = story_.line();
    float bw = std::min(U.W - U.u(160.f), U.u(1500.f)), bh = U.u(230.f);
    Rectangle box{U.W * 0.5f - bw * 0.5f, U.H - bh - U.u(36.f), bw, bh};
    if (l || story_.atChoices()) {
        ui::Box b; b.radius = U.u(26.f); b.glass = true; b.glassTint = U.th.glass; b.glassAmt = 0.66f;
        b.fill = Color{255, 236, 224, 6}; b.border = std::max(1.f, U.u(1.f)); b.borderCol = U.th.hairline;
        b.shadow = U.u(36.f); b.shadowCol = Color{0, 0, 0, 110};
        U.box(box, b);
        const StoryLine* shown = l;
        if (!shown && !story_.history.empty()) { static StoryLine last; last.who = story_.history.back().who; last.text = story_.history.back().text; shown = &last; }
        if (shown) {
            bool narr = shown->who.empty() || shown->who == "narrator" || shown->who == "you";
            if (!narr) {
                std::string name = shown->who == "vivian" ? "Vivian" : (shown->who == "marcus" ? "Marcus" : titleCase(shown->who));
                Color c = shown->who == "vivian" ? U.th.rose : (shown->who == "marcus" ? U.th.gold : U.th.accent);
                float nw = U.measure(ui::Face::SerifBold, U.u(26.f), name).x + U.u(44.f);
                ui::Box nb; nb.radius = U.u(18.f); nb.fill = Color{20, 12, 16, 220}; nb.border = std::max(1.f, U.u(1.2f)); nb.borderCol = ui::withA(c, 0.85f);
                nb.shadow = U.u(14.f); nb.shadowCol = ui::withA(c, 0.25f);
                U.box(Rectangle{box.x + U.u(40.f), box.y - U.u(22.f), nw, U.u(44.f)}, nb);
                U.text(ui::Face::SerifBold, U.u(26.f), name, box.x + U.u(40.f) + nw * 0.5f, box.y - U.u(22.f) + U.u(7.f), c, ui::Center);
            }
            std::string txt = l ? u8prefix(shown->text, (int)story_.revealed()) : shown->text;
            if (shown->who == "you") txt = "\xe2\x80\x94 " + txt;
            U.paragraph(narr ? ui::Face::SerifItalic : ui::Face::Serif, U.u(30.f), txt, box.x + U.u(56.f), box.y + U.u(42.f), box.width - U.u(112.f),
                        narr ? U.th.inkDim : U.th.ink, 1.35f);
            if (l && story_.lineDone() && !story_.atChoices()) {
                float bl = 0.5f + 0.5f * std::sin(time_ * 4.f);
                U.diamond(box.x + box.width - U.u(46.f), box.y + box.height - U.u(36.f), U.u(6.f), ui::withA(U.th.accent, 0.4f + 0.6f * bl));
            }
        }
        // controls
        struct C { const char* label; bool on; int id; };
        C cs[6] = {{"Auto", story_.autoMode, 0}, {"Skip", storySkipToggle_, 1}, {"Log", false, 2}, {"Save", false, 3}, {"Load", false, 4}, {"Menu", false, 5}};
        float cx = box.x + box.width - U.u(40.f), cy = box.y + U.u(16.f);
        float minX = cx;
        for (int i = 5; i >= 0; --i) {
            float tw = U.measure(ui::Face::SansBold, U.u(13.f), cs[i].label, 0.18f).x;
            Rectangle r{cx - tw - U.u(10.f), cy - U.u(6.f), tw + U.u(20.f), U.u(28.f)};
            bool over = CheckCollisionPointRec(U.in.mouse, r);
            U.text(ui::Face::SansBold, U.u(13.f), cs[i].label, cx, cy, cs[i].on ? U.th.accent : (over ? U.th.ink : U.th.inkFaint), ui::Right, 0.18f);
            if (over && U.in.clicked && storyOverlay_ == 0) {
                if (ui.onSound) ui.onSound(1);
                if (cs[i].id == 0) story_.autoMode = !story_.autoMode;
                else if (cs[i].id == 1) storySkipToggle_ = !storySkipToggle_;
                else if (cs[i].id == 2) storyOverlay_ = 1;
                else if (cs[i].id == 3) storyOverlay_ = 2;
                else if (cs[i].id == 4) storyOverlay_ = 3;
                else storyOverlay_ = 5;
            }
            cx -= tw + U.u(30.f);
            minX = r.x;
        }
        storyControlsRect_ = Rectangle{minX, cy - U.u(8.f), box.x + box.width - minX, U.u(32.f)};
    }
    // overlays
    if (storyOverlay_ == 1) drawStoryLog();
    else if (storyOverlay_ == 2) drawStorySlots(true);
    else if (storyOverlay_ == 3) drawStorySlots(false);
    else if (storyOverlay_ == 5) drawStoryMenu();
}

void Game::drawStoryLog() {
    ui::Ui& U = ui;
    U.backdrop(0.9f);
    float pw = U.u(1100.f), ph = U.H - U.u(160.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.u(80.f), pw, ph};
    U.panel(r, 26.f, 0.62f);
    U.text(ui::Face::Serif, U.u(46.f), "So far", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    U.text(ui::Face::Sans, U.u(15.f), "Scroll to read  \xc2\xb7  Esc to close", r.x + r.width - U.u(48.f), r.y + U.u(48.f), U.th.inkFaint, ui::Right);
    storyLogScroll_ = std::max(0.f, storyLogScroll_ + U.in.wheel * U.u(60.f));
    float y = r.y + r.height - U.u(60.f) + storyLogScroll_;
    BeginScissorMode((int)r.x, (int)(r.y + U.u(100.f)), (int)r.width, (int)(r.height - U.u(130.f)));
    for (int i = (int)story_.history.size() - 1; i >= 0 && y > r.y; --i) {
        const StoryPlayer::Past& p = story_.history[(size_t)i];
        float lineH = U.u(30.f) * (1.f + (float)p.text.size() / 70.f);
        y -= lineH;
        if (y > r.y + r.height) continue;
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
    float pw = U.u(980.f), ph = U.u(720.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.62f);
    U.text(ui::Face::Serif, U.u(50.f), saving ? "Save" : "Load", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    float y = r.y + U.u(120.f);
    for (int i = saving ? 1 : 0; i < (int)storySaves_.size(); ++i) {
        StorySave& sv = storySaves_[(size_t)i];
        Rectangle sr{r.x + U.u(48.f), y, r.width - U.u(96.f), U.u(68.f)};
        bool usable = saving || sv.used;
        bool over = usable && CheckCollisionPointRec(U.in.mouse, sr);
        float h = U.anim("slot#" + std::to_string(i), over ? 1.f : 0.f, 14.f);
        ui::Box b; b.radius = U.u(16.f); b.fill = Color{255, 236, 224, (unsigned char)(8 + 18 * h)};
        b.border = std::max(1.f, U.u(1.f)); b.borderCol = ui::mixc(U.th.hairline, ui::withA(U.th.accent, 0.8f), h);
        U.box(sr, b);
        std::string label = i == 0 ? "Autosave" : "Slot " + std::to_string(i);
        U.text(ui::Face::SansBold, U.u(14.f), label, sr.x + U.u(24.f), sr.y + U.u(12.f), U.th.accent, ui::Left, 0.12f);
        if (sv.used) {
            const Story* st = storyById(sv.story);
            std::string desc = (st ? st->title : sv.story) + (sv.chapterTitle.empty() ? "" : "  \xc2\xb7  " + sv.chapterTitle);
            U.text(ui::Face::Serif, U.u(23.f), desc, sr.x + U.u(24.f), sr.y + U.u(32.f), U.th.ink);
            U.text(ui::Face::Sans, U.u(14.f), sv.when, sr.x + sr.width - U.u(24.f), sr.y + U.u(26.f), U.th.inkFaint, ui::Right);
        } else U.text(ui::Face::SerifItalic, U.u(22.f), "Empty", sr.x + U.u(24.f), sr.y + U.u(32.f), U.th.inkFaint);
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
        y += U.u(80.f);
    }
    if (U.button("slot#close", "Close", Rectangle{r.x + r.width - U.u(200.f), r.y + r.height - U.u(80.f), U.u(160.f), U.u(48.f)}) || U.in.back) storyOverlay_ = 0;
}

void Game::drawStoryMenu() {
    ui::Ui& U = ui;
    U.backdrop(0.85f);
    float pw = U.u(520.f), ph = U.u(560.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.6f);
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
    float pw = U.u(980.f), ph = U.u(680.f);
    Rectangle r{U.W * 0.5f - pw * 0.5f, U.H * 0.5f - ph * 0.5f, pw, ph};
    U.panel(r, 26.f, 0.62f);
    const Story* s = gallerySel_ >= 0 && gallerySel_ < (int)stories_.size() ? &stories_[(size_t)gallerySel_] : nullptr;
    U.text(ui::Face::Serif, U.u(50.f), "Gallery", r.x + U.u(48.f), r.y + U.u(32.f), U.th.ink);
    if (s) U.text(ui::Face::SerifItalic, U.u(24.f), s->title, r.x + U.u(260.f), r.y + U.u(50.f), U.th.inkDim);
    float y = r.y + U.u(120.f);
    if (s) {
        const auto& un = storyProgress_.gallery[s->id];
        for (auto& g : s->gallery) {
            bool open = StoryProgress::has(un, g.first);
            Rectangle gr{r.x + U.u(48.f), y, r.width - U.u(96.f), U.u(56.f)};
            bool over = open && CheckCollisionPointRec(U.in.mouse, gr);
            ui::Box b; b.radius = U.u(14.f); b.fill = Color{255, 236, 224, (unsigned char)(over ? 26 : 8)};
            b.border = std::max(1.f, U.u(1.f)); b.borderCol = over ? ui::withA(U.th.accent, 0.8f) : U.th.hairline;
            U.box(gr, b);
            U.text(ui::Face::Serif, U.u(23.f), open ? g.second : "Not yet found", gr.x + U.u(24.f), gr.y + U.u(14.f), open ? U.th.ink : U.th.inkFaint);
            if (over && U.in.clicked) {
                // replay from the node that unlocked it
                for (auto& kv : s->nodes) if (kv.second.gallery == g.first) { storyOverlay_ = 0; beginStory(s, kv.first); break; }
            }
            y += U.u(66.f);
        }
        if (s->gallery.empty()) U.text(ui::Face::Sans, U.u(18.f), "This story has no gallery scenes.", r.x + U.u(48.f), y, U.th.inkFaint);
    }
    if (U.button("gal#close", "Close", Rectangle{r.x + r.width - U.u(200.f), r.y + r.height - U.u(80.f), U.u(160.f), U.u(48.f)}) || U.in.back) storyOverlay_ = 0;
}

}  // namespace ag
